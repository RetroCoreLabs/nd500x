#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Solo instruction - CONTROL class
 *
 * SOLO - Disable Process Switch (Prevent context switching for atomic sequences)
 *
 * Mnemonic: SOLO
 * Format: SOLO
 * Variants: 1
 * Operands: 0
 *
 * Opcode:
 *   0xFE00 (SOLO) - Disable process switching
 *
 * Operation:
 *   process_switch_disabled = true
 *   ; Subsequent instructions execute atomically without preemption
 *   ; SOLO mode ends on next conditional jump taken or not taken
 *
 * Description:
 *   Disables process switching (context switching) for a sequence of
 *   instructions, ensuring they execute atomically without interruption
 *   from the scheduler or other processes. This is essential for
 *   implementing critical sections that must complete without preemption.
 *
 *   Unlike TSET which provides atomic memory operations, SOLO provides
 *   atomic instruction sequences by preventing the operating system or
 *   domain controller from switching to another process until the SOLO
 *   mode ends.
 *
 *   SOLO mode automatically terminates when the next conditional branch
 *   instruction is encountered (regardless of whether the branch is taken
 *   or not taken). This ensures bounded atomicity and prevents indefinite
 *   monopolization of the CPU.
 *
 * Duration of SOLO Mode:
 *   SOLO mode remains active until one of these events occurs:
 *   1. A conditional branch instruction (IF conditions, LOOP variants, etc.)
 *   2. An explicit trap or interrupt (hardware exception)
 *   3. A SOLO instruction is executed again (re-enters SOLO mode)
 *
 *   Unconditional jumps (GO, JUMP) do NOT terminate SOLO mode.
 *
 * Typical Instruction Sequence:
 *   SOLO                  ; Enter atomic mode
 *   ; Critical section instructions (no branches allowed)
 *   ADD I1, I2
 *   STORE RESULT
 *   COMP I1, LIMIT
 *   IF>GO:B DONE          ; First conditional branch, ends SOLO mode
 *   ; SOLO mode has ended here
 *
 * Flags: None modified
 *   All status flags remain unchanged by this instruction
 *
 * Trap conditions:
 *   - None under normal operation
 *   - Hardware interrupts/traps can override SOLO (system-dependent)
 *
 * Performance:
 *   - Execution: 1 cycle
 *   - Overhead: Minimal (sets internal CPU state bit)
 *   - No memory access required
 *
 * Key Characteristics:
 *   - Zero operands (standalone instruction)
 *   - Prevents process/domain switching
 *   - Bounded duration (ends at next conditional branch)
 *   - Does not prevent interrupts (hardware-dependent)
 *   - No flags modified
 *   - Minimal overhead (single cycle)
 *   - Complementary to TSET for different atomicity needs
 *   - Essential for OS kernel critical sections
 *
 * Common Use Cases:
 *   - Critical section protection in kernel code
 *   - Atomic read-modify-write sequences (multiple steps)
 *   - Process control block manipulation
 *   - Scheduler data structure updates
 *   - Interrupt handler entry/exit sequences
 *   - Domain switching preparation
 *   - Hardware register sequences that must be atomic
 *
 * Example Usage:
 *   ; Atomic increment of shared counter
 *   SOLO                  ; Prevent preemption
 *   I1 = COUNTER          ; Read
 *   I1 = I1 + 1           ; Modify
 *   COUNTER = I1          ; Write
 *   COMP I1, I1           ; Dummy comparison
 *   IF=GO:B CONTINUE      ; End SOLO mode (always taken)
 *   CONTINUE:
 *
 *   ; Atomic list insertion
 *   SOLO
 *   I1 = LIST_HEAD        ; Read current head
 *   NEW_NODE.NEXT = I1    ; Link new node to old head
 *   LIST_HEAD = NEW_NODE  ; Update head to new node
 *   COMP I1, I1
 *   IF=GO:B DONE          ; Terminate SOLO
 *   DONE:
 *
 *   ; Process state transition
 *   SOLO
 *   I1 = PROCESS_STATE
 *   COMP I1, STATE_READY
 *   IF<>GO:B NOT_READY    ; SOLO ends here (branch may/may not be taken)
 *   ; Process was ready, continue
 *   STATE = STATE_RUNNING
 *   NOT_READY:
 *   ; SOLO mode has ended
 *
 *   ; Atomic double-word update
 *   SOLO
 *   I1 = VALUE_HIGH
 *   I2 = VALUE_LOW
 *   TARGET_HIGH = I1
 *   TARGET_LOW = I2
 *   NOOP                  ; Force instruction
 *   IF=GO:B END           ; Always taken, ends SOLO
 *   END:
 *
 *   ; Scheduler context save
 *   SOLO                  ; Prevent preemption during context save
 *   SAVE_REGISTERS
 *   UPDATE_PCB
 *   MARK_PROCESS_BLOCKED
 *   COMP I1, I1
 *   IF=GO:B SAVED         ; End SOLO
 *   SAVED:
 *
 * Related Instructions:
 *   - TSET: Atomic test-and-set (memory operation)
 *   - IF conditions: Conditional branches (terminate SOLO)
 *   - NOOP: No operation (does not terminate SOLO)
 *   - GO: Unconditional branch (does not terminate SOLO)
 *
 * Comparison with Related Instructions:
 *   - SOLO vs TSET: Prevents preemption vs atomic memory operation
 *   - SOLO vs interrupt disable: Different mechanisms, may coexist
 *   - SOLO vs critical section locks: Hardware vs software approach
 *
 * SOLO vs TSET Use Cases:
 *   Use TSET when:
 *   - Single memory location needs atomic update
 *   - Implementing locks/semaphores/mutexes
 *   - Multi-processor synchronization required
 *
 *   Use SOLO when:
 *   - Multiple instructions must execute atomically
 *   - Preventing process switching (not just memory conflicts)
 *   - Single-processor atomic sequence guarantee
 *   - Operating system kernel critical sections
 *
 * Termination Pattern:
 *   Always terminate SOLO with a conditional branch:
 *   SOLO
 *   ; Critical instructions
 *   COMP R1, R1           ; Force condition
 *   IF=GO:B END           ; Always taken, terminates SOLO
 *   END:
 *
 *   Or use natural conditional logic:
 *   SOLO
 *   ; Critical instructions
 *   COMP VALUE, LIMIT
 *   IF<GO:B LESS          ; Natural branch, terminates SOLO
 *   ; Greater or equal path
 *   GO:B CONTINUE
 *   LESS:
 *   ; Less than path
 *   CONTINUE:
 *
 * Interrupt Handling:
 *   The behavior of SOLO with respect to hardware interrupts is
 *   system-dependent. Some implementations may:
 *   - Allow interrupts but prevent process switching
 *   - Defer interrupts until SOLO ends
 *   - Allow critical interrupts, disable others
 *
 *   Consult system documentation for specific interrupt behavior.
 *
 * Nested SOLO:
 *   Executing SOLO while already in SOLO mode typically:
 *   - Re-enters SOLO mode (resets the termination condition)
 *   - Extends atomic sequence until next conditional branch
 *   - Not recommended (adds complexity, no benefit)
 *
 * Performance Considerations:
 *   - Keep SOLO sequences short (few instructions)
 *   - Long SOLO sequences reduce system responsiveness
 *   - May increase interrupt latency
 *   - Consider TSET for simpler atomic operations
 *   - Scheduler cannot preempt until conditional branch
 *
 * Multi-Processor Considerations:
 *   SOLO only prevents process switching on the current CPU. For
 *   multi-processor synchronization, combine with TSET or other
 *   memory-based locks to ensure atomicity across processors.
 *
 * Implementation Notes:
 *   In a full implementation, this instruction would:
 *   1. Set an internal CPU flag (SOLO mode active)
 *   2. Disable scheduler preemption for this processor
 *   3. Continue executing instructions normally
 *   4. Monitor for conditional branch instructions
 *   5. Clear SOLO flag when conditional branch encountered
 *   6. Re-enable scheduler preemption
 *
 *   In an emulator without true multi-processing or scheduling,
 *   SOLO may have no practical effect. However, it should still
 *   track the state for accurate emulation and debugging.
 */
