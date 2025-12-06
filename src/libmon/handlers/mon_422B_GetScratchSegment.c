/*
 * MON 422B (274 decimal): GetScratchSegment (GSWSP)
 *
 * Connects an empty data segment to the user's domain and reserves space
 * for it on the swap file. The segment is assigned the default name
 * "SCRATCH-SEGMENT:DSEG".
 *
 * Parameters (all are pointers to 32-bit WORD on ND-500):
 *   [O] WorkspaceSize (W INTEGER ptr): Returns the allocated workspace size in bytes.
 *   [I] LogSegmentNo (W INTEGER ptr): Logical segment number to use.
 *       Use 0 for system to select first available free segment.
 *   [O] RetLogSegmentNo (W INTEGER ptr): Returns the logical segment number
 *       actually selected.
 *
 * IMPORTANT: All arguments are longword pointers (mode=10).
 * - arg0: GSWSP writes the allocated workspace size here (do NOT read old value)
 * - arg1: Read requested segment number from here
 * - arg2: GSWSP writes the assigned segment number here
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"

/* Default workspace size to allocate (8KB) */
#define DEFAULT_WORKSPACE_SIZE  0x2000

/* Next available scratch segment number (starts at 16 to avoid low segments) */
static uint32_t g_next_scratch_segment = 16;

MonResult mon_422B_GetScratchSegment(MonContext* ctx) {
    uint32_t workspace_size;
    uint32_t requested_segment;
    uint32_t assigned_segment;

    /* Read requested segment from arg1 (input parameter) */
    requested_segment = mon_read_param_word(ctx, 1);

    /* Determine segment number to use */
    if (requested_segment == 0) {
        /* Auto-assign next available segment */
        assigned_segment = g_next_scratch_segment++;

        /* Wrap around if we exceed reasonable segment range */
        if (g_next_scratch_segment > 30) {
            g_next_scratch_segment = 16;
        }
    } else {
        /* Use requested segment number */
        assigned_segment = requested_segment;
    }

    /* Set workspace size - this is what we allocate */
    workspace_size = DEFAULT_WORKSPACE_SIZE;

    /* Log argument addresses for debugging */
    mon_log(MON_LOG_DEBUG, "MON 422B GSWSP: arg_count=%u, arg0_addr=0x%08X, arg1_addr=0x%08X, arg2_addr=0x%08X",
            ctx->arg_count,
            ctx->arg_count > 0 ? ctx->arg_addresses[0] : 0,
            ctx->arg_count > 1 ? ctx->arg_addresses[1] : 0,
            ctx->arg_count > 2 ? ctx->arg_addresses[2] : 0);

    /* Write output parameters as 32-bit words */
    /* arg0: Write allocated workspace size (OUTPUT - do not read!) */
    if (ctx->arg_count >= 1) {
        mon_log(MON_LOG_DEBUG, "MON 422B GSWSP: Writing size 0x%08X to addr 0x%08X",
                workspace_size, ctx->arg_addresses[0]);
        mon_write_param_word(ctx, 0, workspace_size);
    }

    /* arg2: Write assigned segment number */
    if (ctx->arg_count >= 3) {
        mon_log(MON_LOG_DEBUG, "MON 422B GSWSP: Writing seg %u to addr 0x%08X",
                assigned_segment, ctx->arg_addresses[2]);
        mon_write_param_word(ctx, 2, assigned_segment);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, "MON 422B GSWSP: allocated_size=%u bytes, requested_seg=%u, assigned_seg=%u",
            workspace_size, requested_segment, assigned_segment);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
