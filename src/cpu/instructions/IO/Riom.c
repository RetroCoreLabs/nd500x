/*
 * Riom.c - ND-500 RIOM instruction (IO class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_settings.h"
#include <stdio.h>

/**
 * RIOM instruction - IO class
 *
 * Mnemonic: riom
 * Operands: 3
 * Opcode: 0xFE76 (hex) / 0177166 (octal) / 65142 (decimal)
 *
 * Operation: Read I/O Processor Memory (ND-100 -> ND-500 transfer)
 *
 * Description:
 * Privileged instruction that copies data from the I/O processor (ND-100) memory
 * to ND-500 memory through the ND-500 interface. This allows the ND-500 to access
 * private ND-100 memory that is not directly addressable by the ND-500.
 *
 * The ND-100 memory is accessed via DMA and does not interrupt ND-100 program
 * execution, allowing efficient data transfer between the two processors.
 *
 * Dual-Processor Architecture:
 * ND-500 systems use a companion I/O processor (ND-100) that handles peripheral
 * I/O operations while the ND-500 performs computation. RIOM enables inter-processor
 * communication by providing access to the ND-100's private memory space.
 *
 *   +----------+                    +----------+
 *   |  ND-500  | <----- RIOM -----  |  ND-100  |
 *   |   CPU    |  (Read from IOP)   |   IOP    |
 *   +----------+                    +----------+
 *      Main Processor              I/O Processor
 *
 * Address Spaces:
 * - ND-500 uses 32-bit byte addressing (4GB address space)
 * - RIOM addresses ND-100 memory by PHYSICAL word address, 24 bits
 *   (0x000000-0xFFFFFF). It is NOT limited to the ND-100's 64 KW logical
 *   window: manual ND-05.009.4 section 16.23 says the operand "specifies the
 *   physical ND-100 address and is usually private ND-100 memory".
 * - Hardware bridge translates between address spaces
 * - _private offset (typically 0x40000) maps ND-100 space into physical RAM
 *
 * HOW THE REAL HARDWARE DOES IT (decoded from the ND-5000 B30 microcode, 2026-07-20):
 *
 * RIOM is NOT a DMA engine with a descriptor and a word counter. It is a MICROCODED
 * COPY LOOP: the CPU's own microprogram issues ordinary physical memory reads, one
 * halfword per iteration, through its own memory port. Routine RIOM_0..RIOM_3:
 *
 *   012255 RIOM_0:  SC2 := BM01                    ; mask, bit 1
 *   012256          ALU,AND A,MIC,STS B,SC2        ; test MIC status bit 1
 *   012257          C,SEQ COND,MZRO -> ILLEG       ; not privileged -> IIC trap
 *   012261          ... G,OPS LADDR EA2SAVE ADACT  ; operand 2's ADDRESS -> EA2
 *   012262          ... D,LC   READ ADACT          ; operand 3 (count) -> LC
 *   012263          D,DAC,DPA := SC1               ; operand 1 VALUE -> DAC phys addr reg
 *   012266 RIOM_2:  LCDECR C,SEQ INVSEQ COND,LCZ   ; decrement, exit when zero
 *   012270 RIOM_3:  SC1 := DATA (TYP,HW)  RD,POF   ; read halfword from ND-100
 *   012272          <SC1>                 WRITE    ; write halfword to ND-500, loop
 *
 * The entire ND-100 access is the single field RD,POF. The ND-5000 Microprogram Guide
 * (ND-05.022.1:2467) defines it as "PERFORM A PHYSICAL READ WITH MMS", and the Hardware
 * Description (ND-05.020.01:5777) names the bus request RPOFF, "paging off read memory -
 * physical address translation". So the halfword is fetched by an ordinary memory read
 * with paging OFF, at a raw physical address. Note "with MMS": paging is off but the
 * memory management system still performs a physical translation, so POF is NOT the same
 * as the raw RD,PHYS/WR,PHYS pair.
 *
 * That is why the manual can say the transfer "does not interrupt the ND-100 program
 * execution" (section 16.23): it is DMA only from the ND-100's point of view. The ND-100
 * CPU is never involved because the ND-500 reaches shared/multiport memory through its
 * own port (on ND-5000, the MFbus channel interface MPCC on the mother board). The ACCP
 * is NOT on this path - its multiport commands WMPM/RMPM/TESTMPM are documented as
 * illegal while the microprogram is running.
 *
 * There is no descriptor, no controller command and no interface word-counter: the
 * counter is the microcode's own LC register, the pointers its own EA1/EA2.
 *
 * NOTE: there is no "WIOM" write counterpart - it appears in no manual, no manual index
 * entry and no opcode table (cpu.c also states this). The ND-500 writes back to the
 * ND-100 by (1) ordinary stores into SHARED memory, which IS directly addressable by the
 * ND-500 - hence the "usually PRIVATE ... not directly addressable" qualifier on RIOM's
 * source; (2) microcode writing the mailbox answer into the 5MPM block plus a level-12
 * interrupt; or (3) trapping outward via MON and letting the ND-100, the master I/O
 * processor, do the work. RIOM exists only for the case none of those covers: reading
 * memory the ND-500 cannot address at all.
 *
 * Operand Structure:
 * - Operand[0]: ND-100 source address (read, word)
 *   - Source address in I/O processor memory (ND-100 word address)
 *   - Usually private ND-100 memory, not directly ND-500 addressable
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Word (physical ND-100 address)
 *
 * - Operand[1]: ND-500 destination buffer (WRITE, halfword)
 *   - The operand EFFECTIVE ADDRESS is the ND-500 destination buffer address;
 *     the operand is NOT dereferenced. Manual ND-05.009.4 EN section 16.23
 *     declares operand 2 as <buffer/w/H>. Confirmed against the real ND-5000
 *     microcode (RIOM_0..RIOM_3), which saves operand 2 effective address as
 *     the destination pointer.
 *   - Destination buffer in ND-500 memory (logical address)
 *   - Addressing modes: LOCAL, RECORD, PRE_INDEXED, ABSOLUTE.
 *     REGISTER and CONSTANT are ILLEGAL for this operand.
 *   - Data type: Same as instruction prefix (halfword with H prefix)
 *
 *   WHY REGISTER AND CONSTANT ARE ILLEGAL HERE [settled 2026-07-20]:
 *     The microcode fetches this operand with a LADDR request (CS 012261,
 *     "LADDR EA2SAVE ADACT"). LADDR is value 1 of the microword MEMORY field -
 *     "perform a LADDR request" - the same field position that otherwise holds
 *     READ / WRITE / RD,POF. So operand 2 is resolved by a LOAD-ADDRESS request,
 *     not a data access. The manual's rule for that request is explicit, manual
 *     ND-05.009.4 section 15.4 (line 9435):
 *       "The address of the operand is loaded into the specified register.
 *        Registers and constants have no address in memory and are illegal as
 *        operands."
 *     A register has no address to load, so REGISTER cannot satisfy the request.
 *     This is also an independent second confirmation for CONSTANT, which the IOS
 *     definition already forbids as a destination.
 *     This was NOT decided from the manual's per-instruction addressing-mode table:
 *     that table exists but OCR has destroyed its column alignment. Do not cite it.
 *
 * - Operand[2]: Count (read, WORD)
 *   - Number of halfwords to transfer (0-65535)
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: WORD. CORRECTED 2026-07-29 - it is NOT the instruction prefix type.
 *     The manual annotates operands 1 and 2 explicitly (/W and /H) and annotates
 *     operand 3 not at all; "no of halfwords" is the value's MEANING, not its type.
 *     This matters because the data type also sets the post-index scale - see the
 *     detailed note at the operand read in the body.
 *
 * Operation Steps:
 * 1. Validate operand count (must be 3)
 * 2. Check supervisor mode (PIA bit must be set)
 * 3. Read ND-100 source address from operand[0] as a full 32-bit WORD
 *    (manual section 16.23: operand 1 data type is W, regardless of the H prefix)
 * 4. ND-500 destination address = EFFECTIVE ADDRESS of operand[1] (not its value)
 * 5. Read transfer count from operand[2] (halfword)
 * 6. Validate count (must fit in 16 bits)
 * 7. Validate ND-100 address (must fit in the 24-bit physical range 0xFFFFFF)
 * 8. For i = 0 to count-1:
 *    a. Calculate ND-100 address = nd100_addr + i (word address, 24-bit range)
 *       NOTE: the 24-bit ceiling is an EMULATOR CONVENTION agreed across the
 *       C and C# emulators, not a manual fact - ND-05.009.4 section 16.23 states no
 *       address width for the RIOM path. The real microcode masks nothing.
 *    b. Calculate ND-500 address = nd500_addr + ix2 (byte address)
 *    c. Read halfword from ND-100 memory[nd100_addr]
 *    d. Write halfword to ND-500 memory[nd500_addr]
 * 9. Do NOT touch any status flag (see Flag Behavior below)
 *
 * Memory Access Pattern:
 * - ND-100 source: Word address (multiply by 2 for byte offset)
 * - ND-500 dest: Byte address (increment by 2 for each halfword)
 * - The ND-100 side is a PHYSICAL access (microcode RD,POF), the ND-500 side a
 *   LOGICAL one (plain WRITE) - the asymmetry is intentional and matches the manual:
 *   "the <ND-100 addr> specifies the physical ND-100 address ... <buffer> is a
 *   logical ND-500 address". The bridge applies the offset and big-endian assembly.
 *
 * Flag Behavior:
 * - Manual section 16.23: "Data status bits: Unaffected".
 *   RIOM must not modify Z or any other status flag on ANY path, including the
 *   count == 0 path. (A previous implementation wrongly set Z when count == 0.)
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Illegal instruction code (IIC): Not in supervisor mode (requires PIA bit set).
 *   Microcode equivalent: CS 012256-012257 ANDs MIC,STS with bit 1, jumps to ILLEG.
 * - Illegal operand value (IOV, trap 16): "Operand values exceeding the legal range"
 *   (manual line 2132) - a VALUE check. Used here for the address-range guard.
 * - Illegal operand specifier (IOS, trap 34): the correct trap for a REGISTER or
 *   CONSTANT buffer operand. Its definition (manual line 2215) covers "constant
 *   operands as destination" and instructions that "do not allow register or constant
 *   operands". Do NOT use IOV for that case - IOV is about values, IOS about
 *   specifiers.
 *   IMPLEMENTED 2026-07-20: the executor now raises IOS for a REGISTER / CONSTANT /
 *   CONSTANT_SHORT buffer operand (guard at the top of the transfer body, mirrors
 *   RetroCore Riom.cs commit 84ca6098d). The manual lists IOS per-instruction inconsistently - section 15.4's
 *   own trap line omits it despite the prose forbidding registers - because IOS is
 *   raised by the operand decode generally, so its absence from section 16.23's trap
 *   list is not evidence that RIOM cannot raise it.
 *
 * Performance:
 * - Execution: Variable, depends on transfer size
 * - Overhead: ~10 cycles
 * - Transfer: ~2 cycles per halfword
 * - Total: ~(10 + 2xcount) cycles
 * - DMA access does not interrupt ND-100 execution
 *
 * Key Characteristics:
 * - Supervisor-only DMA transfer from I/O processor to ND-500 memory
 * - Accesses ND-100 private memory not directly addressable by ND-500
 * - Halfword (16-bit) transfers only
 * - IIC trap if not in supervisor mode
 * - IOV trap on invalid address or count
 * - DMA does not interrupt ND-100 execution
 * - Essential for inter-processor communication
 *
 * Common Use Cases:
 * - Accessing I/O processor private memory
 * - Inter-processor data transfer
 * - Reading I/O processor status/configuration
 * - Debugging I/O processor state
 * - Shared memory communication between processors
 * - Reading device controller buffers via ND-100
 *
 * Typical Usage:
 *   Example 1: Copy one page from ND-100 memory
 *     H RIOM 66000B:W, PG, 1024        ; Copy 1024 halfwords
 *
 *   Example 2: Read I/O processor status
 *     H RIOM IOP_STATUS_ADDR:W, STATUS_BUFFER, 16
 *
 *   Example 3: Variable-sized transfer
 *     H RIOM SOURCE_ADDR, DEST_BUFFER, B.TRANSFER_SIZE
 *
 *   Example 4: Read configuration from ND-100
 *     H RIOM IOP_CONFIG:W, LOCAL_BUF, CONFIG_SIZE
 *
 * Notes:
 * - RIOM must be preceded by H prefix (halfword operations)
 * - Transfers are always in halfword (16-bit) units
 * - ND-100 physical word addresses are 24-bit (0x000000-0xFFFFFF), not 16-bit
 * - ND-500 address space is full 32-bit
 * - Requires supervisor mode (PIA bit set in status register)
 * - Use for inter-processor communication in dual-processor ND-500 systems
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL (Basic bridge implemented)
 *
 * Current implementation:
 * - ND-100 bridge with address translation (nd100_memory_offset = 0x40000)
 * - Reads from shared physical memory at offset 0x40000
 * - Full transfer loop with proper address calculation
 * - Flag updates and validation
 *
 * Future enhancements:
 * 1. Cycle-accurate timing for the transfer loop (there is no DMA controller to model -
 *    the microcode does the copy itself, one halfword per iteration)
 * 2. (DONE 2026-07-20) IOS for a REGISTER/CONSTANT buffer operand - see Trap Conditions
 *
 * Related Instructions:
 * - RIOM: Read I/O processor memory (ND-100 -> ND-500) [this instruction]
 *
 * Reference: ND-500 Reference Manual, Section 16.23
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/IO/Riom.cs
 */
