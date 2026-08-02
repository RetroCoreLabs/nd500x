/*
 * SIMH .tap record layer - the motion and framing logic behind the tape
 * device (MON 600 generic 2). Kept separate from the fecall packet handling
 * in nd500_fecall.c so it can be tested on its own: the guest has no mt
 * binary and no /dev/mt nodes, so the record layer cannot currently be
 * exercised through a boot, and a layer that has never run is a layer nobody
 * should trust.
 *
 * Format (see test/mktape.sh, which writes it):
 *   record  <4-byte LE length N> <N bytes, padded to even> <4-byte LE N>
 *   N == 0            tape mark (filemark)
 *   N == 0xFFFFFFFF   end of medium
 *   N == 0xFFFFFFFE   erase gap
 *   bit 31 set        error record; low 24 bits are still the length
 *
 * The length words are LITTLE-endian. That is the format's definition and has
 * nothing to do with the guest's big-endian byte order, so they are read byte
 * by byte rather than through any of the packet helpers.
 *
 * The trailing length is what makes backspacing possible: stepping back reads
 * the four bytes before the current position to learn the record's span.
 */
#pragma once
#include <stdint.h>
#include <stdio.h>

#define ND500_TAPE_MARK      0x00000000u
#define ND500_TAPE_EOM       0xFFFFFFFFu
#define ND500_TAPE_ERASE_GAP 0xFFFFFFFEu

typedef struct {
    FILE* fp;
    long  size;
    long  pos;      /* byte offset of the NEXT record header */
} Nd500Tape;

/* Attach an image. Returns 0 on success, -1 if it cannot be opened. */
int  nd500_tape_attach(Nd500Tape* t, const char* path);
void nd500_tape_detach(Nd500Tape* t);

/* Header word at the current position, without moving. 0 if unreadable. */
int  nd500_tape_peek(Nd500Tape* t, uint32_t* hdr);

/* Payload bytes a record of this length occupies (data padded to even). */
long nd500_tape_padded(uint32_t len);

/* Data length carried by a header word, ignoring the error flag in bit 31. */
uint32_t nd500_tape_datalen(uint32_t hdr);

/* Step over one record forward / backward. Return 1 on success and set *hdr to
 * the header stepped over; 0 at end of medium, at BOT, or on a malformed
 * image. Erase gaps are skipped transparently going forward. */
int nd500_tape_fwd(Nd500Tape* t, uint32_t* hdr);
int nd500_tape_back(Nd500Tape* t, uint32_t* hdr);

/* Rewind to load point. */
void nd500_tape_rewind(Nd500Tape* t);

/* Read at most max bytes of the next record into buf and advance past it.
 *   *actual  bytes copied into buf
 *   *reclen  the record's REAL length, which may exceed *actual (short read)
 *   returns  1 record read (including a tape mark, which yields 0 bytes)
 *            0 end of medium / nothing readable
 *  The record is consumed whether or not the caller's buffer took all of it,
 *  which is what a real drive does. */
int nd500_tape_read(Nd500Tape* t, uint8_t* buf, uint32_t max,
                    uint32_t* actual, uint32_t* reclen, int* is_mark, int* err_flag);
