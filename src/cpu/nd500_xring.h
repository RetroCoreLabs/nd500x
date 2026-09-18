/*
 * nd500_xring.h - the XMSG ring buffers NDIX shares with the ND-100.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHAT THIS IS
 * ------------
 * NDIX does all of its networking through two ring buffers in the segment-6
 * window it shares with the ND-100. There is no FE_READ or FE_WRIT on the XMSG
 * generic (7) at all - the fecall carries only control, and every byte of data
 * moves through these rings. Anything that wants to be `*ENUM0` has to be the
 * other end of them.
 *
 * WHY IT IS ITS OWN FILE, WITH NO CPU IN IT
 * -----------------------------------------
 * The producer/consumer discipline is fiddly in ways that do not fail loudly.
 * NDIX's FULL() test is asymmetric and wastes a slot; the wrap is modulo a
 * COMPILE-TIME constant rather than the mp field sitting right next to it; and
 * the kick that tells the other side there is work only fires on an empty ->
 * non-empty transition, so a server that stops draining is never told again and
 * the link stalls in silence. Every one of those reads as "nothing happened",
 * which is the worst thing to be debugging inside a booting guest.
 *
 * So the discipline lives here, as plain functions over a byte accessor, and is
 * tested on its own. The guest is not needed to find out whether the rules are
 * right - and per the spec's own warning, getting FULL wrong means a producer
 * that fills one slot further than NDIX expects, which the consumer then reads
 * as empty.
 *
 * SOURCE OF TRUTH
 *   NDIX kernel  : if/xmsg.h (layout), if/xg.c R_init/R_put/R_get (discipline),
 *                  machine/locore.c:109-122 (the two fixed addresses)
 *   Written up in: notes/docs/SPEC_ENUM0_ETHERNET_MEDIA_SERVER_FOR_ND500X_2026-08-08.md
 *                  sections 2.1 and 2.2, in the NDIX-C repository.
 *
 * BYTE ORDER. The ND-500 is big-endian and these structures are guest memory,
 * so every field is read and written big-endian regardless of the host.
 */

#ifndef ND500_XRING_H
#define ND500_XRING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The two rings, at the addresses locore.c fixes them at. Segment 6 begins at
 * _sharebase = 0x30000000; these are ND-500 VIRTUAL addresses. */
#define XRING_CMD_VADDR    0x30000000u   /* ND-500 -> ND-100, commands  */
#define XRING_RESP_VADDR   0x30000800u   /* ND-100 -> ND-500, responses */

/* Entry counts. These are COMPILE-TIME constants in NDIX, derived from the
 * 0x800 window and the ND-500's byte-aligned packing:
 *   sizeof(struct xmsg_cmd)  = 20  ->  (0x800 - 6) / 20 = 102
 *   sizeof(struct xmsg_resp) = 18  ->  (0x800 - 6) / 18 = 113
 * R_init() panics unless mp matches, and R_put/R_get wrap modulo THESE, not
 * modulo the mp field - which is the second reason R_init checks it. */
#define XRING_NCMD         102
#define XRING_NRESP        113
#define XRING_CMD_SIZE     20
#define XRING_RESP_SIZE    18
#define XRING_HDR_SIZE     6            /* p, k, mp - three 16-bit fields */

/* How this code reaches guest memory. A function pointer rather than a direct
 * call into the CPU keeps the ring logic testable with a plain array, which is
 * the whole point of the file. */
typedef struct Nd500XRingMem {
    uint8_t (*read8) (void* ctx, uint32_t vaddr);
    void    (*write8)(void* ctx, uint32_t vaddr, uint8_t val);
    void*   ctx;
} Nd500XRingMem;

/* One ring. `base` is its virtual address, `entries` its compile-time entry
 * count, `entry_size` the size of one entry in bytes. */
typedef struct Nd500XRing {
    uint32_t base;
    uint16_t entries;
    uint16_t entry_size;
} Nd500XRing;

void nd500_xring_cmd_init (Nd500XRing* r);   /* the command ring  (102 x 20) */
void nd500_xring_resp_init(Nd500XRing* r);   /* the response ring (113 x 18) */

/* Header access. p = producer index, k = consumer index, mp = entry count. */
uint16_t nd500_xring_p (const Nd500XRingMem* m, const Nd500XRing* r);
uint16_t nd500_xring_k (const Nd500XRingMem* m, const Nd500XRing* r);
uint16_t nd500_xring_mp(const Nd500XRingMem* m, const Nd500XRing* r);
void nd500_xring_set_p(const Nd500XRingMem* m, const Nd500XRing* r, uint16_t v);
void nd500_xring_set_k(const Nd500XRingMem* m, const Nd500XRing* r, uint16_t v);

/*
 * Write the headers NDIX demands before it will run.
 *
 * R_init() (xg.c:399-410) PANICS unless p == 0, k == 0 and mp == the expected
 * count, on both rings - and NDIX never writes mp itself, so somebody on this
 * side has to have done it before FE_IDEV completes. Getting it wrong stops the
 * machine on the spot rather than degrading.
 */
void nd500_xring_write_headers(const Nd500XRingMem* m, const Nd500XRing* r);

/*
 * EMPTY and FULL, exactly as NDIX computes them (xg.c:425-430):
 *
 *   #define EMPTY(b) (b.p == b.k)
 *   #define FULL(b)  ((b.p == (b.k - 1)) || ((b.k == 0) && (b.p == b.mp - 2)))
 *
 * The FULL test is ASYMMETRIC and it is copied verbatim on purpose. It wastes a
 * slot, and the `k == 0` arm uses mp - 2 rather than mp - 1. A server that
 * "corrects" this to the obvious ring-buffer form fills one slot further than
 * NDIX expects, and NDIX then reads the ring as EMPTY - no error, no data, and
 * nothing on screen to say why.
 */
int nd500_xring_empty(const Nd500XRingMem* m, const Nd500XRing* r);
int nd500_xring_full (const Nd500XRingMem* m, const Nd500XRing* r);

/* Byte offset of entry i. Entry i begins at base + 6 + i*entry_size. */
uint32_t nd500_xring_entry_addr(const Nd500XRing* r, uint16_t index);

/*
 * Take the next entry NDIX produced, into `out` (entry_size bytes), and advance
 * k. Returns 1 if an entry was taken, 0 if the ring was empty.
 *
 * DRAIN TO EMPTY, EVERY TIME. NDIX kicks only on an empty -> non-empty
 * transition (`oldp == k` in R_put). A server that stops while entries remain
 * is never kicked again and the link stalls silently - which is why this
 * returns "was there one" rather than taking a count.
 */
int nd500_xring_get(const Nd500XRingMem* m, const Nd500XRing* r, uint8_t* out);

/*
 * Put one entry into the ring and advance p. Returns 1 on success, 0 if the
 * ring was full (by NDIX's FULL rule above, not by an obvious one).
 */
int nd500_xring_put(const Nd500XRingMem* m, const Nd500XRing* r, const uint8_t* in);

/* Big-endian field helpers for the entry bodies. */
uint16_t nd500_xring_be16(const uint8_t* p);
uint32_t nd500_xring_be32(const uint8_t* p);
void     nd500_xring_put_be16(uint8_t* p, uint16_t v);
void     nd500_xring_put_be32(uint8_t* p, uint32_t v);

#ifdef __cplusplus
}
#endif

#endif /* ND500_XRING_H */
