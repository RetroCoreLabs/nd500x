/*
 * nd500_xring.c - the XMSG ring discipline. See nd500_xring.h for why this is
 * a file of its own with no CPU in it.
 *
 * Everything here mirrors NDIX's if/xg.c. Where a rule looks wrong, it is
 * copied anyway and the reason is written down: the two sides have to agree,
 * and NDIX is the side that cannot be changed.
 */

#include <string.h>
#include "nd500_xring.h"

/* ---- big-endian access ---------------------------------------------------
 * The ND-500 is big-endian and these structures live in guest memory, so the
 * host's own byte order never comes into it. */

uint16_t nd500_xring_be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
uint32_t nd500_xring_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
void nd500_xring_put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v;
}
void nd500_xring_put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static uint16_t rd16(const Nd500XRingMem* m, uint32_t a) {
    return (uint16_t)((m->read8(m->ctx, a) << 8) | m->read8(m->ctx, a + 1));
}
static void wr16(const Nd500XRingMem* m, uint32_t a, uint16_t v) {
    m->write8(m->ctx, a,     (uint8_t)(v >> 8));
    m->write8(m->ctx, a + 1, (uint8_t)v);
}

/* ---- the two rings ------------------------------------------------------- */

void nd500_xring_cmd_init(Nd500XRing* r) {
    r->base = XRING_CMD_VADDR;
    r->entries = XRING_NCMD;
    r->entry_size = XRING_CMD_SIZE;
}

void nd500_xring_resp_init(Nd500XRing* r) {
    r->base = XRING_RESP_VADDR;
    r->entries = XRING_NRESP;
    r->entry_size = XRING_RESP_SIZE;
}

/* Header layout: p @ +0, k @ +2, mp @ +4 (if/xmsg.h:59-69). */
uint16_t nd500_xring_p (const Nd500XRingMem* m, const Nd500XRing* r) { return rd16(m, r->base + 0); }
uint16_t nd500_xring_k (const Nd500XRingMem* m, const Nd500XRing* r) { return rd16(m, r->base + 2); }
uint16_t nd500_xring_mp(const Nd500XRingMem* m, const Nd500XRing* r) { return rd16(m, r->base + 4); }
void nd500_xring_set_p(const Nd500XRingMem* m, const Nd500XRing* r, uint16_t v) { wr16(m, r->base + 0, v); }
void nd500_xring_set_k(const Nd500XRingMem* m, const Nd500XRing* r, uint16_t v) { wr16(m, r->base + 2, v); }

void nd500_xring_write_headers(const Nd500XRingMem* m, const Nd500XRing* r) {
    wr16(m, r->base + 0, 0);              /* p  */
    wr16(m, r->base + 2, 0);              /* k  */
    wr16(m, r->base + 4, r->entries);     /* mp */
}

/* ---- EMPTY and FULL ------------------------------------------------------
 *
 * Verbatim from xg.c:425-430:
 *
 *   #define EMPTY(b) (b.p == b.k)
 *   #define FULL(b)  ((b.p == (b.k - 1)) || ((b.k == 0) && (b.p == b.mp - 2)))
 *
 * Two things about FULL that look like mistakes and are not ours to fix:
 *
 *  - `b.k - 1` is computed on a SHORT, so at k == 0 it is -1 and the first arm
 *    can never match; that is what the second arm is there to cover. Doing the
 *    arithmetic in unsigned here would make (k-1) huge and the arm would never
 *    match at all.
 *  - the second arm compares against mp - 2, not mp - 1, so the ring holds one
 *    fewer entry than it looks like it should. Correcting that to the obvious
 *    form makes this side fill one slot further than NDIX expects, and NDIX
 *    then reads the ring as EMPTY: no error, no data, nothing on screen.
 */
int nd500_xring_empty(const Nd500XRingMem* m, const Nd500XRing* r) {
    return nd500_xring_p(m, r) == nd500_xring_k(m, r);
}

int nd500_xring_full(const Nd500XRingMem* m, const Nd500XRing* r) {
    int p  = (int)nd500_xring_p(m, r);
    int k  = (int)nd500_xring_k(m, r);
    int mp = (int)nd500_xring_mp(m, r);
    /* Signed, so that k == 0 gives k - 1 == -1 and the first arm falls through
     * to the second - exactly as the C in xg.c does on a short. */
    return (p == k - 1) || ((k == 0) && (p == mp - 2));
}

uint32_t nd500_xring_entry_addr(const Nd500XRing* r, uint16_t index) {
    return r->base + XRING_HDR_SIZE + (uint32_t)index * r->entry_size;
}

int nd500_xring_get(const Nd500XRingMem* m, const Nd500XRing* r, uint8_t* out) {
    uint16_t k;
    uint32_t a;
    int i;
    if (nd500_xring_empty(m, r)) return 0;
    k = nd500_xring_k(m, r);
    a = nd500_xring_entry_addr(r, k);
    for (i = 0; i < r->entry_size; i++) out[i] = m->read8(m->ctx, a + (uint32_t)i);
    /* Modulo the COMPILE-TIME count, not mp. R_get does `k = (k+1) % NXMSGRESP`
     * with the constant, and R_init's mp check is what guarantees the two
     * agree - so using mp here would be right only by coincidence. */
    nd500_xring_set_k(m, r, (uint16_t)((k + 1) % r->entries));
    return 1;
}

int nd500_xring_put(const Nd500XRingMem* m, const Nd500XRing* r, const uint8_t* in) {
    uint16_t p;
    uint32_t a;
    int i;
    if (nd500_xring_full(m, r)) return 0;
    p = nd500_xring_p(m, r);
    a = nd500_xring_entry_addr(r, p);
    for (i = 0; i < r->entry_size; i++) m->write8(m->ctx, a + (uint32_t)i, in[i]);
    /* The entry is fully written BEFORE p moves. On real hardware the ND-100
     * and the ND-500 run at the same time, and a consumer that sees p advance
     * is entitled to read the entry immediately. */
    nd500_xring_set_p(m, r, (uint16_t)((p + 1) % r->entries));
    return 1;
}
