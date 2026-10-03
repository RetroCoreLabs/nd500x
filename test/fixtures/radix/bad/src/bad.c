/*
 * radix_gate fixture: KNOWN BAD. This file is never compiled; it exists so the
 * gate can be shown to FAIL on the defect it was written for, not merely to
 * pass on clean sources. A gate that has never been seen to fire is not known
 * to work.
 *
 * The incident: a per-segment descriptor stride written as octal while the
 * comment beside it documented the decimal value. 0100 is 64. Every entry was
 * then read at the wrong address and reported absent.
 */
static const unsigned long desc_stride = 0100u;   /* 0o144 = 100 bytes */
