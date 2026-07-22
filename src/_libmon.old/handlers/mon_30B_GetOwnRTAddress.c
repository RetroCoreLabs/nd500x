/*
 * MON 30B [GETRT/GetOwnRTAddress]
 *
 * Gets the address of the calling program's RT description. Background
 * programs get the RT description address of the RT program which controls
 * the terminal.
 *
 * For RT programs (execMode=3), this returns a pointer to the RT description
 * structure which contains scheduling and timing information.
 *
 * Parameters:
 *   [O] RTDescrAddress (INTEGER): RT description address (returned in W1)
 *
 * RT-Description Structure (26 words / 104 bytes on ND-500):
 *   Offset 0:  TLINK      - Time queue link (0 if not in time queue)
 *   Offset 1:  STATE_PRIO - Bits 0-7: priority, Bits 8-13: state flags
 *   Offset 2:  DTIM1      - Scheduled time (high word)
 *   Offset 3:  DTIM2      - Scheduled time (low word)
 *   Offset 4:  DTN1       - Time interval (high word)
 *   Offset 5:  DTN2       - Time interval (low word)
 *   Offset 6:  STADR      - Start address (entry point)
 *   Offset 7:  SEGM       - Segment numbers (packed)
 *   Offset 8:  DPREG      - Saved P register (program counter)
 *   Offset 9:  DXREG      - Saved X register (I1 on ND-500)
 *   Offset 10: DTREG      - Saved T register (0 on ND-500)
 *   Offset 11: DAREG      - Saved A register (A1 on ND-500)
 *   Offset 12: DDREG      - Saved D register (0 on ND-500)
 *   Offset 13: DLREG      - Saved L register
 *   Offset 14: DSREG      - Saved status register (PS on ND-500)
 *   Offset 15: DBREG      - Saved B register
 *   Offset 16: WLINK      - Waiting queue link
 *   Offset 17: ACTSEG1    - Active segment 1
 *   Offset 18: ACTSEG2    - Active segment 2
 *   Offset 19: ACTPRI     - Actual priority
 *   Offset 20: BRESLINK   - Reservation queue head
 *   Offset 21: RSEGM      - Reserved segment info
 *   Offset 22-25: BITMAP  - Segment bitmap words
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 * Reference: SINTRAN/OS/02-QUEUE-STRUCTURES-DETAILED.md
 */

#include "../mon.h"
#include "../mon_log.h"

/*
 * SINTRAN WINDOW SEGMENT (Segment 31)
 *
 * ARCHITECTURAL NOTE:
 * In the real ND-500 system, RT descriptions are stored in ND-100 memory, not ND-500
 * memory. The ND-100 runs SINTRAN III and manages all RT scheduling. When the ND-100
 * suspends an ND-500 process, it saves the ND-500 registers into the RT description
 * (fields DPREG-DBREG). The ND-500 accesses this structure through shared memory or
 * a memory-mapped window to ND-100 space.
 *
 * We use Segment 31 (0x1F) as the "SINTRAN Window" - a region that maps to ND-100
 * system tables. This is architecturally correct because:
 * - Segment 31 is reserved for system use (not user program space)
 * - It provides a clean separation between user data and SINTRAN data
 * - It can later be replaced with actual ND-100 memory when integrating emulators
 *
 * IMPORTANT: The emulator must ensure segment 31 has valid MMU mappings:
 * - DC[31] must point to a valid physical segment in the PST
 * - Physical memory must be allocated for this segment
 * - The segment must be readable/writable by user code (for accessing RT descriptions)
 *
 * Reference: ND-05.009.4 EN (ND-500 Reference Manual) - Memory Management
 */
#define SINTRAN_SEGMENT         31
#define SINTRAN_BASE            0xF8000000  /* Segment 31 base address */
#define RT_DESCRIPTION_OFFSET   0x00001000  /* Offset within SINTRAN segment */
#define RT_DESCRIPTION_ADDR     (SINTRAN_BASE | RT_DESCRIPTION_OFFSET)  /* = 0xF8001000 */

/*
 * RT-Description structure size: 26 words.
 */
#define RT_DESCRIPTION_SIZE_WORDS 26

/*
 * Default priority for emulated processes.
 */
#define DEFAULT_PRIORITY 100

/*
 * Track if RT description has been initialized.
 */
static int rt_description_initialized = 0;

/*
 * Initialize the RT-Description structure at RT_DESCRIPTION_ADDR.
 * This creates a simulated RT description for the emulated process.
 *
 * STATUS word (offset 1) bit definitions per ND-60062-01D-EN page 289:
 *   Bit 0-7:  Priority (0-255, 255 = highest)
 *   Bit 8-9:  Ring
 *   Bit 10:   Not used
 *   Bit 11:   5ABS   - Absolute time scheduling
 *   Bit 12:   5INT   - Periodic program
 *   Bit 13:   5RWAIT - Voluntarily waiting (RTWT)
 *   Bit 14:   5REP   - Repeat after termination
 *   Bit 15:   5WAIT  - Waiting for I/O transfer
 *
 * For a running program: no flags set, just priority.
 *
 * Reference: SINTRAN III System Documentation (ND-60062-01D-EN), pages 289-290
 */
