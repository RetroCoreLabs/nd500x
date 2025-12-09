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

MonResult mon_422B_GetScratchSegment(MonContext* ctx) {
    uint32_t segment_size;
    uint32_t requested_segment;
    uint32_t assigned_segment;

    /* Check if callback is available */
    if (!ctx->allocate_segment) {
        mon_log(MON_LOG_ERROR, "MON 422B GSWSP: allocate_segment callback not available");
        mon_set_error(ctx, -1);
        return MON_ERROR;
    }

    /* Read input parameters */
    segment_size = mon_read_param_word(ctx, 0);        /* arg0: segment size in bytes */
    requested_segment = mon_read_param_word(ctx, 1);   /* arg1: requested segment number */

    /* Call allocation callback - pass 0xFF as sentinel to indicate "use current domain" */
    /* The callback will get the domain from cpu->CED internally */
    int rc = ctx->allocate_segment(ctx->cpu, ctx->machine, 0xFF,
                                    requested_segment, segment_size,
                                    &assigned_segment);

    if (rc != 0) {
        /* Allocation failed - set error code */
        mon_log(MON_LOG_ERROR, "MON 422B GSWSP: allocation failed with error %d", rc);
        mon_set_error(ctx, rc);
        return MON_ERROR;
    }

    /* Write output parameter: arg2 = assigned segment number */
    if (ctx->arg_count >= 3) {
        mon_write_param_word(ctx, 2, assigned_segment);
    }

    /* Log the call */
    mon_log(MON_LOG_INFO, "MON 422B GSWSP: segment_size=%u bytes, requested_seg=%u, assigned_seg=%u", segment_size, requested_segment, assigned_segment);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
