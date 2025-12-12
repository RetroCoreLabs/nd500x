/*
 * MON 122B (82 decimal): ReserveResource (RESRV)
 *
 * Reserves a device or file for your program only. You release it with
 * ReleaseResource. Some devices, e.g. terminals, have both an input and
 * output part. You can only reserve one part with each ReserveResource call.
 *
 * - A normal termination of an RT program releases all resources.
 * - Release the device with ReleaseResource or ForceRelease.
 * - A background program does not release a resource when you press ESCAPE.
 *
 * Parameters:
 *   [I] DeviceNo (WORD): Logical device number
 *   [I] IOFlag (WORD): 0=input part, 1=output part
 *   [I] WaitFlag (WORD): 0=wait if busy, 1=return immediately
 *   [O] Status (WORD): 0=success, negative=unavailable (if WaitFlag=1)
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_file_table.h"

MonResult mon_122B_ReserveResource(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, "MON 122B RESRV: Missing parameters (need 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read input parameters */
    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t io_flag = mon_read_param_word(ctx, 1);
    uint32_t wait_flag = mon_read_param_word(ctx, 2);

    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");
    MON_LOG_IN_WORD(ctx, 2, "WaitFlag");

    mon_log(MON_LOG_DEBUG, "MON 122B RESRV: DeviceNo=%u (octal %o), IOFlag=%u, WaitFlag=%u",
            device_no, device_no, io_flag, wait_flag);

    /* Call reservation API */
    int result = mon_reserve_device(device_no, (uint8_t)io_flag, wait_flag == 0);

    /* If wait_flag != 0 and we have a status output parameter, write result */
    if (wait_flag != 0 && ctx->arg_count > 3) {
        mon_write_param_word(ctx, 3, (uint32_t)result);
        mon_log(MON_LOG_DEBUG, "MON 122B RESRV: Status=%d written to output parameter", result);
    }

    if (result < 0) {
        mon_log(MON_LOG_INFO, "MON 122B RESRV: Device %u (%s) reservation failed",
                device_no, io_flag == 0 ? "input" : "output");
        mon_set_error(ctx, 57);  /* Device reserved by other */
        return MON_ERROR;
    }

    mon_log(MON_LOG_INFO, "MON 122B RESRV: Device %u (%s) reserved successfully",
            device_no, io_flag == 0 ? "input" : "output");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