static void initialize_rt_description(MonContext* ctx) {
    uint32_t addr = RT_DESCRIPTION_ADDR;
    uint32_t current_pc = ctx->return_address;

    /* Default values for simulated RT process */
    uint32_t state_prio = DEFAULT_PRIORITY;  /* Priority in low byte, no flags */

    /* Offset 0: TLINK - Not in time queue */
    ctx->write_word(ctx->cpu, addr + 0 * 4, 0);

    /* Offset 1: STATE/PRIORITY */
    ctx->write_word(ctx->cpu, addr + 1 * 4, state_prio);

    /* Offset 2-3: DTIM (scheduled time) - Not scheduled */
    ctx->write_word(ctx->cpu, addr + 2 * 4, 0);
    ctx->write_word(ctx->cpu, addr + 3 * 4, 0);

    /* Offset 4-5: DTN (time interval) - No interval */
    ctx->write_word(ctx->cpu, addr + 4 * 4, 0);
    ctx->write_word(ctx->cpu, addr + 5 * 4, 0);

    /* Offset 6: STADR - Start address (use return address as proxy) */
    ctx->write_word(ctx->cpu, addr + 6 * 4, current_pc);

    /* Offset 7: SEGM - Segment numbers (C# uses currentDomain & 0xFF) */
    ctx->write_word(ctx->cpu, addr + 7 * 4, 0);  /* TODO: get current domain */

    /* Offset 8-15 (decimal) = 10-17 (octal): Saved registers (P, X, T, A, D, L, S, B)
     *
     * Per SINTRAN III System Documentation (ND-60062-01D-EN), page 290:
     * "All registers of the current RT-program are saved in these locations
     *  IF IT IS INTERRUPTED."
     *
     * Since our program is actively running (not interrupted), these fields
     * should be 0. The actual register values are in the CPU, not here.
     * These only get populated when the scheduler suspends the RT program.
     */
    ctx->write_word(ctx->cpu, addr + 8 * 4, 0);   /* DPREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 9 * 4, 0);   /* DXREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 10 * 4, 0);  /* DTREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 11 * 4, 0);  /* DAREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 12 * 4, 0);  /* DDREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 13 * 4, 0);  /* DLREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 14 * 4, 0);  /* DSREG - Only set when interrupted */
    ctx->write_word(ctx->cpu, addr + 15 * 4, 0);  /* DBREG - Only set when interrupted */

    /* Offset 16: WLINK - Not in waiting queue */
    ctx->write_word(ctx->cpu, addr + 16 * 4, 0);

    /* Offset 17-18: Active segments (C# uses currentDomain for ACTSEG1) */
    ctx->write_word(ctx->cpu, addr + 17 * 4, 0);  /* ACTSEG1 - TODO: get domain */
    ctx->write_word(ctx->cpu, addr + 18 * 4, 0);  /* ACTSEG2 */

    /* Offset 19: ACTPRI - Actual priority (same as STATE_PRIO) */
    ctx->write_word(ctx->cpu, addr + 19 * 4, DEFAULT_PRIORITY);

    /* Offset 20: BRESLINK - No reserved resources */
    ctx->write_word(ctx->cpu, addr + 20 * 4, 0);

    /* Offset 21: RSEGM - No reserved segment */
    ctx->write_word(ctx->cpu, addr + 21 * 4, 0);

    /* Offset 22-25: BITMAP words - No segments marked */
    ctx->write_word(ctx->cpu, addr + 22 * 4, 0);
    ctx->write_word(ctx->cpu, addr + 23 * 4, 0);
    ctx->write_word(ctx->cpu, addr + 24 * 4, 0);
    ctx->write_word(ctx->cpu, addr + 25 * 4, 0);

    mon_log(MON_LOG_DEBUG, MON_ID_30B ": Initialized RT-Description at 0x%08X", RT_DESCRIPTION_ADDR);
}

MonResult mon_30B_GetOwnRTAddress(MonContext* ctx) {
    uint32_t rt_addr = RT_DESCRIPTION_ADDR;

    /* Initialize RT description structure if not already done */
    if (!rt_description_initialized) {
        initialize_rt_description(ctx);
        rt_description_initialized = 1;
    }

    /* Return RT description address in W1 register */
    if (ctx->set_i1) {
        ctx->set_i1(ctx->cpu, rt_addr);
    }

    /* Also write to output parameter if provided (for HLL compatibility) */
    if (ctx->arg_count >= 1) {
        mon_write_param_word(ctx, 0, rt_addr);
    }

    mon_log(MON_LOG_DEBUG, MON_ID_30B ": OUT: RTDescrAddress=0x%08X", rt_addr);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
