/*
 * MON 155B (109 decimal): GraphicFunction (GRAPH)
 *
 * Executes various functions on a graphic peripheral, such as a NORDCOM terminal, a pen plotter, or a Textronix display.
 *
 * Parameters:
 *   [I] Ycoor (INTEGER): input
 *   [I] Xcoor (INTEGER): input
 *   [I] Code (INTEGER): input
 *   [I] DeviceNo (INTEGER): input
 *   [I] Func (INTEGER): input
 *   [O] ReturnValue (INTEGER): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_155B_GraphicFunction(MonContext* ctx) {
    /* TODO: Implement GraphicFunction (GRAPH) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Ycoor");
    MON_LOG_IN_WORD(ctx, 1, "Xcoor");
    MON_LOG_IN_WORD(ctx, 2, "Code");
    MON_LOG_IN_WORD(ctx, 3, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 4, "Func");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
