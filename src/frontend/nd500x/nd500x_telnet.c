/*
 * nd500x_telnet.c - single-client TCP/telnet terminal host for the SINTRAN shell.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The telnet IAC state machine, negotiation options and the accept/reader
 * threading model are reused from nd100x (src/ndlib/telnetserver.c). This
 * version is deliberately single-terminal: one listening socket, one client,
 * bridged to one ConsoleIO - which is all the nd500x shell needs.
 *
 * Sockets go through net_compat.h so the same code builds on POSIX and Winsock:
 * descriptors are nd_socket_t, close is nd_socket_close(), and the error to
 * test after a failed call comes from nd_last_socket_error() rather than errno,
 * because Winsock does not report through errno at all.
 */

#include "nd500x_telnet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "net_compat.h"

/* Telnet protocol (RFC 854/855) - same constants as nd100x telnetserver.c. */
#define IAC  255
#define WILL 251
#define WONT 252
#define DO   253
#define DONT 254
#define SB   250
#define SE   240
#define OPT_ECHO 1
#define OPT_SGA  3   /* suppress go-ahead */

/* IAC state machine states (from nd100x). */
typedef enum {
    T_DATA = 0, T_IAC, T_WILL, T_WONT, T_DO, T_DONT, T_SB, T_SB_DATA, T_SB_IAC
} IacState;

#define INBUF_SIZE 4096

static nd_socket_t g_listen_fd = ND_INVALID_SOCKET;
static nd_socket_t g_client_fd = ND_INVALID_SOCKET;
static volatile int g_connected = 0;
static volatile int g_shutdown = 0;
static pthread_t g_accept_thread;

/* Input ring buffer: bytes decoded from the client (IAC stripped). */
static unsigned char g_in[INBUF_SIZE];
static int g_in_head = 0, g_in_tail = 0;   /* head=write, tail=read */
static pthread_mutex_t g_in_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_in_cond = PTHREAD_COND_INITIALIZER;

static void in_push(unsigned char b) {
    pthread_mutex_lock(&g_in_mtx);
    int next = (g_in_head + 1) % INBUF_SIZE;
    if (next != g_in_tail) {           /* drop on overflow */
        g_in[g_in_head] = b;
        g_in_head = next;
    }
    pthread_cond_signal(&g_in_cond);
    pthread_mutex_unlock(&g_in_mtx);
}

/* Pop one byte, or -1 if empty (non-blocking). */
static int in_pop(void) {
    int r = -1;
    pthread_mutex_lock(&g_in_mtx);
    if (g_in_tail != g_in_head) {
        r = g_in[g_in_tail];
        g_in_tail = (g_in_tail + 1) % INBUF_SIZE;
    }
    pthread_mutex_unlock(&g_in_mtx);
    return r;
}

/* Return the next queued byte without consuming it, or -1 if the ring is empty.
 * Used by the async ESCAPE user-break poll; a following in_pop() returns the
 * same byte, so a non-break char is left intact for the program. */
static int in_peek(void) {
    int r = -1;
    pthread_mutex_lock(&g_in_mtx);
    if (g_in_tail != g_in_head) {
        r = g_in[g_in_tail];
    }
    pthread_mutex_unlock(&g_in_mtx);
    return r;
}

static int in_available(void) {
    pthread_mutex_lock(&g_in_mtx);
    int a = (g_in_tail != g_in_head);
    pthread_mutex_unlock(&g_in_mtx);
    return a;
}

/* Block until a byte is available or the client goes away. 1 = have input. */
static int in_wait(void) {
    pthread_mutex_lock(&g_in_mtx);
    while (g_in_tail == g_in_head && g_connected && !g_shutdown) {
        pthread_cond_wait(&g_in_cond, &g_in_mtx);
    }
    int a = (g_in_tail != g_in_head);
    pthread_mutex_unlock(&g_in_mtx);
    return a;
}

/* Byte-exact TX trace, gated by env ND500X_TELNET_TXLOG. Logs every byte we
 * hand to send() as hex to stderr, so it can be diffed against the terminal
 * client's RECEIVE log to prove (or rule out) transport byte loss. */
