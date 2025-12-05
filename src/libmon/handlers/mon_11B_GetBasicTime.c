/*
 * MON 11B (9 decimal): GetBasicTime (TIME)
 *
 * **Time**
 *
 * Gets the current internal time. The internal time is specified in basic time units.
 * There are 50 basic time units in a second.
 *
 * - The internal time is set to 0 each time SINTRAN III is started.
 *
 * Parameters:
 *   [O] BasicTime (LONGINT): output - current time in basic time units
 *
 * Implementation:
 *   Uses CPU instruction counter divided by 40000 to approximate basic time units.
 *   At ~2 MHz execution, 40000 instructions = 1/50th second = 1 basic time unit.
 */

#include "../mon.h"
#include "../../cpu/cpu_protos.h"

MonResult mon_11B_GetBasicTime(MonContext* ctx) {
    if (!ctx || !ctx->cpu) {
        mon_set_error(ctx, -1);
        return MON_ERROR;
    }

    /* Get instruction count from CPU and convert to basic time units */
    Nd500Cpu* cpu = (Nd500Cpu*)ctx->cpu;
    uint64_t basic_time = cpu->instruction_count / 40000;

    /* Write result to output parameter (LONGINT = 32-bit) */
    if (ctx->arg_count >= 1) {
        mon_write_param_word(ctx, 0, (uint32_t)(basic_time & 0xFFFFFFFF));
        mon_log(MON_LOG_DEBUG, "MON 11B TIME: instruction_count=%llu -> basic_time=%llu",
                (unsigned long long)cpu->instruction_count, (unsigned long long)basic_time);
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
