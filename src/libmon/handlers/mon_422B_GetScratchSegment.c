/*
 * MON 422B (274 decimal): GetScratchSegment (GSWSP)
 *
 * Connects an empty data segment to the user's domain and reserves space
 * for it on the swap file. The segment is assigned the default name
 * "SCRATCH-SEGMENT:DSEG".
 *
 * Parameters (all are 32-bit WORD on ND-500):
 *   [I] SizeInBytes (W INTEGER): Segment size in bytes.
 *   [I] LogSegmentNo (W INTEGER): Logical segment number to use.
 *       Use 0 for system to select first available free segment.
 *   [O] RetLogSegmentNo (W INTEGER): Returns the logical segment number
 *       actually selected.
 *
 * Note: On ND-500, INTEGER = 32-bit Word (W type).
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"

/* Next available scratch segment number (starts at 16 to avoid low segments) */
static uint32_t g_next_scratch_segment = 16;

MonResult mon_422B_GetScratchSegment(MonContext* ctx) {
    uint32_t size_in_bytes;
    uint32_t requested_segment;
    uint32_t assigned_segment;

    /* Read input parameters as 32-bit words (ND-500 INTEGER = W) */
    size_in_bytes = mon_read_param_word(ctx, 0);
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

    /* Write output parameter as 32-bit word (ND-500 INTEGER = W) */
    if (ctx->arg_count >= 3) {
        mon_write_param_word(ctx, 2, assigned_segment);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, "MON 422B GSWSP: size=%u bytes, requested_seg=%u, assigned_seg=%u",
            size_in_bytes, requested_segment, assigned_segment);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
