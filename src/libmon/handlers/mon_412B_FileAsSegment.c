/*
 * MON 412B (266 decimal): FileAsSegment (FSCNT)
 *
 * Connects a file as a segment to your domain. You can then access the file as a logical segment. This reduces the access time.
 * 
 * - The file must be open. The access must be specified in the OpenFile call.
 * - The file is disconnected when it is closed.
 * - A file may be connected to several processes simultaneously. It is your responsibility to synchronize simultaneous accesses.
 * - You may not use ReadFromFile (mon 117) or WriteToFile (mon 120) on a file which is connected to a segment. Refer to these monitor calls for further details.
 *
 * Parameters:
 *   [I] FileNo (INTEGER2): input
 *   [I] LogSegmentNo (INTEGER2): input
 *   [I] AccessType (INTEGER2): input
 *   [O] SegmentNo (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_412B_FileAsSegment(MonContext* ctx) {
    /* TODO: Implement FileAsSegment (FSCNT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "LogSegmentNo");
    MON_LOG_IN_WORD(ctx, 2, "AccessType");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