static void txlog(const unsigned char* buf, int len) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("ND500X_TELNET_TXLOG"); on = (e && e[0] && e[0] != '0'); }
    if (!on) return;
    for (int i = 0; i < len; i++) fprintf(stderr, "%02X ", buf[i]);
    fflush(stderr);
}

/* Raw send to the client. Returns bytes actually sent (== len unless the peer
 * closed mid-write - which WOULD be real loss, now visible in the return). */
static int sock_send(const unsigned char* buf, int len) {
    nd_socket_t fd = g_client_fd;
    if (fd < 0) return 0;
    txlog(buf, len);
    int off = 0;
    while (off < len) {
        nd_ssize_t n = send(ND_SOCK_NATIVE(fd), ND_SOCK_BUF(buf + off),
                            ND_SOCK_LEN(len - off), MSG_NOSIGNAL);
        if (n <= 0) { if (nd_last_socket_error() == ND_EINTR) continue; break; }
        off += (int)n;
    }
    if (off < len)
        fprintf(stderr, "\n[telnet] SHORT SEND: %d of %d bytes (client gone?) - REAL byte loss\n", off, len);
    return off;
}

/* Reader thread: accept a client, negotiate, then decode bytes into the ring. */
static void* accept_thread_fn(void* arg) {
    (void)arg;
    while (!g_shutdown) {
        struct sockaddr_in cli; nd_socklen_t clen = sizeof cli;
        nd_socket_t fd = (nd_socket_t)accept(ND_SOCK_NATIVE(g_listen_fd),
                                             (struct sockaddr*)&cli, &clen);
        if (fd < 0) { if (g_shutdown) break; continue; }

        int one = 1;
        setsockopt(ND_SOCK_NATIVE(fd), IPPROTO_TCP, TCP_NODELAY,
                   ND_SOCKOPT(&one), sizeof one);

        /* Put the client in character-at-a-time mode with server echo:
         * IAC WILL ECHO, IAC WILL SGA, IAC DONT LINEMODE (same as nd100x). */
        unsigned char nego[] = {
            IAC, WILL, OPT_ECHO,
            IAC, WILL, OPT_SGA,
            IAC, DONT, 34 /* LINEMODE */
        };
        g_client_fd = fd;
        sock_send(nego, sizeof nego);

        fprintf(stderr, "[telnet] client connected from %s:%d\n",
                inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        pthread_mutex_lock(&g_in_mtx);
        g_connected = 1;
        g_in_head = g_in_tail = 0;
        pthread_cond_signal(&g_in_cond);
        pthread_mutex_unlock(&g_in_mtx);

        IacState st = T_DATA;
        unsigned char buf[1024];
        for (;;) {
            nd_ssize_t n = recv(ND_SOCK_NATIVE(fd), ND_SOCK_BUF(buf),
                                ND_SOCK_LEN(sizeof buf), 0);
            if (n <= 0) {
                if (n < 0 && nd_last_socket_error() == ND_EINTR) continue;
                break;
            }
            for (int i = 0; i < n; i++) {
                unsigned char b = buf[i];
                switch (st) {
                case T_DATA:
                    if (b == IAC) st = T_IAC;
                    else in_push(b);
                    break;
                case T_IAC:
                    switch (b) {
                    case WILL: st = T_WILL; break;
                    case WONT: st = T_WONT; break;
                    case DO:   st = T_DO;   break;
                    case DONT: st = T_DONT; break;
                    case SB:   st = T_SB;   break;
                    case IAC:  in_push(IAC); st = T_DATA; break;  /* escaped 255 */
                    default:   st = T_DATA; break;
                    }
                    break;
                case T_WILL: case T_WONT: case T_DO: case T_DONT:
                    st = T_DATA; break;             /* ignore the option byte */
                case T_SB:      st = T_SB_DATA; break;
                case T_SB_DATA: if (b == IAC) st = T_SB_IAC; break;
                case T_SB_IAC:  st = (b == SE) ? T_DATA : T_SB_DATA; break;
                }
            }
        }

        pthread_mutex_lock(&g_in_mtx);
        g_connected = 0;
        g_client_fd = ND_INVALID_SOCKET;
        pthread_cond_signal(&g_in_cond);
        pthread_mutex_unlock(&g_in_mtx);
        nd_socket_close(fd);
        fprintf(stderr, "[telnet] client disconnected\n");
    }
    return NULL;
}

/* ------------------------------------------------------------ ConsoleIO --- */

static int g_last_out = 0;

static int tel_read_char(void* ctx) {
    (void)ctx;
    /* BLOCK until a byte arrives, mirroring the stdio console's raw VMIN=1
     * read. A char-at-a-time line read (503B DVINST reads until the CR break)
     * calls read_char repeatedly; if this returned -1 on an empty ring during
     * a human's pause between keystrokes, the caller would treat it as EOF and
     * return a PARTIAL line (every telnet command truncated at its first char).
     * in_wait() is race-free (checks the ring under the lock before waiting)
     * and returns 0 on disconnect/shutdown -> report EOF so readers unwind. */
    if (!in_wait()) return -1;   /* EOF: client gone */
    return in_pop();
}
static bool tel_char_avail(void* ctx) { (void)ctx; return in_available() != 0; }
static int tel_wait_input(void* ctx) { (void)ctx; return in_wait(); }
static int tel_peek_char(void* ctx) { (void)ctx; return in_peek(); }

static void tel_write_char(void* ctx, int ch) {
    (void)ctx;
    unsigned char c = (unsigned char)ch;
    /* Drop NUL (0x00): VT100/SINTRAN fill/timing padding with no display
     * meaning. Some emulators advance the cursor on NUL, shifting the whole
     * screen one column. Real terminals ignore it; so do we. */
    if (c == 0x00) return;
    /* Expand a bare LF to CR LF for the remote terminal (mirrors the stdio
     * console's dedup so we do not double CRs). */
    if (c == '\n' && g_last_out != '\r') {
        unsigned char crlf[2] = { '\r', '\n' };
        sock_send(crlf, 2);
    } else {
        sock_send(&c, 1);
    }
    g_last_out = c;
}

static ConsoleIO g_tel_console = {
    .read_char = tel_read_char,
    .write_char = tel_write_char,
    .char_available = tel_char_avail,
    .context = NULL,
    .user_break = NULL,
    .wait_for_input = tel_wait_input,
    .peek_char = tel_peek_char,
};

ConsoleIO* nd500x_telnet_console(void) { return &g_tel_console; }

/* ------------------------------------------------------------- line I/O --- */

void nd500x_telnet_write(const char* s) {
    for (const char* p = s; *p; p++) tel_write_char(NULL, (unsigned char)*p);
}

int nd500x_telnet_readline(char* out, int len) {
    /* Drop an LF that pairs with the CR we just returned on (CR LF from the
     * client is ONE Enter); persists across calls since each call is one line. */
    static int swallow_lf = 0;
    int n = 0;
    out[0] = '\0';
    for (;;) {
        if (!in_wait()) return -1;             /* disconnected */
        int c = in_pop();
        if (c < 0) continue;
        if (c == 0) continue;                  /* CR NUL / padding: ignore the NUL */
        if (swallow_lf) {                      /* second half of a CR LF pair */
            swallow_lf = 0;
            if (c == '\n') continue;
        }
        if (c == 27) {                         /* ESC */
            /* An ESC that introduces a CSI/SS3 sequence (arrow/function keys:
             * ESC '[' ... or ESC 'O' ...) is NOT an abort - drain and ignore it.
             * The reader thread pushes a key's whole sequence at once, so the
             * introducer is already queued by the time we pop the ESC. */
            if (in_available()) {
                int nx = in_peek();
                if (nx == '[' || nx == 'O') {
                    in_pop();                  /* consume introducer */
                    int f;                     /* consume up to the final byte */
                    while (in_available() && (f = in_peek()) >= 0 &&
                           !(f >= 0x40 && f <= 0x7E)) in_pop();
                    if (in_available()) in_pop();  /* the final byte */
                    continue;
                }
            }
            /* Lone ESC: abort the current command line (SINTRAN user-break on the
             * command processor). Discard any typed input and give a fresh line. */
            nd500x_telnet_write("\r\nABORTED\r\n");
            out[0] = '\0';
            return 0;
        }
        if (c == '\r' || c == '\n') {          /* Enter: emit newline, return line */
            if (c == '\r') swallow_lf = 1;     /* a following LF is its pair */
            nd500x_telnet_write("\r\n");
            out[n] = '\0';
            return n;                          /* even n==0: empty Enter -> new line */
        }
        if (c == 8 || c == 127) {              /* backspace / DEL */
            if (n > 0) { n--; nd500x_telnet_write("\b \b"); }
            continue;
        }
        if (c >= 32 && n < len - 1) {
            out[n++] = (char)c;
            unsigned char ec = (unsigned char)c;  /* echo */
            sock_send(&ec, 1);
        }
    }
}

/* ------------------------------------------------------------- lifecycle -- */

int nd500x_telnet_start(int port) {
    /* No-op on POSIX; on Windows this is the WSAStartup that every other
     * socket call here depends on having run first. */
    if (nd_net_init() != 0) { fprintf(stderr, "[telnet] socket layer init failed\n"); return -1; }

    g_listen_fd = (nd_socket_t)socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) { perror("[telnet] socket"); return -1; }
    int one = 1;
    setsockopt(ND_SOCK_NATIVE(g_listen_fd), SOL_SOCKET, SO_REUSEADDR,
               ND_SOCKOPT(&one), sizeof one);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(ND_SOCK_NATIVE(g_listen_fd), (struct sockaddr*)&addr, sizeof addr) < 0) {
        perror("[telnet] bind");
        nd_socket_close(g_listen_fd); g_listen_fd = ND_INVALID_SOCKET; return -1;
    }
    if (listen(ND_SOCK_NATIVE(g_listen_fd), 1) < 0) {
        perror("[telnet] listen");
        nd_socket_close(g_listen_fd); g_listen_fd = ND_INVALID_SOCKET; return -1;
    }
    g_shutdown = 0;
    if (pthread_create(&g_accept_thread, NULL, accept_thread_fn, NULL) != 0) {
        perror("[telnet] pthread_create");
        nd_socket_close(g_listen_fd); g_listen_fd = ND_INVALID_SOCKET; return -1;
    }
    fprintf(stderr, "[telnet] listening on port %d - connect with: telnet localhost %d\n",
            port, port);
    return 0;
}

