/*
 * uplink_tap.c - NDIX's et0 on a Linux TAP device.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See uplink_tap.h for why this exists alongside uplink_tcp.c and why it does
 * not configure the host interface itself.
 *
 * The whole file is: open the device, hand nd500_xmsg two function pointers,
 * and move bytes. There is no framing - a TAP read returns exactly one
 * ethernet frame and a write sends exactly one, which is why this is a third
 * the size of the TCP uplink even though it does strictly more.
 */

#include "uplink_tap.h"
#include "../../cpu/nd500_ethhub.h"   /* ETHHUB_MAX_FRAME - one frame budget */
#include "../../cpu/nd500_xmsg.h"
#include "../../cpu/nd500_settings.h"

#include <stdio.h>
#include <string.h>

#ifdef __linux__

#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* One device, like everything else here: one process, one machine, one et0. */
static int  g_fd = -1;
static char g_dev[IFNAMSIZ];

static unsigned long g_tx, g_rx, g_drop;

/* The default device name. NOT "tap0": that is the name every other tool on the
 * box reaches for first, and colliding with someone else's tap0 would look like
 * a network fault rather than a name clash. "nd0" says whose it is. */
#define TAP_DEFAULT_DEV "nd0"

/* ---- sending: guest -> host --------------------------------------------- */

static void tap_frame_out(void* ctx, const uint8_t* frame, uint32_t len) {
    ssize_t n;
    (void)ctx;

    if (g_fd < 0) return;
    if (len == 0 || len > ETHHUB_MAX_FRAME) {
        fprintf(stderr, "[XMSG] tap: refusing to send a %u-byte frame "
                        "(limit is %d)\n", len, ETHHUB_MAX_FRAME);
        return;
    }

    /* A TAP write is all-or-nothing: it never does a partial frame, because a
     * half-frame is not a thing the kernel could deliver. So there is no
     * send-loop here, unlike the TCP uplink where a short write is normal. */
    do {
        n = write(g_fd, frame, (size_t)len);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        /* ENOBUFS/EAGAIN just means the host queue is full this instant. Drop
         * it and count it - ethernet is allowed to lose frames, and taking the
         * guest's interface down over a momentary queue would be far worse. */
        g_drop++;
        if (g_drop == 1 || (g_drop % 100) == 0)
            fprintf(stderr, "[XMSG] tap: dropped %lu frame(s) writing to %s (%s)\n",
                    g_drop, g_dev, strerror(errno));
        return;
    }
    g_tx++;
}

/* ---- receiving: host -> guest -------------------------------------------- */

/*
 * Drain whatever the host has queued and hand each frame to the XMSG server.
 *
 * BOUNDED ON PURPOSE. This runs from the 50 Hz clock tick inside the CPU loop,
 * so an unbounded drain would let a busy host interface starve the guest of CPU
 * - the machine would appear to hang under load and nothing would say why.
 * 64 frames per tick is 3200 frames/second, far more than a 1988 kernel will
 * consume, and anything above that would be dropped by NDIX's own receive queue
 * anyway.
 */
#define TAP_DRAIN_PER_TICK 64

static void tap_poll(void* ctx, Nd500Cpu* cpu) {
    uint8_t frame[ETHHUB_MAX_FRAME];
    int i;
    (void)ctx;

    if (g_fd < 0 || !cpu) return;

    for (i = 0; i < TAP_DRAIN_PER_TICK; i++) {
        ssize_t n;
        do {
            n = read(g_fd, frame, sizeof frame);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            /* Nothing waiting. The fd is non-blocking, so this is the normal
             * exit from this loop, not an error. */
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            fprintf(stderr, "[XMSG] tap: read from %s failed (%s)\n",
                    g_dev, strerror(errno));
            return;
        }
        if (n == 0) return;

        g_rx++;
        /* frame_in raises the receive interrupt as well as queueing, which is
         * what makes NDIX come and collect it. Its return value says whether
         * the guest had room; a full queue is counted there, not here. */
        (void)nd500_xmsg_frame_in(cpu, frame, (uint32_t)n);
    }
}

