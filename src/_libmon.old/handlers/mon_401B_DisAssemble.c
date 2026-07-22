/*
 * MON 401B (257 decimal): DisAssemble (DIASS)
 *
 * Disassembles one machine instruction on the ND-500. Output is the instruction in ASSEMBLY-500 language. See the manual ND-500 ASSEMBLER Reference Manual (ND-860113).
 *
 * Parameters:
 *   [I] ProgPointer (INTEGER2): input
 *   [O] ReturnString (STRING): output
 *   [I] MaxNoOfChar (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_401B_DisAssemble(MonContext* ctx) {
    /* TODO: Implement DisAssemble (DIASS) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "ProgPointer");
    MON_LOG_IN_WORD(ctx, 2, "MaxNoOfChar");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
