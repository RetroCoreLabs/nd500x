/*
 * MON 37B (31 decimal): ReadADChannel (AIRDW)
 *
 * Reads an analog to digital channel.
 * 
 * ### PARAMETERS
 *
 * Parameters:
 *   [I] NoOfChannels (INTEGER): input
 *   [I] Channel (INTEGER): input
 *   [O] Buffer (ARRAY): output
 *   [O] ReturnValue (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_37B_ReadADChannel(MonContext* ctx) {
    /* TODO: Implement ReadADChannel (AIRDW) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "NoOfChannels");
    MON_LOG_IN_WORD(ctx, 1, "Channel");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
