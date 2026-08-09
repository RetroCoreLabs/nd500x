/*
 * uplink_tcp.c - NDIX as one member of a RetroCore TCP ethernet segment.
 *
 * See uplink_tcp.h for why this is in the frontend, and ../../cpu/nd500_ethhub.h
 * for where every byte of the protocol was read from.
 *
 * Shape: dial out, exchange the 5-byte hello, then length-prefixed frames both
 * ways. Dialling OUT is what makes this work through NAT with no port
 * forwarding - the relay is the meeting point and only it needs to be reachable
 * (TcpEthernetRelay.cs:26-29).
 */

#include "uplink_tcp.h"
#include "net_compat.h"
/* net_compat.h covers the parts of the socket API that DIVERGE between POSIX
 * and Winsock. getaddrinfo is not one of them - it is spelled the same on both
 * - but it lives in a header net_compat does not pull in on POSIX. */
#ifndef _WIN32
#include <netdb.h>
#endif
#include "../../cpu/nd500_ethhub.h"
#include "../../cpu/nd500_xmsg.h"
#include "../../cpu/nd500_settings.h"

#include <stdio.h>
#include <string.h>

/* One connection, like everything else here: nd500x runs one machine per
 * process, and one machine has one et0. */
static nd_socket_t g_sock = ND_INVALID_SOCKET;
static int         g_up;             /* handshake done, frames may flow */

/* LISTEN mode: bound and waiting for somebody to dial in. Kept separate from
 * g_sock because the listener outlives any one peer - a guest that is rebooted
 * on the other end must be able to come back without this one restarting. */
static nd_socket_t g_listen = ND_INVALID_SOCKET;

/* Whatever has arrived and not yet been taken apart. A frame can be split
 * across any number of reads, so this has to survive between polls. */
static uint8_t  g_in[4 * ETHHUB_MAX_FRAME];
static size_t   g_in_len;

static unsigned long g_tx, g_rx;

static void uplink_close(const char* why) {
    if (g_sock != ND_INVALID_SOCKET) {
        fprintf(stderr, "[XMSG] uplink: link down (%s) after %lu sent, %lu received\n",
                why, g_tx, g_rx);
        nd_socket_close(g_sock);
    }
    g_sock   = ND_INVALID_SOCKET;
    g_up     = 0;
    g_in_len = 0;
    /* The server keeps answering with no uplink - frames are dropped and
     * counted. A dead relay must not take the guest's ethernet down with it. */
}

void nd500x_uplink_tcp_stop(void) {
    if (g_sock != ND_INVALID_SOCKET) uplink_close("stopped");
    if (g_listen != ND_INVALID_SOCKET) {
        nd_socket_close(g_listen);
        g_listen = ND_INVALID_SOCKET;
    }
    nd500_xmsg_set_uplink(NULL, NULL);
    nd500_xmsg_set_uplink_poll(NULL, NULL);
}

/* ---- sending ------------------------------------------------------------- */

/*
 * A transmitted frame, straight from XETHER. Written as one buffer rather than
 * a header write followed by a body write: two writes can interleave with
 * nothing, since this is single threaded, but one write is one packet on the
 * wire and one syscall.
 */
static void uplink_frame_out(void* ctx, const uint8_t* frame, uint32_t len) {
    uint8_t out[2 + ETHHUB_MAX_FRAME];
    size_t total, sent = 0;
    (void)ctx;

    if (!g_up || g_sock == ND_INVALID_SOCKET) return;
    if (len == 0 || len > ETHHUB_MAX_FRAME) {
        fprintf(stderr, "[XMSG] uplink: refusing to send a %u-byte frame "
                        "(limit is %d)\n", len, ETHHUB_MAX_FRAME);
        return;
    }

    nd500_ethhub_put_length(out, (uint16_t)len);
    memcpy(out + 2, frame, len);
    total = len + 2u;

    while (sent < total) {
        nd_ssize_t n = send(ND_SOCK_NATIVE(g_sock), ND_SOCK_BUF(out + sent),
                            ND_SOCK_LEN(total - sent), MSG_NOSIGNAL);
        if (n <= 0) {
            if (n < 0 && nd_last_socket_error() == ND_EINTR) continue;
            uplink_close("send failed");
            return;
        }
        sent += (size_t)n;
    }
    g_tx++;
}

