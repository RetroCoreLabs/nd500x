/*
 * MON 336B (222 decimal): Terminal / TerminalFunction (IOMTY)
 *
 * I/O multifunction call: changes the attributes of terminal and terminal access
 * device (TAD) input/output. Also configures NET/One interfaces and SCSI disks.
 * The number of parameters varies by function, so they are passed in an array.
 *
 * PARAMETERS - the ND-500 form is FOUR arguments, from the manual's own
 * ASSEMBLY-500 example (ND-860228.2 EN page 503):
 *
 *     CALLG TerminalFunction, 4, Func, Size, ParamArray, Status2
 *     IF K GO Error
 *     W1 =: Status1
 *
 *   [I]  FunctionCode  - see the table below
 *   [I]  ArrayLength   - array size (no. of parameters); must be >= the minimum
 *                        for the function
 *   [IO] ParameterArray- function parameter array (max 760B)
 *   [O]  Status2       - standard error code (appendix A)
 *
 * NOTE: the extracted YAML lists only THREE parameters - it dropped Status2.
 * Status2 is an OUT parameter; failing to write it is the 412B FSCNT / 144B
 * MAGTP defect class, where a call returns success but the caller reads a stale
 * value and fails far away from the real cause.
 *
 * FUNCTION CODES ARE OCTAL. Primary source: ND-860228.2 EN pages 504-505
 * (`/mnt/e/Dev/Ronny/NDInsight/Reference-Manuals/ND-860228-2-EN SINTRAN III Monitor Calls.md`).
 * The ranges are 0-77, 100-177, 200-277, 300-377; the list runs 7 -> 10, and
 * later entries are written 112B/113B, both confirming octal.
 *
 *    0 Set logical device number to become own terminal
 *    1 Reset to original own terminal
 *    2 Set character conversion mode for device no. 0
 *    3 Get character conversion mode for device no. 0
 *    4 Set echo strategy                 5 Get echo strategy
 *    6 Set break strategy                7 Get break strategy
 *   10 Set communication mode for terminal
 *   11 Get communication mode for terminal
 *   12 Set/reset 8-bit unmodified input and output      <-- IMPLEMENTED
 *   13 List logical device numbers of terminals and TADs in system
 *   14 Display functions                15 Change signals on an RS-232 (V.24) connection
 *   16 Set/reset terminal interface in/from test mode
 *   17 Connect NIU on specified device number (VSX only)
 *   20 Disconnect NIU on specified device number (VSX only)
 *   21 Read/write on a PIO interface     22 Get magic number for a TAD (VSX only)
 *   23 Access the CI window on a NQTS controller (VSX only)
 *   24 Get no-reserve status of a device (VSX only)
 *   25 Set/reset no-reserve status of a device (VSX only)
 *  100 Return function parameters set by functions 101-177
 *  101 Set terminal type                102 Set escape or local character
 *  103 Xon/Xoff protocol, input control 104 Xon/Xoff protocol, output control
 *  105 Set Xon/Xoff only or dual        106 Set character length
 *  107 Set baud rate for terminal       110 Set number of stop bits
 *  111 Set terminal to printer / reset  112 Set half or full duplex on terminal
 *  113 Set/reset variable speed         114 Set/reset terminal connected to printer
 *  200-203 NOTS configuration (VSX only)
 *  300-303 SCSI device definition (VSX only)
 *
 * Manual notes (page 505): device number 1 is the console terminal; for a
 * background program logical device number 0 means own terminal.
 */

#include "../mon.h"
#include "../mon_errors.h"
#include "../mon_terminal_state.h"

/* Function codes, written as the raw values the caller passes.
 * The manual numbers them in OCTAL, so 012B == 10 decimal. */
#define IOMTY_FUNC_SET_8BIT_UNMODIFIED  012   /* 12B - set/reset 8-bit unmodified I/O */

/* Read a big-endian 32-bit word from guest memory. */
static uint32_t iomty_read_word(MonContext* ctx, uint32_t addr) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v = (v << 8) | ctx->read_byte(ctx->cpu, addr + i);
    return v;
}

