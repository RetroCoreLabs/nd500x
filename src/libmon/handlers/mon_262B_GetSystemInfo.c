/*
 * MON 262B (178 decimal): GetSystemInfo (CPUST)
 *
 * Gets various system information. The system number, the CPU type, the
 * SINTRAN III version, the instruction set, the patch indicator, and the
 * system generation time are returned.
 *
 * Parameters:
 *   [I] Number (INTEGER): System number to query (usually 1)
 *   [O] Buffer (ARRAY of 7 INTEGERs):
 *       [0] System number
 *       [1] CPU type (500 for ND-500)
 *       [2] SINTRAN version (e.g., 0x4A00 for version J.0)
 *       [3] Instruction set (0 = standard)
 *       [4] Patch indicator
 *       [5] Generation time (high word)
 *       [6] Generation time (low word)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"

/* Simulated system values */
#define SYSTEM_NUMBER        1      /* System number */
#define CPU_TYPE_ND500       500    /* ND-500 CPU */

/* SINTRAN version encoding:
 * High byte = ASCII code of version letter (A-Z)
 * Low byte = sub-version number
 * Examples: 0x4100 = A.0, 0x4A00 = J.0, 0x4B01 = K.1
 * 0x4A = 74 = ASCII 'J', so 0x4A00 = SINTRAN III J.0 */
#define SINTRAN_VERSION_J    0x4A00 /* SINTRAN III version J.0 ('J' = 0x4A) */

#define INSTRUCTION_SET      0      /* Standard instruction set */
#define PATCH_INDICATOR      0      /* No patches */
#define GENERATION_TIME_HI   0      /* Generation time (not used) */
#define GENERATION_TIME_LO   0

MonResult mon_262B_GetSystemInfo(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, "MON 262B CPUST: Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read system number (not really used, but log it) */
    uint32_t sys_num = mon_read_param_word(ctx, 0);
    uint32_t buffer_addr = ctx->arg_addresses[1];

    MON_LOG_IN_WORD(ctx, 0, "Number");

    mon_log(MON_LOG_DEBUG, "MON 262B CPUST: Querying system %u", sys_num);

    /* Write 7 words to buffer */
    /* [0] System number */
    ctx->write_word(ctx->cpu, buffer_addr + 0, SYSTEM_NUMBER);

    /* [1] CPU type (500 for ND-500) */
    ctx->write_word(ctx->cpu, buffer_addr + 4, CPU_TYPE_ND500);

    /* [2] SINTRAN version (J.0) */
    ctx->write_word(ctx->cpu, buffer_addr + 8, SINTRAN_VERSION_J);

    /* [3] Instruction set */
    ctx->write_word(ctx->cpu, buffer_addr + 12, INSTRUCTION_SET);

    /* [4] Patch indicator */
    ctx->write_word(ctx->cpu, buffer_addr + 16, PATCH_INDICATOR);

    /* [5] Generation time (high) */
    ctx->write_word(ctx->cpu, buffer_addr + 20, GENERATION_TIME_HI);

    /* [6] Generation time (low) */
    ctx->write_word(ctx->cpu, buffer_addr + 24, GENERATION_TIME_LO);

    mon_log(MON_LOG_DEBUG, "MON 262B CPUST: CPU=%u, Version=0x%04X",
            CPU_TYPE_ND500, SINTRAN_VERSION_J);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
