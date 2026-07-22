/*
 * MON 150B (104 decimal): CAMACGLRegister (GL)
 *
 * Read the CAMAC GL (Graded LAM - \"look at me\") register or the last CAMAC identification number. See under CAMACfunction (mon 147) for more general information.
 * 
 * When a program requests interrupt, it generates a LAM to the control station. Up to 23 such signals may arrive, but for efficient handling in the computer, these must be reduced to 16 lines called Graded LAMs. The GL register, which may be read by the program, holds a bit mask giving the current LAM requests (bit=1 indicates LAM). The different graded LAM lines have individual IDENT codes (Bits 0-3: Graded LAM code. Bits 4-7: crate code. Bit 8=1 : CAMAC). A LAM Mask Register is used to enable (bit=1) or disable (bit=0) any of the 16 graded LAMs. Each time an IDENT code is read, the associated bit in the MASK register is cleared and must be set by the program to enable a new interrupt.
 *
 * Parameters:
 *   [I] Flag (INTEGER2): input
 *   [I] CrateNo (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_150B_CAMACGLRegister(MonContext* ctx) {
    /* TODO: Implement CAMACGLRegister (GL) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Flag");
    MON_LOG_IN_WORD(ctx, 1, "CrateNo");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
