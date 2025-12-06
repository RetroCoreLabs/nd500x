/*
 * MON 422B (274 decimal): GetScratchSegment (GSWSP)
 *
 * Connects an empty data segment to the user's domain and reserves space
 * for it on the swap file. The segment is assigned the default name
 * "SCRATCH-SEGMENT:DSEG".
 *
 * Parameters (all are pointers to 32-bit WORD on ND-500):
 *   [I] SegmentSize (W INTEGER ptr): Segment size in bytes.
 *   [I] LogSegmentNo (W INTEGER ptr): Logical segment number to use.
 *       Use 0 for system to select first available free segment.
 *   [O] RetLogSegmentNo (W INTEGER ptr): Returns the logical segment number
 *       actually selected (only written if LogSegmentNo was 0).
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"

/* Next available scratch segment number (starts at 16 to avoid low segments) */
static uint32_t g_next_scratch_segment = 16;

MonResult mon_422B_GetScratchSegment(MonContext* ctx) {
    uint32_t segment_size;
    uint32_t requested_segment;
    uint32_t assigned_segment;

    /* Read input parameters */
    segment_size = mon_read_param_word(ctx, 0);        /* arg0: segment size in bytes */
    requested_segment = mon_read_param_word(ctx, 1);   /* arg1: requested segment number */

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

    /* Write output parameter: arg2 = assigned segment number */
    if (ctx->arg_count >= 3) {
        mon_write_param_word(ctx, 2, assigned_segment);
    }

    /* Log the call */
    mon_log(MON_LOG_INFO, "MON 422B GSWSP: segment_size=%u bytes, requested_seg=%u, assigned_seg=%u",
            segment_size, requested_segment, assigned_segment);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
