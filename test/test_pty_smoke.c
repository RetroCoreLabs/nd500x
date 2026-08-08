/*
 * Interactive-terminal smoke test: run nd500x --debug on a REAL pty.
 *
 * Why a pty and not a pipe: readline (and cbreak-style termios setup in
 * general) is a no-op or takes a different code path when stdin is not a
 * tty, so piped tests cannot catch "the shell exits immediately on a real
 * terminal" or "the tty is left in a broken state" - both of which have
 * shipped before precisely because everything automated ran through pipes.
 *
 * What it asserts:
 *   1. nd500x --debug starts on a pty and produces output.
 *   2. The "regs" command prints a register dump (we look for "PC").
 *   3. The "q" command makes the process exit normally (WIFEXITED).
 *
 * argv[1] = path to the nd500x binary (ctest passes $<TARGET_FILE:nd500x>).
 *
 * Exit code 0 on pass, 1 on fail, with the captured output echoed on fail
 * so the log shows what the "terminal" actually saw.
 */
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <errno.h>

#define TOTAL_TIMEOUT_SEC 30
#define OUTBUF_MAX (1 << 20)

static char outbuf[OUTBUF_MAX];
static size_t outlen = 0;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Drain whatever the child wrote to the pty master, until the deadline or
 * until `until` (if non-NULL) appears in the accumulated output. Returns 1
 * if `until` was seen, 0 otherwise. Also returns on EOF/EIO (child gone). */
static int drain(int mfd, double deadline, const char* until) {
    for (;;) {
        if (until && strstr(outbuf, until)) return 1;
        double left = deadline - now_sec();
        if (left <= 0) return 0;
        struct timeval tv;
        tv.tv_sec = (long)left;
        tv.tv_usec = (long)((left - (double)tv.tv_sec) * 1e6);
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(mfd, &rf);
        int rc = select(mfd + 1, &rf, NULL, NULL, &tv);
        if (rc == 0) return 0;              /* window elapsed */
        if (rc < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        char tmp[4096];
        ssize_t n = read(mfd, tmp, sizeof(tmp));
        if (n <= 0) return 0;               /* EOF or EIO: slave side closed */
        if (outlen + (size_t)n < OUTBUF_MAX - 1) {
            memcpy(outbuf + outlen, tmp, (size_t)n);
            outlen += (size_t)n;
            outbuf[outlen] = '\0';
        }
    }
}

static void send_line(int mfd, const char* line) {
    (void)!write(mfd, line, strlen(line));
    (void)!write(mfd, "\r", 1);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <path-to-nd500x>\n", argv[0]);
        return 1;
    }
    const char* nd500x = argv[1];

    int mfd = posix_openpt(O_RDWR | O_NOCTTY);
    if (mfd < 0) { perror("posix_openpt"); return 1; }
    if (grantpt(mfd) < 0 || unlockpt(mfd) < 0) { perror("grantpt/unlockpt"); return 1; }
    const char* sname = ptsname(mfd);
    if (!sname) { perror("ptsname"); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        /* Child: new session, pty slave becomes the controlling terminal. */
        setsid();
        int sfd = open(sname, O_RDWR);
        if (sfd < 0) _exit(120);
        ioctl(sfd, TIOCSCTTY, 0);
        dup2(sfd, 0);
        dup2(sfd, 1);
        dup2(sfd, 2);
        if (sfd > 2) close(sfd);
        execl(nd500x, nd500x, "--debug", (char*)NULL);
        _exit(121); /* exec failed */
    }

    double deadline = now_sec() + TOTAL_TIMEOUT_SEC;
    int fail = 0;

    /* Give the REPL a moment to come up, then ask for registers and quit.
     * The banner text is not asserted (it changes); the register dump is. */
    drain(mfd, now_sec() + 2.0, NULL);
    send_line(mfd, "regs");
    if (!drain(mfd, deadline, "PC")) {
        fprintf(stderr, "FAIL: no register dump (no \"PC\") after 'regs'\n");
        fail = 1;
    }
    send_line(mfd, "q");
    drain(mfd, deadline, NULL); /* runs to EOF when the child exits */

    /* Reap the child; on timeout, kill our own child so ctest is not left
     * hanging on an orphaned emulator. */
    int status = 0;
    for (;;) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (now_sec() > deadline) {
            fprintf(stderr, "FAIL: nd500x did not exit within %ds after 'q'; killing it\n",
                    TOTAL_TIMEOUT_SEC);
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            fail = 1;
            break;
        }
        drain(mfd, now_sec() + 0.2, NULL);
    }

    if (!fail) {
        if (!WIFEXITED(status)) {
            fprintf(stderr, "FAIL: nd500x terminated by signal %d, not a clean exit\n",
                    WIFSIGNALED(status) ? WTERMSIG(status) : -1);
            fail = 1;
        } else if (WEXITSTATUS(status) == 120 || WEXITSTATUS(status) == 121) {
            fprintf(stderr, "FAIL: could not start %s (exec/slave-open failed, code %d)\n",
                    nd500x, WEXITSTATUS(status));
            fail = 1;
        }
    }

    if (outlen == 0) {
        fprintf(stderr, "FAIL: nd500x produced no output at all on the pty\n");
        fail = 1;
    }

    if (fail) {
        fprintf(stderr, "---- captured pty output (%zu bytes) ----\n", outlen);
        fwrite(outbuf, 1, outlen, stderr);
        fprintf(stderr, "\n---- end ----\n");
        return 1;
    }
    printf("PASS: nd500x --debug on a pty: started, 'regs' printed, 'q' exited cleanly\n");
    close(mfd);
    return 0;
}