/* ---- receiving ----------------------------------------------------------- */

/*
 * Called from the front-end clock tick, 50 times a second. Reads whatever is
 * there without blocking and hands every complete frame to the XMSG server,
 * which either completes a parked receive (and raises the interrupt) or queues
 * the frame for NDIX's next XFRREN.
 */
static int read_fully(nd_socket_t s, uint8_t* buf, size_t count);

/*
 * LISTEN mode: is somebody dialling in? Non-blocking - if nobody is there this
 * returns at once and the guest carries on.
 *
 * One peer at a time. This is a point-to-point link, not a hub: the hub role is
 * RetroCore's TcpEthernetRelay, and duplicating it here would be a second
 * implementation of something that already exists and works. What this is for
 * is two emulators talking directly with nothing else running.
 */
static void uplink_accept(void) {
    nd_pollfd_t pfd;
    nd_socket_t s;
    uint8_t hello[ETHHUB_HANDSHAKE_LEN], peer[ETHHUB_HANDSHAKE_LEN];
    uint8_t peer_version = 0;
    int one = 1;

    pfd.fd = ND_SOCK_NATIVE(g_listen);
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (nd_poll(&pfd, 1, 0) <= 0) return;

    s = accept(ND_SOCK_NATIVE(g_listen), NULL, NULL);
    if (s == ND_INVALID_SOCKET) return;

    setsockopt(ND_SOCK_NATIVE(s), IPPROTO_TCP, TCP_NODELAY,
               ND_SOCKOPT(&one), (nd_socklen_t)sizeof one);

    /* Write ours, then read theirs - the order the relay uses on an accepted
     * connection too (TcpEthernetRelay.cs:145-146), so neither side is waiting
     * for the other to go first.
     *
     * The read blocks, briefly and once per connection. The peer writes its
     * hello the instant it connects, so this is a formality; the poll below
     * bounds it so a peer that connects and then says nothing costs half a
     * second rather than the whole machine. */
    nd500_ethhub_build_handshake(hello, ETHHUB_VERSION_MEMBER);
    if (send(ND_SOCK_NATIVE(s), ND_SOCK_BUF(hello), ND_SOCK_LEN(sizeof hello),
             MSG_NOSIGNAL) != (nd_ssize_t)sizeof hello) {
        nd_socket_close(s);
        return;
    }
    pfd.fd = ND_SOCK_NATIVE(s);
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (nd_poll(&pfd, 1, 500) <= 0 ||
        !read_fully(s, peer, sizeof peer) ||
        !nd500_ethhub_check_handshake(peer, &peer_version)) {
        fprintf(stderr, "[XMSG] uplink: something connected but did not send a "
                        "RETH handshake - dropped\n");
        nd_socket_close(s);
        return;
    }

    g_sock   = s;
    g_up     = 1;
    g_in_len = 0;
    fprintf(stderr, "[XMSG] uplink: peer joined (protocol version %u)\n",
            peer_version);
}

