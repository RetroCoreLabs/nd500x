/*
 * MON 406B (262 decimal): AccessRTCommon (RWRTC)
 *
 * Reads from or writes to RT common from an ND-500 program. RT common is an area in physical memory where RT programs may exchange data.
 * 
 * - Some SINTRAN III systems are generated without RT common. The size of RT common is defined at the time of system generation. The size may be increased by up to 8 pages by the SINTRAN-SERVICE-PROGRAM command DEFINE-RTCOMMON-SIZE (see the SINTRAN III Commands Reference Manual, ND-860128, for more information).
 *
 * Parameters:
 *   [I] Func (INTEGER): input
 *   [I] RTCommon (INTEGER): input
 *   [I] NoOfBytes (INTEGER): input
 *   [IO] Buffer (ARRAY): in/out
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_406B_AccessRTCommon(MonContext* ctx) {
    /* TODO: Implement AccessRTCommon (RWRTC) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Func");
    MON_LOG_IN_WORD(ctx, 1, "RTCommon");
    MON_LOG_IN_WORD(ctx, 2, "NoOfBytes");
    MON_LOG_IN_WORD(ctx, 3, "Buffer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
