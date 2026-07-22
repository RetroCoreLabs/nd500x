/*
 * MON 67B [OSIZE/OutBufferSpace]
 *
 * Gets the number of free bytes in the output buffer (number of bytes
 * which can be written before the program must wait).
 *
 * Terminals and other character devices place output in a buffer.
 * Monitor calls like OutByte write to this buffer.
 *
 * Use ExecutionInfo to get the logical device number for terminals.
 * You can specify 1 for your own terminal.
 * This monitor call is not available for internal devices.
 *
 * Parameters:
 *   [I] DeviceNumber (INTEGER): Logical device number. Use 1 for your own terminal.
 *   [O] NoOfBytes (INTEGER): Number of free bytes in output buffer (returned in I1).
 *
 * Returns:
 *   K flag set on error, error code in I1
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"

#define DEFAULT_BUFFER_SIZE 2048

MonResult mon_67B_OutBufferSpace(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 1) {
        mon_log(MON_LOG_WARN, MON_ID_67B ": Missing parameters (need 1, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read device number */
    uint32_t device_no = mon_read_param_word(ctx, 0);

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");

    mon_log(MON_LOG_DEBUG, MON_ID_67B ": IN: DeviceNo=%o", device_no);

    /* File numbers are not allowed */
    if (is_mass_storage_file(device_no)) {
        mon_log(MON_LOG_WARN, MON_ID_67B ": File numbers not supported (device %o)", device_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* For emulation, we always report a large buffer available
     * since we don't actually have output buffer constraints */
    uint32_t no_of_bytes = DEFAULT_BUFFER_SIZE;

    /* Return result in I1 register */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, no_of_bytes);
    }

    /* Also write to output parameter if provided */
    if (ctx->arg_count >= 2) {
        mon_write_param_word(ctx, 1, no_of_bytes);
    }

    mon_log(MON_LOG_DEBUG, MON_ID_67B ": OUT: Device %o -> %o bytes free", device_no, no_of_bytes);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
