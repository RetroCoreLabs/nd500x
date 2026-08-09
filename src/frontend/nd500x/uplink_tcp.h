/*
 * uplink_tcp.h - dial into a RetroCore TCP ethernet relay and be one member of
 * the emulated LAN.
 *
 * WHY THIS LIVES IN THE FRONTEND AND "loop" DOES NOT
 * --------------------------------------------------
 * The loopback uplink is in the portable boot path because the wasm build needs
 * it too. This one needs a socket, so it lives here with net_compat.h and is
 * native-only. Both plug into the same one-function-pointer seam
 * (nd500_xmsg_set_uplink), which is the whole point of that seam.
 *
 * The wire format is in ../../cpu/nd500_ethhub.h, with no sockets in it, so the
 * parts that can be got wrong silently are tested. What is left here is
 * connect, read, write, and giving up gracefully.
 */

#ifndef ND500X_UPLINK_TCP_H
#define ND500X_UPLINK_TCP_H

#include "../../cpu/cpu_protos.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Start the uplink named by ND500X_ETH_UPLINK, if it is a TCP spec.
 *
 *   tcp:host[:port] / host:port   dial a relay, 3094 by default
 *   loop / none / unset           not ours - handled elsewhere, or nothing
 *
 * Returns 1 if a TCP uplink was started, 0 if the setting named something else,
 * -1 if it named a TCP uplink that could not be reached.
 *
 * A FAILED DIAL IS NOT FATAL and must not be. The guest boots either way; it
 * just has an ethernet with nothing on the other end, which is exactly what it
 * had before this file existed. Killing the boot because a relay is not running
 * would be a worse machine than no networking at all.
 */
int nd500x_uplink_tcp_start(Nd500Cpu* cpu);

/* Close the connection, if there is one. Safe to call when there is not. */
void nd500x_uplink_tcp_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* ND500X_UPLINK_TCP_H */