/* Whether to trace RIOM transfers. Off unless ND500X_RIOMDBG is set, and never on
 * when this CPU is embedded in another machine whose stdout is a guest console. */
static int riom_trace(void)
{
    static int on = -1;
    if (on < 0) { on = nd500_settings()->riomdbg; }
    return on && !nd500_embedded;
}

void nd500_instr_Riom(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] RIOM expects 3 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* RIOM is a PRIVILEGED instruction (manual section 16.23: "Privileged
     * instruction", trap condition "Illegal instruction code (IIC)"). The real
     * ND-5000 microcode ANDs the MIC status register with bit 1 and branches to
     * ILLEG when the bit is clear. Use the established repo-wide helper, the
     * same pattern as RWIP and the other privileged SYSTEM instructions. */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* IIC trap already raised; abort the instruction */
    }

    /* Read operands using nd500_read_operand_value for proper addressing mode support */
    /* Operand 0: ND-100 source address (word address in I/O processor space) */
    /* Manual section 16.23 declares operand 1 as <ND-100 addr/r/W>: a full 32-bit
     * WORD, independent of the H instruction prefix. Reading it with fi->data_type
     * (H) truncated real SINTRAN pointers above 0xFFFF. */
    uint32_t nd100_source_addr = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Operand 1 (second operand): ND-500 destination buffer. This is a WRITE
     * operand whose EFFECTIVE ADDRESS is the buffer (manual section 16.23), so it
     * must not be dereferenced. Same convention used by the STRING instructions. */
    /* Operand 1 buffer: REGISTER and CONSTANT (both forms) are ILLEGAL here. The
     * microcode fetches this operand with a LADDR (load-address) request at CS 012261,
     * and manual ND-05.009.4 section 15.4 states the rule for that request verbatim:
     * "Registers and constants have no address in memory and are illegal as operands."
     * A register/constant has no effective address to write into, so raise IOS (trap 34,
     * the illegal-operand-SPECIFIER trap via trap_illegal_operand), NOT IOV (a value
     * check). Mirrors RetroCore Riom.cs (commit 84ca6098d). Closes the [OPEN] note above. */
    Nd500AddrMode buf_mode = fi->operands[1].mode;
    if (buf_mode == ND500_ADDR_REGISTER ||
        buf_mode == ND500_ADDR_CONSTANT ||
        buf_mode == ND500_ADDR_CONSTANT_SHORT) {
        printf("[ERROR] RIOM buffer operand has illegal addressing mode %d "
               "(a register or constant has no address in memory; ND-05.009.4 section 15.4) "
               "at PC=0x%08X\n", (int)buf_mode, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t nd500_dest_addr = fi->operands[1].effective_address;

    /* Operand 2: Count of halfwords to transfer - a WORD, not a halfword.
     * CORRECTED 2026-07-29, mirrored from RetroCore Riom.cs / Instructionset.Init.cs.
     *
     * The manual's format line types operands 1 and 2 EXPLICITLY and gives operand 3
     * no type at all:
     *     H RIOM <ND-100 addr/r/W>,<buffer/w/H>,<no of halfwords>
     * (ND-05.009.4 section 16.23). "no of halfwords" names what the value MEANS, not its
     * data type. It is a plain count, and the natural ND-500 integer is a word - exactly
     * like the address in operand 1, which is already read as ND500_DTYPE_WORD above.
     *
     * The dtype passed here drives TWO things: the width of the value read AND the
     * POST-INDEX SCALE. Both were wrong with fi->data_type (H).
     *
     * Measured on the live ND-500 swapper (RetroCore octobus harness), which takes its
     * count from a post-indexed table that is an array of 32-BIT WORDS at VA 0x0802403C:
     *     [0]=0x0D [1]=0x0A [2]=0x0F [3]=0x8A [4]=0x09 [5]=0x46 [6]=0x08 ...
     *   H (scale 2), index 5 -> address 0x24046, MISALIGNED across two entries, and it
     *                           returned the low halfword of [2] = 15. Plausible-looking
     *                           and pure coincidence; it stopped the transfer 106 bytes
     *                           short and left the swapper's base pointer zero, which is
     *                           what produced SWPFATAL 0o201.
     *   W (scale 4), index 5 -> address 0x24050, [5] = 0x46 = 70 halfwords = 140 bytes,
     *                           which covers the field the swapper actually reads.
     * Reading as H was also wrong on its own: a 2-byte big-endian read of 0x00000046
     * yields 0x0000, i.e. it transfers nothing. */
    /* THE SCALE AS WELL AS THE WIDTH. Reading the value as a WORD fixes how many
     * bytes come back; it cannot fix WHERE they come from, because the decode
     * already computed this operand's effective address using the INSTRUCTION's
     * data type - H - and scaled the post-index by 2.
     *
     * Measured 30-SEP-2026 on the live ND-5000 swapper: index 5 against the count
     * table at 0x0802403C resolved to 0x08024046 instead of 0x08024050, straddling
     * entries [2] and [3], and returned 0x000F0000 - 983040 halfwords, which the
     * range check below rightly refused. The comment above already recorded that
     * 0x24046 is the WRONG address and 0x24050 the right one; only the read width
     * had been corrected. */
    Nd500OperandDecoded count_op = fi->operands[2];
    count_op.effective_address =
        nd500_operand_ea_at_dtype(cpu, &fi->operands[2], ND500_DTYPE_WORD);
    uint32_t count = (uint32_t)nd500_read_operand_value(cpu, &count_op, ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Validate count - must fit in 16 bits (halfword range 0-65535) */
    if (count > 0xFFFF) {
        /* SAY WHICH CELL THE COUNT CAME FROM. The swapper reads it from a
         * post-indexed table of 32-bit words, so a count that is out of range is
         * almost always a read at the wrong address rather than a genuinely silly
         * number - and the address distinguishes a bad index from a bad scale from a
         * bad base. Without it, 983040 is just a number. */
        printf("[ERROR] RIOM invalid count %u (0x%08X) read from EA=0x%08X "
               "(mode=%d reg=%u) at PC=0x%08X\n",
               count, count, count_op.effective_address,
               (int)count_op.mode, (unsigned)count_op.reg, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate the ND-100 source against the PHYSICAL word-address space.
     * The 24-bit ceiling is an EMULATOR CONVENTION, matched deliberately with the C#
     * RetroCore emulator so the two agree. It is NOT a hardware fact: manual section
     * 16.23 states no address width for the RIOM path, and the real microcode masks
     * nothing at all. If a real width is ever established, change it here, in
     * src/cpu/cpu.c, AND in RetroCore Riom.cs together. */
    if (nd100_source_addr > 0xFFFFFF) {
        printf("[ERROR] RIOM invalid ND-100 address 0x%08X at PC=0x%08X (max 0xFFFFFF)\n",
               nd100_source_addr, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for potential address wraparound in ND-100 space.
     * ND-100 physical word-address space here is 24-bit (0x000000-0xFFFFFF), so the
     * boundary is 0x1000000. (This constant previously read 0x400000, the old 22-bit
     * boundary, and was missed when the ceiling was widened - it warned on every
     * transfer above 4 MW.) Purely diagnostic: the real microcode neither checks nor
     * masks, it just walks EA1 with LC. */
    if ((uint64_t)nd100_source_addr + count > 0x1000000ULL) {
        printf("[WARNING] RIOM transfer wraps ND-100 address space at PC=0x%08X\n",
               fi->address);
        /* This is allowed but logged for debugging */
    }

    /* NOT ON THE GUEST'S CONSOLE. In the embedded lane (an nd100x running SINTRAN
     * with an ND-5000 on the octobus) stdout is the ND-100 GUEST'S console, so a
     * per-transfer trace here lands in the middle of SINTRAN's own output. Reported by
     * Ronny 30-SEP-2026: the trace read as "lot of errors" on the console. It is NOT
     * the cause of the slow swapper load - Ronny confirmed that slowness predates this
     * trace and is a separate, still-open question. The transfer detail is a
     * DIAGNOSTIC and now needs
     * ND500X_RIOMDBG, which also keeps the free-running binary quiet by default. */
    if (riom_trace())
    {
        printf("[RIOM] Transfer: ND-100[0x%06X] -> ND-500[0x%08X], count=%u halfwords at PC=0x%08X\n",
               nd100_source_addr, nd500_dest_addr, count, fi->address);
    }

    /* ========================================================================
     * DMA TRANSFER FROM ND-100 TO ND-500
     * ========================================================================
     *
     * Transfer loop: Copy count halfwords from ND-100 memory to ND-500 memory
     *
     * Address spaces:
     * - ND-100 source: Word address (multiply by 2 for byte offset)
     * - ND-500 dest: Byte address (increment by 2 for each halfword)
     *
     * Mirrors microcode RIOM_2/RIOM_3 (CS 012266-012272): read halfword via RD,POF,
     * write halfword via plain WRITE, LCDECR / COND,LCZ to terminate. There is no DMA
     * controller involved on the ND-500 side; the ND-100 is undisturbed because the
     * memory is reached through a second port, not because its bus cycles are stolen.
     *
     * Emulator implementation (this is a REAL transfer, not a stub - an older comment
     * here claimed it "writes zeros to destination", which has not been true since the
     * bridge landed):
     * - nd500_read_nd100_word() applies nd100_memory_offset and big-endian assembly
     * - nd500_write_memory_16() goes through the normal ND-500 MMU path
     * ======================================================================== */

    /* HOW FAR THE SOURCE ADVANCES PER HALFWORD IS TRANSPORT-SPECIFIC, because the units
     * of the ND-100-side operand are. Stepping by 1 unconditionally is only right under
     * the 3022 word-address convention; on the octobus the operand is a BYTE offset in
     * the 5MPM window, so one halfword is 2 units. Derived from the same mapping
     * nd500_read_nd100_word uses, so the two cannot disagree. See the field comment on
     * nd100_memory_offset in Nd500Cpu. */
    const uint32_t source_step = nd500_cpu_nd100_step_per_halfword(cpu);

    for (uint32_t i = 0; i < count; i++) {
        /* Calculate addresses for this transfer iteration */
        /* ND-100 operand: stepped in ITS OWN units, wrapped at the 24-bit boundary */
        uint32_t nd100_addr = (nd100_source_addr + (i * source_step)) & 0xFFFFFF;

        /* ND-500 address: byte-based, each halfword is 2 bytes */
        uint32_t nd500_addr = nd500_dest_addr + (i * 2);

        /* Read halfword from ND-100 memory via bridge
         * The bridge handles:
         *   - Address translation (nd100_memory_offset + nd100_addr x 2)
         *   - Physical memory access to emulated ND-100 space
         *   - Big-endian byte order (both ND-100 and ND-500 use big-endian)
         */
        uint16_t data = nd500_read_nd100_word(cpu, nd100_addr);

        /* Write halfword to ND-500 memory */
        nd500_write_memory_16(cpu, nd500_addr, data);

        /* Debug trace for first and last transfers (avoid log spam for large transfers) */
        /* NAME THE FIRST TEN HALFWORDS. One halfword cannot say whether a transfer
         * found the right record or merely a plausible first word; ten can be compared
         * against the known-good dump of the swapper's message, measured on the C#
         * octobus harness as
         *     FFFF FFFF 0427 0001 0000 0000 0005 0005 003B 0840
         * (0x427 = 2047B, 0x840 = ADRZERO page 2112). Bounded, so a page-sized transfer
         * cannot flood the log. */
        if (riom_trace() && (i < 10 || i == count - 1 || count <= 4)) {
            printf("  RIOM[%u]: ND-100[0x%06X] = 0x%04X -> ND-500[0x%08X]\n",
                   i, nd100_addr, data, nd500_addr);
        }
    }

    /* Status flags: manual section 16.23 says "Data status bits: Unaffected".
     * Deliberately NO flag update here, not even on the count == 0 path. */

    if (riom_trace())
    {
        printf("[RIOM] Completed: %u halfwords transferred (status bits unaffected) at PC=0x%08X\n",
               count, fi->address);
    }

    /* NOTE on "DMA": the manual calls this a DMA access that "does not interrupt the
     * ND-100 program execution", and that is true - but only from the ND-100's point of
     * view. On the ND-500 side there is no DMA controller: the microprogram itself
     * issues the reads one halfword at a time through its own memory port (on ND-5000,
     * the MFbus channel interface MPCC).
     * - ND-500 blocks until the transfer completes (~10 + 2*count cycles)
     * - ND-100 continues executing (no interruption)
     *
     * In emulator:
     * - Transfer is immediate (no cycle counting)
     * - Both processors are in same address space
     * - Bridge would provide address translation
     */
}