void nd500_instr_Solo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 0) {
        printf("[ERROR] SOLO at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Set PSD (ST1 bit 4). The manual is explicit that this bit "is only
     * modifiable by the SOLO and TUTTI instructions" (ch.6.5.4, Table:
     * status/trap bits), so this is the one place that may set it - TUTTI
     * clears it.
     *
     * This used to be a TODO that only logged, which left PSD permanently
     * clear. Two consequences followed from that single omission: nothing
     * could ever detect a SOLO region running too long (DT, bit 30), and
     * nothing could notice a page fault taken inside one (DE, bit 31) - so
     * both traps looked "unimplemented" when the real cause was that the
     * condition they detect could never arise. NDIX enables both (machine/
     * trap.h T_CMTE1/T_KOTE1 have bits 30 and 31 set) and vectors them
     * (machine/locore.c:689-690), so it was asking for a signal the emulator
     * could not give. */
    cpu->ST1 |= ND500_FLAG_PSD;

    /* Stamp the start so check_solo_timeout() can measure the region. The
     * limit counts MACROINSTRUCTION cycles on the ND-5000 (ch.16.1), which is
     * what instruction_count holds. */
    cpu->solo_start_icount = cpu->instruction_count;

    {
        static int dbg = -1;
        if (dbg < 0) { const char* e = getenv("ND500X_SOLODBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (dbg)
            printf("[SOLO] Process switch disabled at PC=0x%08X (PSD set, icount=%llu)\n",
                   fi->address, (unsigned long long)cpu->instruction_count);
    }

    /* PSD is a status bit, not a condition flag: no arithmetic status is
     * affected by SOLO. */
}