static void uplink_poll(void* ctx, Nd500Cpu* cpu) {
    nd_pollfd_t pfd;
    (void)ctx;

    if (!cpu) return;
    if (!g_up && g_listen != ND_INVALID_SOCKET) uplink_accept();
    if (!g_up || g_sock == ND_INVALID_SOCKET) return;

    for (;;) {
        int consumed;
        uint16_t flen;

        /* Take everything already buffered apart FIRST, then ask for more.
         * A single read can carry several frames, and dropping out to the CPU
         * between each one would leave them sitting here for another 20 ms. */
        while ((consumed = nd500_ethhub_take_frame(g_in, g_in_len, &flen)) > 0) {
            nd500_xmsg_frame_in(cpu, g_in + 2, flen);
            g_rx++;
            memmove(g_in, g_in + consumed, g_in_len - (size_t)consumed);
            g_in_len -= (size_t)consumed;
        }
        if (consumed < 0) {
            /* A length of 0 or over 2048. There is no marker to hunt for, so
             * the stream cannot be resynchronised - RetroCore drops the link on
             * the same condition (TcpEthernetBackend.cs:446-449). */
            uplink_close("stream desynchronised");
            return;
        }

        if (g_in_len == sizeof g_in) {
            /* Cannot happen while frames are legal - the buffer holds four
             * maximum frames - so if it does, the peer is not speaking this
             * protocol at all. */
            uplink_close("receive buffer full with no complete frame");
            return;
        }

        pfd.fd = ND_SOCK_NATIVE(g_sock);
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (nd_poll(&pfd, 1, 0) <= 0) return;      /* nothing waiting */

        {
            nd_ssize_t n = recv(ND_SOCK_NATIVE(g_sock),
                                ND_SOCK_BUF(g_in + g_in_len),
                                ND_SOCK_LEN(sizeof g_in - g_in_len), 0);
            if (n == 0) { uplink_close("relay closed the connection"); return; }
            if (n < 0) {
                if (nd_last_socket_error() == ND_EINTR) continue;
                uplink_close("receive failed");
                return;
            }
            g_in_len += (size_t)n;
        }
    }
}

/* ---- connecting ---------------------------------------------------------- */

/* Read exactly `count` bytes, blocking. Used only for the handshake, which is
 * five bytes and happens once per connection. */
static int read_fully(nd_socket_t s, uint8_t* buf, size_t count) {
    size_t got = 0;
    while (got < count) {
        nd_ssize_t n = recv(ND_SOCK_NATIVE(s), ND_SOCK_BUF(buf + got),
                            ND_SOCK_LEN(count - got), 0);
        if (n == 0) return 0;
        if (n < 0) {
            if (nd_last_socket_error() == ND_EINTR) continue;
            return 0;
        }
        got += (size_t)n;
    }
    return 1;
}

