/*
 * MON 422B (274 decimal): GetScratchSegment (GSWSP)
 *
 * Connects an empty data segment to the user's domain and reserves space
 * for it on the swap file. The segment is assigned the default name
 * "SCRATCH-SEGMENT:DSEG".
 *
 * Parameters:
 *   [I] SizeInBytes (INTEGER): Segment size in bytes.
 *   [I] LogSegmentNo (INTEGER): Logical segment number to use.
 *       Use 0 for system to select first available free segment.
 *   [O] RetLogSegmentNo (INTEGER): Returns the logical segment number
 *       actually selected.
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"

/* Next available scratch segment number (starts at 16 to avoid low segments) */
static uint16_t g_next_scratch_segment = 16;

MonResult mon_422B_GetScratchSegment(MonContext* ctx) {
    uint32_t size_in_bytes;
    uint16_t requested_segment;
    uint16_t assigned_segment;

    /* Read input parameters */
    size_in_bytes = mon_read_param_word(ctx, 0);
    requested_segment = (uint16_t)mon_read_param_word(ctx, 1);

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

    /* Write output parameter - the assigned segment number */
    if (ctx->arg_count >= 3) {
        mon_write_param_halfword(ctx, 2, assigned_segment);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, "MON 422B GSWSP: size=%u bytes, requested_seg=%u, assigned_seg=%u",
            size_in_bytes, requested_segment, assigned_segment);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
