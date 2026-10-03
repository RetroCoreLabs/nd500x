/*
 * radix_gate fixture: KNOWN GOOD. Every line here is a shape the gate must
 * NOT flag, because a gate that fires on correct code is one people learn to
 * skip - which is the failure it exists to prevent.
 */

/* Octal literal, octal comment. The ND machine's addresses ARE octal and this
 * is the honest way to write them. */
static const unsigned long swpdecoder = 012243u;   /* 012243 */

/* Octal literal described in words, no contradicting digit string. */
static const unsigned long trap_page_fault = 046u;  /* 46B page fault */

/* Decimal literal with the octal in the comment - the prescribed direction. */
static const unsigned long desc_stride = 100u;     /* 0o144 bytes */

/* Hex is unambiguous. */
static const unsigned long msg_block = 0x8E30u;    /* 8E30 */

/* Single octal digit: same value in both radices, so no contradiction exists. */
static const unsigned long two = 02u;              /* 2 */

/* A comment number that is not the literal's digits. */
static const unsigned long pages = 0102u;          /* 66 decimal */
