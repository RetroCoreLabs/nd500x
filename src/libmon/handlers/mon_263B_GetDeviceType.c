/*
 * MON 263B (179 decimal): GetDeviceType (GDEVT)
 *
 * Gets the device type (terminal, floppy, mass-storage file, ...) and how the
 * device is to be handled. Spec: SINTRAN III Monitor Calls (ND-860228.2 EN).
 *
 * Parameters:
 *   [I] DeviceNo (INTEGER): logical device number (1 = own terminal); appendix B
 *   [I] IOFlag   (INTEGER): 0 = input part, 1 = output part
 *   [O] DevType  (INTEGER): device type (values below)
 *   [O] DevAttr  (INTEGER4): device attribute bits (below)
 *
 * DevType values (manual):
 *   0 Unspecified   1 Terminal   2 Terminal access device (TAD)
 *   3 Communication channel      4 Internal block device
 *   5 Floppy disk drive          6 Magnetic tape station
 *   7 Mass-storage file
 *
 * DevAttr bits (manual):
 *   0 InByte/OutByte allowed      1 StartOnInterrupt allowed
 *   2 DeviceControl allowed       3 Block calls allowed
 *   4 ClearDevice available       5 Reservation not needed
 *   6 COSMOS remote open file    10B NOTS terminal    11B MTAD device
 */

#include "../mon.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"

/* DevType */
#define GDEVT_UNSPECIFIED   0
#define GDEVT_TERMINAL      1
#define GDEVT_MASS_FILE     7

/* DevAttr bits */
#define GDEVT_ATTR_INOUTBYTE   (1u << 0)
#define GDEVT_ATTR_BLOCKCALLS  (1u << 3)
#define GDEVT_ATTR_NO_RESERVE  (1u << 5)

MonResult mon_263B_GetDeviceType(MonContext* ctx) {
    if (ctx->arg_count < 4) {
        mon_log(MON_LOG_WARN, "MON 263B GDEVT: Missing parameters (need 4, got %u)", ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B */
        return MON_ERROR;
    }

    uint32_t device_no = mon_read_param_word(ctx, 0);
    uint32_t io_flag   = mon_read_param_word(ctx, 1);
    MON_LOG_IN_WORD(ctx, 0, "DeviceNo");
    MON_LOG_IN_WORD(ctx, 1, "IOFlag");

    uint32_t dev_type;
    uint32_t dev_attr;

    if (is_mass_storage_file(device_no)) {
        /* An open mass-storage file - which is what the ND linker asks about, for
         * the DDBTABLES file it just opened. Byte I/O and block I/O both apply;
         * an already-open file needs no reservation. */
        dev_type = GDEVT_MASS_FILE;
        dev_attr = GDEVT_ATTR_INOUTBYTE | GDEVT_ATTR_BLOCKCALLS | GDEVT_ATTR_NO_RESERVE;
    } else if (device_no == 1 || is_terminal(device_no) || is_character_device(device_no)) {
        /* Device 1 is the console; the character-device range covers the terminal.
         * A terminal does byte I/O; it is the program's own terminal, so no
         * reservation is needed. */
        dev_type = GDEVT_TERMINAL;
        dev_attr = GDEVT_ATTR_INOUTBYTE | GDEVT_ATTR_NO_RESERVE;
    } else {
        /* Not modelled - report Unspecified with no capabilities rather than
         * guess. UNVERIFIED: whether SINTRAN ever returns 0 for a live device, or
         * errors instead. No program has exercised this branch. */
        dev_type = GDEVT_UNSPECIFIED;
        dev_attr = 0;
    }

    (void)io_flag;  /* Input vs output part does not change the answer for the
                     * device classes modelled here; kept for the log/contract. */

    /* [O] DevType (INTEGER = 32-bit word), [O] DevAttr (INTEGER4). The manual
     * says DevAttr comes back "in the combined A and D registers", but this ND-500
     * CALLG passes four parameter ADDRESSES, so both OUT values are written to the
     * parameter block. Writing them is the point of the call - a GDEVT that
     * returned success without writing DevType/DevAttr would be the 412B FSCNT /
     * 144B MAGTP OUT-parameter defect class again. */
    mon_write_param_word(ctx, 2, dev_type);
    mon_write_param_dword(ctx, 3, (uint64_t)dev_attr);
    MON_LOG_OUT_WORD(ctx, 2, "DevType");

    mon_log(MON_LOG_DEBUG, "MON 263B GDEVT: device %o (io=%u) -> type=%u attr=0x%X",
            device_no, io_flag, dev_type, dev_attr);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
