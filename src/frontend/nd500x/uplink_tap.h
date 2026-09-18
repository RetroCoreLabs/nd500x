/*
 * uplink_tap.h - put NDIX's et0 on a real Linux TAP device, so the host (and
 * anything the host routes for) can reach the guest with ordinary tools.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHY A TAP AND NOT THE TCP RELAY
 * ------------------------------
 * uplink_tcp.c joins nd500x to a RetroCore *emulated* segment: everything on
 * that wire is another emulator. That is the right thing for guest-to-guest,
 * and it is how the two-machine ping test runs. But it cannot be reached by
 * `telnet` on the host, because nothing on the host speaks the RETH framing.
 *
 * A TAP device is the opposite trade: the kernel gives us a virtual ethernet
 * NIC whose other end is a normal host interface. Give that interface an
 * address and the host's own IP stack is on NDIX's segment. `telnet
 * 223.255.254.8` then works from a plain shell, with no relay and no new
 * protocol code - the frames we already build go straight out.
 *
 * MEASURED CONSTRAINT, NOT A GUESS: the image has TCP *servers* and no matching
 * client. /etc has telnetd, rlogind, rshd, rexecd; /usr/ucb has ftp but /etc
 * has no ftpd (and /etc/rc says so in as many words). /usr/ucb has no telnet
 * and no rlogin. So a guest-to-guest TCP session cannot be built from what is
 * on the disk - reaching the guest from the host is the only way to exercise
 * TCP at all, which is what makes this file necessary rather than convenient.
 *
 * LINUX ONLY. /dev/net/tun is a Linux interface. On other platforms
 * nd500x_uplink_tap_start() reports that and returns -1; it never pretends.
 *
 * PRIVILEGE. Opening /dev/net/tun needs CAP_NET_ADMIN, or a persistent device
 * pre-created and chowned:
 *
 *     sudo ip tuntap add dev nd0 mode tap user "$USER"
 *     sudo ip addr add 223.255.254.1/24 dev nd0
 *     sudo ip link set nd0 up
 *
 * After that nd500x needs no privilege at all, which is why this deliberately
 * does NOT create or configure the device itself. Bringing an interface up and
 * assigning it an address are the host administrator's business; an emulator
 * silently reconfiguring host networking is the kind of thing that is very hard
 * to undo and very easy to not notice.
 */

#ifndef ND500X_UPLINK_TAP_H
#define ND500X_UPLINK_TAP_H

#include "../../cpu/cpu_protos.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Start the uplink named by ND500X_ETH_UPLINK, if it names a TAP.
 *
 *   tap            attach to a device named "nd0"
 *   tap:<name>     attach to that device
 *   anything else  not ours - returns 0 so the caller can try uplink_tcp
 *
 * Returns 1 if a TAP uplink was started, 0 if the setting named something else,
 * -1 if it named a TAP that could not be opened.
 *
 * A FAILED ATTACH IS NOT FATAL, for the same reason it is not in uplink_tcp:
 * the guest boots either way, it just has an ethernet with nothing on the far
 * end. Refusing to boot a machine because its network is missing would be a
 * worse machine than one with no network.
 */
int nd500x_uplink_tap_start(Nd500Cpu* cpu);

/* Close the device, if one is open. Safe to call when there is not. */
void nd500x_uplink_tap_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* ND500X_UPLINK_TAP_H */