int nd500x_uplink_tcp_start(Nd500Cpu* cpu) {
    const char* spec = nd500_settings()->eth_uplink;
    char host[256];
    int port = 0;
    struct addrinfo hints, *res = NULL, *ai;
    char portstr[16];
    uint8_t hello[ETHHUB_HANDSHAKE_LEN];
    uint8_t peer[ETHHUB_HANDSHAKE_LEN];
    uint8_t peer_version = 0;
    int one = 1;

    if (!cpu || !spec || !*spec) return 0;
    if (strcmp(spec, "loop") == 0 || strcmp(spec, "none") == 0) return 0;

    /* LISTEN: bind and wait. Nothing blocks here - the accept happens in the
     * poll, so the guest boots whether or not anyone ever turns up. */
    if (nd500_ethhub_parse_listen(spec, &port)) {
        struct sockaddr_in a;
        nd_socklen_t alen = (nd_socklen_t)sizeof a;
        int one = 1;

        if (nd_net_init() != 0) {
            fprintf(stderr, "[XMSG] uplink: no networking available\n");
            return -1;
        }
        g_listen = socket(AF_INET, SOCK_STREAM, 0);
        if (g_listen == ND_INVALID_SOCKET) {
            fprintf(stderr, "[XMSG] uplink: cannot create a listening socket\n");
            return -1;
        }
        /* Without this a restart inside TIME_WAIT fails to bind, which on a
         * machine being booted over and over is most of the time. */
        setsockopt(ND_SOCK_NATIVE(g_listen), SOL_SOCKET, SO_REUSEADDR,
                   ND_SOCKOPT(&one), (nd_socklen_t)sizeof one);

        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons((unsigned short)port);
        if (bind(ND_SOCK_NATIVE(g_listen), (struct sockaddr*)&a, (nd_socklen_t)sizeof a) != 0 ||
            listen(ND_SOCK_NATIVE(g_listen), 1) != 0) {
            fprintf(stderr, "[XMSG] uplink: cannot listen on port %d "
                            "(is one already running?)\n", port);
            nd_socket_close(g_listen);
            g_listen = ND_INVALID_SOCKET;
            return -1;
        }
        /* Report the port actually bound - with "listen:0" the OS picked it. */
        if (getsockname(ND_SOCK_NATIVE(g_listen), (struct sockaddr*)&a, &alen) == 0)
            port = ntohs(a.sin_port);

        g_tx = g_rx = 0;
        nd500_xmsg_set_uplink(uplink_frame_out, NULL);
        nd500_xmsg_set_uplink_poll(uplink_poll, NULL);
        fprintf(stderr, "[XMSG] uplink: listening on port %d for one peer\n", port);
        return 1;
    }

    if (!nd500_ethhub_parse_spec(spec, host, sizeof host, &port)) {
        fprintf(stderr, "[XMSG] uplink: ND500X_ETH_UPLINK=\"%s\" is not a spec I "
                        "understand. Use loop, listen[:port], or "
                        "tcp:host[:port].\n", spec);
        return -1;
    }

    if (nd_net_init() != 0) {
        fprintf(stderr, "[XMSG] uplink: no networking available\n");
        return -1;
    }

    snprintf(portstr, sizeof portstr, "%d", port);
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0 || !res) {
        fprintf(stderr, "[XMSG] uplink: cannot resolve %s\n", host);
        return -1;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        nd_socket_t s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == ND_INVALID_SOCKET) continue;
        if (connect(ND_SOCK_NATIVE(s), ai->ai_addr, (nd_socklen_t)ai->ai_addrlen) == 0) {
            g_sock = s;
            break;
        }
        nd_socket_close(s);
    }
    freeaddrinfo(res);

    if (g_sock == ND_INVALID_SOCKET) {
        /* Not fatal, and deliberately so - see the header. */
        fprintf(stderr, "[XMSG] uplink: no relay answered at %s:%d. et0 will "
                        "come up with nothing on the other end.\n", host, port);
        return -1;
    }

    /* Nagle off: these are small, latency-sensitive frames, and the relay does
     * the same on its side (TcpEthernetRelay.cs:142). */
    setsockopt(ND_SOCK_NATIVE(g_sock), IPPROTO_TCP, TCP_NODELAY,
               ND_SOCKOPT(&one), (nd_socklen_t)sizeof one);

    /* Write our hello, then read theirs - the order both RetroCore endpoints
     * use (TcpEthernetBackend.cs, TcpEthernetRelay.cs:145-146), so neither side
     * waits for the other to speak first. */
    nd500_ethhub_build_handshake(hello, ETHHUB_VERSION_MEMBER);
    if (send(ND_SOCK_NATIVE(g_sock), ND_SOCK_BUF(hello),
             ND_SOCK_LEN(sizeof hello), MSG_NOSIGNAL) != (nd_ssize_t)sizeof hello) {
        uplink_close("handshake could not be sent");
        return -1;
    }
    if (!read_fully(g_sock, peer, sizeof peer) ||
        !nd500_ethhub_check_handshake(peer, &peer_version)) {
        fprintf(stderr, "[XMSG] uplink: %s:%d answered, but not with a RETH "
                        "handshake - is that really a relay?\n", host, port);
        uplink_close("bad handshake");
        return -1;
    }

    g_up     = 1;
    g_in_len = 0;
    g_tx = g_rx = 0;
    nd500_xmsg_set_uplink(uplink_frame_out, NULL);
    nd500_xmsg_set_uplink_poll(uplink_poll, NULL);
    fprintf(stderr, "[XMSG] uplink: joined the ethernet segment at %s:%d "
                    "(peer protocol version %u)\n", host, port, peer_version);
    return 1;
}