int nd500x_telnet_wait_client(void) {
    pthread_mutex_lock(&g_in_mtx);
    while (!g_connected && !g_shutdown) {
        pthread_cond_wait(&g_in_cond, &g_in_mtx);
    }
    int c = g_connected;
    pthread_mutex_unlock(&g_in_mtx);
    return c;
}

int nd500x_telnet_connected(void) { return g_connected; }

void nd500x_telnet_stop(void) {
    g_shutdown = 1;
    pthread_mutex_lock(&g_in_mtx);
    pthread_cond_broadcast(&g_in_cond);
    pthread_mutex_unlock(&g_in_mtx);
    if (g_client_fd >= 0) {
        shutdown(ND_SOCK_NATIVE(g_client_fd), ND_SHUT_RDWR);
        nd_socket_close(g_client_fd);
        g_client_fd = ND_INVALID_SOCKET;
    }
    if (g_listen_fd >= 0) {
        shutdown(ND_SOCK_NATIVE(g_listen_fd), ND_SHUT_RDWR);
        nd_socket_close(g_listen_fd);
        g_listen_fd = ND_INVALID_SOCKET;
    }
    /* Matches the nd_net_init() in _start(); refcounted, so the telnetserver
     * side keeping its own reference is fine. */
    nd_net_shutdown();
}