/* ---- opening ------------------------------------------------------------- */

void nd500x_uplink_tap_stop(void) {
    if (g_fd >= 0) {
        fprintf(stderr, "[XMSG] tap: closing %s after %lu sent, %lu received, "
                        "%lu dropped\n", g_dev, g_tx, g_rx, g_drop);
        close(g_fd);
        g_fd = -1;
    }
    nd500_xmsg_set_uplink(NULL, NULL);
    nd500_xmsg_set_uplink_poll(NULL, NULL);
}

int nd500x_uplink_tap_start(Nd500Cpu* cpu) {
    const char* spec = nd500_settings()->eth_uplink;
    const char* name;
    struct ifreq ifr;
    int fd, flags;

    if (!cpu || !spec || !*spec) return 0;

    /* "tap" or "tap:<name>". Anything else belongs to another uplink. */
    if (strcmp(spec, "tap") == 0) {
        name = TAP_DEFAULT_DEV;
    } else if (strncmp(spec, "tap:", 4) == 0 && spec[4]) {
        name = spec + 4;
    } else {
        return 0;
    }

    if (strlen(name) >= IFNAMSIZ) {
        fprintf(stderr, "[XMSG] tap: device name \"%s\" is longer than the %d "
                        "characters Linux allows\n", name, IFNAMSIZ - 1);
        return -1;
    }

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[XMSG] tap: cannot open /dev/net/tun (%s).\n"
                        "        Create the device once, as root:\n"
                        "          sudo ip tuntap add dev %s mode tap user $USER\n"
                        "          sudo ip addr add 223.255.254.1/24 dev %s\n"
                        "          sudo ip link set %s up\n",
                strerror(errno), name, name, name);
        return -1;
    }

    memset(&ifr, 0, sizeof ifr);
    /* IFF_TAP: ethernet frames, not IP packets (that would be IFF_TUN and NDIX
     * would never see a MAC header). IFF_NO_PI: no 4-byte packet-info prefix,
     * so a read is the frame and nothing else. */
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);

    if (ioctl(fd, TUNSETIFF, (void*)&ifr) < 0) {
        fprintf(stderr, "[XMSG] tap: cannot attach to \"%s\" (%s).\n"
                        "        If it does not exist yet:\n"
                        "          sudo ip tuntap add dev %s mode tap user $USER\n",
                name, strerror(errno), name);
        close(fd);
        return -1;
    }

    /* Non-blocking, because tap_poll runs on the clock tick and must never sit
     * in a read waiting for a frame that may never come - that would stop the
     * guest's CPU dead. */
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        fprintf(stderr, "[XMSG] tap: cannot set %s non-blocking (%s)\n",
                name, strerror(errno));
        close(fd);
        return -1;
    }

    g_fd = fd;
    snprintf(g_dev, sizeof g_dev, "%s", ifr.ifr_name);
    g_tx = g_rx = g_drop = 0;

    nd500_xmsg_set_uplink(tap_frame_out, NULL);
    nd500_xmsg_set_uplink_poll(tap_poll, NULL);

    fprintf(stderr, "[XMSG] tap: et0 is on host device %s\n", g_dev);
    return 1;
}

#else /* !__linux__ */

/* /dev/net/tun is a Linux interface. Say so plainly rather than failing in a
 * way that looks like a configuration mistake. */
int nd500x_uplink_tap_start(Nd500Cpu* cpu) {
    const char* spec = nd500_settings()->eth_uplink;
    (void)cpu;
    if (!spec || (strcmp(spec, "tap") != 0 && strncmp(spec, "tap:", 4) != 0))
        return 0;
    fprintf(stderr, "[XMSG] tap: TAP devices are Linux-only. Use "
                    "ND500X_ETH_UPLINK=tcp:host:port or listen:port here.\n");
    return -1;
}

void nd500x_uplink_tap_stop(void) { }

#endif /* __linux__ */