/* Write a big-endian 32-bit word to guest memory. */
static void iomty_write_word(MonContext* ctx, uint32_t addr, uint32_t value) {
    for (int i = 0; i < 4; i++)
        ctx->write_byte(ctx->cpu, addr + i, (uint8_t)((value >> (24 - 8 * i)) & 0xFF));
}

/*
 * Function 12B - Set/reset 8-bit unmodified input/output.
 * "Unmodified means no parity on the most significant bit in byte."
 *
 *   Word 1 = logical device number
 *   Word 2 = 8-bit status: 0 = set 8-bit unmodified I/O
 *                          1 = reset to 7-bit I/O, parity on most significant bit
 *
 * Minimum function parameter array size is 2 (manual page 506 rules table).
 *
 * The ND linker issues exactly this at startup: Func=12B, Size=2,
 * ParamArray=[1, 0] - i.e. "set 8-bit unmodified on the console terminal".
 */
static MonResult iomty_set_8bit(MonContext* ctx, uint32_t array_addr, uint32_t array_len) {
    if (array_len < 2) {
        mon_log(MON_LOG_WARN, "MON 336B IOMTY: function 12B needs array size >= 2, got %u", array_len);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B */
        return MON_ERROR;
    }

    uint32_t device_no  = iomty_read_word(ctx, array_addr);
    uint32_t bit_status = iomty_read_word(ctx, array_addr + 4);

    /* 0 means SET 8-bit unmodified; 1 means reset to 7-bit with parity. Note the
     * sense is inverted relative to the boolean we keep. */
    bool eight_bit = (bit_status == 0);
    mon_set_eight_bit_io(device_no, eight_bit);

    mon_log(MON_LOG_DEBUG,
            "MON 336B IOMTY: function 12B - device %o %s",
            device_no,
            eight_bit ? "set to 8-bit unmodified I/O (no parity on MSB)"
                      : "reset to 7-bit I/O (parity on MSB)");

    mon_set_success(ctx);
    return MON_SUCCESS;
}

MonResult mon_336B_Terminal(MonContext* ctx) {
    if (ctx->arg_count < 3) {
        mon_log(MON_LOG_WARN, "MON 336B IOMTY: Missing parameters (need at least 3, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B */
        return MON_ERROR;
    }

    uint32_t func       = mon_read_param_word(ctx, 0);
    uint32_t array_len  = mon_read_param_word(ctx, 1);
    uint32_t array_addr = ctx->arg_addresses[2];

    MON_LOG_IN_WORD(ctx, 0, "FunctionCode");
    MON_LOG_IN_WORD(ctx, 1, "ArrayLength");

    mon_log(MON_LOG_DEBUG, "MON 336B IOMTY: IN: Func=%o, ArrayLength=%u, ArrayAddr=0x%08X",
            func, array_len, array_addr);

    MonResult r;
    switch (func) {
    case IOMTY_FUNC_SET_8BIT_UNMODIFIED:
        r = iomty_set_8bit(ctx, array_addr, array_len);
        break;

    default:
        /* Every other function is a real terminal/NIU/SCSI attribute operation we
         * do not model. Return a real SINTRAN error rather than pretending to
         * succeed - a silent success here would let a caller act on an unchanged
         * parameter array, which is exactly the failure mode that made 144B MAGTP
         * so expensive to find. */
        mon_log(MON_LOG_WARN, "MON 336B IOMTY: function %o not implemented", func);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        r = MON_ERROR;
        break;
    }

    /* Status2 is the 4th ND-500 argument and is an OUT parameter carrying the
     * standard error code. The caller separately takes its own status from W1
     * ("W1 =: Status1" in the manual's example, which the linker does verbatim at
     * 0xB004C2C7). Only write it when the caller actually passed it. */
    if (ctx->arg_count >= 4)
        iomty_write_word(ctx, ctx->arg_addresses[3], (r == MON_SUCCESS) ? 0u : 1u);

    return r;
}
