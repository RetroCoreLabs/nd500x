/*
 * nd500_ethhub.h - the RetroCore TCP ethernet wire format, with no sockets in it.
 *
 * WHAT THIS IS FOR
 * ----------------
 * The plan (notes/docs/PLAN_NDIX_NETWORKING_ENUM0_AND_WASM_2026-08-08.md, 2.4)
 * says REUSE RetroCore's existing TCP ethernet fabric rather than invent
 * another one, so that NDIX lands on the same emulated LAN as the ND-100
 * machines. This file is that protocol, and nothing else: parse a spec string,
 * build a handshake, check a handshake, put a length in front of a frame, take
 * one off. All of it is pure, so all of it is testable.
 *
 * WHERE THE FORMAT COMES FROM - READ, NOT ASSUMED
 * -----------------------------------------------
 * RetroCore, Emulated.HW/Common/Network/TcpEthernetBackend.cs:
 *
 *     Magic            = { 0x52, 0x45, 0x54, 0x48 }        "RETH"      (:41)
 *     ProtocolVersion  = 1                                             (:44)
 *     MaxFrameBytes    = 2048                                          (:48)
 *     DefaultPort      = 3094   "the ND Ethernet II PCB number"        (:51)
 *     WriteHandshake   -> 5 bytes: magic then the version              (:395)
 *     ReadHandshake    -> magic must match, version must be >= 1       (:417)
 *     TryReadFrame     -> [u16 BIG-ENDIAN length][bytes], 0 < len <= 2048 (:437)
 *     WriteFrame       -> the same                                     (:460)
 *
 * Both sides write their handshake and then read the peer's - the relay does
 * exactly that too (TcpEthernetRelay.cs:145). A client therefore writes first
 * and does not wait for permission.
 *
 * The version byte doubles as a ROLE: 1 = an ordinary member with one emulated
 * NIC, 2 = a hub-to-hub link carrying an extra inter-hub header
 * (TcpEthernetBackend.cs:386-392). NDIX is one NIC, so it announces 1.
 *
 * NOTE ON A NAME IN THE PLAN. The plan calls the class `TcpEthernetHub`. In the
 * RetroCore tree as it stands (E:\Dev\Repos\Ronny\RetroCore) there is no file
 * of that name: the fabric is `TcpEthernetBackend.cs` (the endpoint) plus
 * `TcpEthernetRelay.cs` (the hub that repeats a frame to every other member).
 * `TcpEthernetHub` is referenced only from a doc comment. The wire format above
 * is read from the code that exists.
 */

#ifndef ND500_ETHHUB_H
#define ND500_ETHHUB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ETHHUB_HANDSHAKE_LEN   5
#define ETHHUB_VERSION_MEMBER  1     /* one emulated NIC - what NDIX is */
#define ETHHUB_MAX_FRAME       2048
#define ETHHUB_DEFAULT_PORT    3094  /* the ND Ethernet II PCB number */

/*
 * Parse an uplink spec into a host and a port.
 *
 * Accepted, matching TcpEthernetBackend.Parse (:120-176) for the CONNECT forms
 * we need:
 *
 *   tcp:host          -> host, 3094
 *   tcp:host:port     -> host, port
 *   host:port         -> host, port
 *
 * A bare word with no port and no "tcp:" prefix is REJECTED rather than guessed
 * at - that is RetroCore's own rule, and it stops a typo like "lop" being
 * dialled as a hostname.
 *
 * Returns 1 on success, 0 if the spec is not a connect spec. `host` is always
 * NUL-terminated on success.
 */
int nd500_ethhub_parse_spec(const char* spec, char* host, size_t hostlen,
                            int* port);

/* Fill `out` (ETHHUB_HANDSHAKE_LEN bytes) with our hello. */
void nd500_ethhub_build_handshake(uint8_t* out, uint8_t version);

/*
 * Check a peer's 5-byte hello. Returns 1 if the magic matched and the version
 * is at least 1, 0 otherwise; *peer_version gets what it announced (0 on
 * failure), so a caller can tell a member from a hub.
 */
int nd500_ethhub_check_handshake(const uint8_t* in, uint8_t* peer_version);

/*
 * Take a frame out of a receive buffer holding `avail` bytes.
 *
 * Returns:
 *   >0  the number of bytes CONSUMED from the buffer; *frame_len is the frame,
 *       which begins at buf + 2.
 *    0  not enough bytes yet - wait for more.
 *   -1  desync: a length of 0 or above ETHHUB_MAX_FRAME. The stream cannot be
 *       resynchronised (there is no framing marker to hunt for), so the caller
 *       must drop the connection rather than carry on reading rubbish.
 */
int nd500_ethhub_take_frame(const uint8_t* buf, size_t avail, uint16_t* frame_len);

/* Write the 2-byte big-endian length prefix for a frame of `len` bytes. */
void nd500_ethhub_put_length(uint8_t* out, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* ND500_ETHHUB_H */
