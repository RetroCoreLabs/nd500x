/*
 * MON 123B [RELES/ReleaseResource]
 *
 * Releases a reserved device or file. The resource can then be used by
 * another program. You reserve a device or opened file with ReserveResource.
 * Some devices, e.g. terminals, have both an input and output part. You can
 * only release one part with each ReleaseResource call.
 *
 * - A normal termination of an RT program releases all resources.
 * - Reserve the device with ReserveResource or ForceReserve.
 * - CloseFile or @CLOSE-FILE releases reserved files.
 *
 * Parameters:
 *   [I] DeviceNumber (WORD): Logical device number
 *   [I] IOFlag (WORD): 0=input part, 1=output part
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"

MonResult mon_123B_ReleaseResource(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_123B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read input parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t io_flag = mon_read_param_word(ctx, 1);

    MON_LOG_IN_WORD(ctx, 0, "DeviceNumber");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");

    mon_log(MON_LOG_DEBUG, MON_ID_123B ": IN: DeviceNo=%o, IOFlag=%o",
            device_no, io_flag);

    /* Call release API. Return codes: 0 = released, -2 = valid device but not
     * reserved, -1 = invalid device number, -52 = invalid io_flag. */
    int result = mon_release_device(device_no, (uint8_t)io_flag);

    if (result == -2) {
        /* Carved L07 RELES: releasing an unreserved (but valid) resource is an
         * idempotent SUCCESS no-op - error 005B belongs to the I/O operations
         * that REQUIRE a reservation, never to RELES itself. */
        mon_log(MON_LOG_DEBUG, MON_ID_123B ": Device %o (%s) not reserved - success no-op",
                device_no, io_flag == 0 ? "input" : "output");
        mon_set_success(ctx);
        return MON_SUCCESS;
    }
    if (result < 0) {
        /* Genuinely bad parameter (invalid device number or io_flag). */
        mon_log(MON_LOG_INFO, MON_ID_123B ": Device %o (%s) release rejected (bad parameter)",
                device_no, io_flag == 0 ? "input" : "output");
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    mon_log(MON_LOG_INFO, MON_ID_123B ": OUT: Device %o (%s) released",
            device_no, io_flag == 0 ? "input" : "output");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
