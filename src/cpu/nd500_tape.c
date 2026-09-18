/*
 * nd500_tape.c - SIMH .tap record layer (see nd500_tape.h)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

/* SIMH .tap record layer - see nd500_tape.h for the format and the reasoning. */
#include "nd500_tape.h"
#include <string.h>

int nd500_tape_attach(Nd500Tape* t, const char* path) {
    memset(t, 0, sizeof(*t));
    if (!path || !path[0]) return -1;
    t->fp = fopen(path, "rb");
    if (!t->fp) return -1;
    fseek(t->fp, 0, SEEK_END);
    t->size = ftell(t->fp);
    fseek(t->fp, 0, SEEK_SET);
    t->pos = 0;
    return 0;
}

void nd500_tape_detach(Nd500Tape* t) {
    if (t->fp) fclose(t->fp);
    memset(t, 0, sizeof(*t));
}

/* Read a length word at an absolute offset. Little-endian by definition of the
 * format - deliberately assembled byte by byte so host byte order is
 * irrelevant. */
static int tape_rd_len(Nd500Tape* t, long off, uint32_t* out) {
    unsigned char b[4];
    if (!t->fp || off < 0 || off + 4 > t->size) return 0;
    if (fseek(t->fp, off, SEEK_SET) != 0) return 0;
    if (fread(b, 1, 4, t->fp) != 4) return 0;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 1;
}

int nd500_tape_peek(Nd500Tape* t, uint32_t* hdr) {
    return tape_rd_len(t, t->pos, hdr);
}

long nd500_tape_padded(uint32_t len) {
    return (long)len + (long)(len & 1u);
}

uint32_t nd500_tape_datalen(uint32_t hdr) {
    return hdr & 0x00FFFFFFu;
}

void nd500_tape_rewind(Nd500Tape* t) {
    t->pos = 0;
}

int nd500_tape_fwd(Nd500Tape* t, uint32_t* hdr) {
    uint32_t h;
    for (;;) {
        if (!tape_rd_len(t, t->pos, &h)) return 0;
        if (h == ND500_TAPE_EOM) return 0;
        if (h == ND500_TAPE_ERASE_GAP) { t->pos += 4; continue; }  /* not a record */
        break;
    }
    /* A tape mark is a bare header with no payload and no trailer. */
    t->pos += (h == ND500_TAPE_MARK)
            ? 4
            : (4 + nd500_tape_padded(nd500_tape_datalen(h)) + 4);
    if (hdr) *hdr = h;
    return 1;
}

int nd500_tape_back(Nd500Tape* t, uint32_t* hdr) {
    uint32_t h;
    if (t->pos <= 0) return 0;
    /* The word before the current position is the previous record's TRAILING
     * length, or the tape mark itself - zero either way, so one read covers
     * both cases. */
    if (!tape_rd_len(t, t->pos - 4, &h)) return 0;
    if (h == ND500_TAPE_MARK) {
        t->pos -= 4;
    } else {
        long span = 4 + nd500_tape_padded(nd500_tape_datalen(h)) + 4;
        if (t->pos - span < 0) return 0;
        t->pos -= span;
    }
    if (hdr) *hdr = h;
    return 1;
}

int nd500_tape_read(Nd500Tape* t, uint8_t* buf, uint32_t max,
                    uint32_t* actual, uint32_t* reclen, int* is_mark, int* err_flag) {
    uint32_t h, len, want, done = 0;

    if (actual)    *actual = 0;
    if (reclen)    *reclen = 0;
    if (is_mark)   *is_mark = 0;
    if (err_flag)  *err_flag = 0;

    for (;;) {
        if (!tape_rd_len(t, t->pos, &h)) return 0;
        if (h == ND500_TAPE_EOM) return 0;
        if (h == ND500_TAPE_ERASE_GAP) { t->pos += 4; continue; }
        break;
    }

    if (h == ND500_TAPE_MARK) {
        t->pos += 4;
        if (is_mark) *is_mark = 1;
        return 1;                       /* a filemark reads as zero bytes */
    }

    len  = nd500_tape_datalen(h);
    want = (len < max) ? len : max;
    if (buf && want && fseek(t->fp, t->pos + 4, SEEK_SET) == 0)
        done = (uint32_t)fread(buf, 1, want, t->fp);

    if (err_flag && (h & 0x80000000u)) *err_flag = 1;
    if (actual) *actual = done;
    if (reclen) *reclen = len;

    /* Consumed in full even on a short read - the remainder is lost, exactly
     * as on a real drive, and the caller sees it by comparing reclen. */
    t->pos += 4 + nd500_tape_padded(len) + 4;
    return 1;
}
