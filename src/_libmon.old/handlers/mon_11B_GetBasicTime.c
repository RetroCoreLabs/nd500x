/*
 * MON 11B [TIME/GetBasicTime]
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
 *
 * Note: Result is returned in W1 register for ASSEMBLY-500 compatibility,
 * and also written to memory if an output parameter address is provided
 * (for high-level language compatibility).
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../../cpu/cpu_protos.h"

MonResult mon_11B_GetBasicTime(MonContext* ctx) {
    if (!ctx || !ctx->cpu) {
        mon_set_error(ctx, -1);
        return MON_ERROR;
    }

    /* Get instruction count from CPU and convert to basic time units */
    Nd500Cpu* cpu = (Nd500Cpu*)ctx->cpu;
    uint64_t basic_time = cpu->instruction_count / 40000;
    uint32_t time_value = (uint32_t)(basic_time & 0xFFFFFFFF);

    /* Return result in W1 register (required for ASSEMBLY-500 callers) */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, time_value);
    }

    /* Also write to memory if address provided (for high-level languages) */
    if (ctx->arg_count >= 1) {
        mon_write_param_word(ctx, 0, time_value);
    }

    mon_log(MON_LOG_DEBUG, MON_ID_11B ": IN: (none)");
    mon_log(MON_LOG_DEBUG, MON_ID_11B ": OUT: BasicTime=%u (%.2f sec)",
            time_value, (double)time_value / 50.0);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
