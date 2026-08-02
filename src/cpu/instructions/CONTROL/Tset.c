#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Tset instruction - CONTROL class
 *
 * TSET - Test and Set (Atomic test-and-set operation for synchronization)
 *
 * Mnemonic: TSET
 * Format: TSET <operand/rw/W>
 * Variants: 1
 * Operands: 1 (<operand/rw/W>)
 *
 * Opcode:
 *   0xFD40 (TSET) - Test and set word operand atomically
 *
 * Operation:
 *   old_value = <operand>
 *   <operand> = 0xFFFFFFFF (all bits set)
 *   if old_value == 0 then
 *       Z = 1 (resource was available)
 *   else
 *       Z = 0 (resource was already locked)
 *   endif
 *
 * Description:
 *   Implements an atomic test-and-set operation for implementing mutual
 *   exclusion primitives such as spinlocks, semaphores, and mutexes. This
 *   instruction is fundamental for synchronization in multi-processor or
 *   multi-domain systems.
 *
 *   The instruction reads the current value of the operand, then writes
 *   0xFFFFFFFF (all bits set) to it, guaranteeing atomicity even in
 *   multi-processor environments. The Zero flag is set based on the
 *   original value before modification.
 *
 *   If Z=1 after TSET, the resource was available (value was 0) and has
 *   now been acquired. If Z=0, the resource was already locked (non-zero)
 *   and the caller must wait or retry.
 *
 * Atomicity Guarantee:
 *   On the ND-500, TSET is implemented as a bus-locked read-modify-write
 *   cycle, ensuring no other processor or DMA controller can access the
 *   memory location between the read and write operations. This makes it
 *   suitable for implementing lock-free data structures and critical
 *   sections.
 *
 * Typical Lock Acquisition:
 *   ACQUIRE_LOCK:
 *       TSET LOCK_VARIABLE    ; Atomically test and set lock
 *       IF=GO:B GOT_LOCK      ; Z=1 means we got it (was 0)
 *       ; Lock busy, spin or yield
 *       GO:B ACQUIRE_LOCK     ; Try again
 *   GOT_LOCK:
 *       ; Critical section
 *       ...
 *       W = 0                 ; Clear lock
 *       LOCK_VARIABLE = W     ; Release lock
 *
 * Flags: Z (zero)
 *   Z = 1 if original operand value was 0 (lock acquired successfully)
 *   Z = 0 if original operand value was non-zero (lock already held)
 *   All other flags (S, C, V, K) remain unchanged
 *
 * Trap conditions:
 *   - Addressing traps for operand access
 *   - Protection violations if operand is read-only
 *   - Page fault if operand page not present
 *
 * Performance:
 *   - Uncontended: 4-5 cycles
 *   - Contended: 5-8 cycles (includes bus lock overhead)
 *   - Multi-processor: Additional cycles for cache coherency protocol
 *
 * Key Characteristics:
 *   - Atomic read-modify-write operation
 *   - Bus-locked for multi-processor safety
 *   - Single word operand (32-bit)
 *   - Sets all bits (0xFFFFFFFF) regardless of original value
 *   - Z flag indicates success (Z=1) or failure (Z=0)
 *   - Essential for lock-free programming
 *   - No memory barriers needed (atomic implies ordering)
 *
 * Common Use Cases:
 *   - Spinlock implementation
 *   - Mutex/semaphore primitives
 *   - Lock-free data structure synchronization
 *   - Critical section protection
 *   - Resource allocation flags
 *   - Inter-processor communication flags
 *   - Hardware device arbitration
 *
 * Example Usage:
 *   ; Simple spinlock
 *   ACQUIRE:
 *       TSET SPINLOCK         ; Try to acquire
 *       IF<>GO:B ACQUIRE      ; Z=0, already locked, retry
 *   ; Lock acquired
 *   CRITICAL_SECTION
 *   W = 0
 *   SPINLOCK = W              ; Release lock
 *
 *   ; Spinlock with backoff
 *   TRIES = 1000
 *   TRY_LOCK:
 *       TSET LOCK
 *       IF=GO:B LOCKED        ; Got it
 *       W LOOPD:B TRIES, 1, TRY_LOCK  ; Retry with limit
 *   ; Failed to acquire
 *   GO:B LOCK_FAILED
 *   LOCKED:
 *   ; Success
 *
 *   ; Semaphore down operation (simplified)
 *   SEM_WAIT:
 *       TSET SEM_FLAG
 *       IF<>GO:B SEM_WAIT     ; Spin until available
 *   ; Semaphore acquired
 *   I1 = SEM_COUNT
 *   I1 = I1 - 1
 *   SEM_COUNT = I1
 *   W = 0
 *   SEM_FLAG = W              ; Release semaphore lock
 *
 *   ; Try-lock (non-blocking)
 *   TSET MUTEX
 *   IF<>GO:B BUSY             ; Couldn't get lock
 *   ; Lock acquired
 *   DO_WORK
 *   W = 0
 *   MUTEX = W
 *   GO:B DONE
 *   BUSY:
 *   ; Lock was busy, do something else
 *   DONE:
 *
 * Related Instructions:
 *   - SOLO: Disable process switch (different atomicity mechanism)
 *   - COMP: Compare instruction (for lock status checks)
 *   - W = 0: Clear word (for lock release)
 *
 * Comparison with Related Instructions:
 *   - TSET vs SOLO: TSET is atomic memory operation, SOLO prevents preemption
 *   - TSET vs COMP: TSET modifies memory atomically, COMP only reads
 *   - TSET vs simple write: TSET guarantees atomicity, write does not
 *
 * Lock Release Pattern:
 *   Locks acquired with TSET are typically released by writing 0:
 *   W = 0
 *   LOCK_VARIABLE = W
 *
 *   On some systems, a memory barrier or fence instruction might be
 *   needed before the write to ensure all critical section stores are
 *   visible. On ND-500, TSET implies necessary barriers.
 *
 * Multi-Processor Considerations:
 *   - TSET locks the system bus during execution
 *   - Cache lines are invalidated on other processors
 *   - Cache coherency protocol ensures consistency
 *   - May cause performance impact under high contention
 *   - Consider backoff strategies for heavily contended locks
 *
 * Spinlock vs Mutex:
 *   - TSET is ideal for short critical sections (spinlock)
 *   - For longer waits, consider yielding CPU (mutex with sleep)
 *   - Spinlocks waste CPU cycles while waiting
 *   - Mutexes require OS support but are more efficient for long waits
 *
 * Implementation Notes:
 *   In a full implementation, this instruction would:
 *   1. Acquire bus lock (prevent other bus masters)
 *   2. Read current operand value
 *   3. Set Z flag based on read value (Z=1 if value was 0)
 *   4. Write 0xFFFFFFFF to operand
 *   5. Release bus lock
 *   6. All steps are atomic from other processors' perspective
 *
 *   For single-processor emulation without true multi-threading,
 *   atomicity is inherent. For multi-threaded emulator, would need
 *   mutex or atomic operations to simulate bus lock.
 */
void nd500_instr_Tset(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 1) {
        printf("[ERROR] TSET at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // TSET requires a MEMORY operand. Register and constant operands are illegal and raise an
    // Illegal-Operand-Specifier (IOS) trap - ND-500 Reference Manual s16.3: "Register and constant
    // operands are illegal, and will cause an illegal operand specifier trap condition." The atomic
    // read-modify-write only has meaning against a main-memory cell (the bus lock protects memory,
    // not a CPU register). [TSET IOS 2026-07-25]
    if (fi->operands[0].mode == ND500_ADDR_REGISTER ||
        fi->operands[0].mode == ND500_ADDR_CONSTANT ||
        fi->operands[0].mode == ND500_ADDR_CONSTANT_SHORT) {
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read current value (atomically in hardware)
    // TSET uses the data type from the instruction (typically BYTE with BY prefix)
    uint64_t old_value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }


    // Write all-ones to operand based on data type
    // For BYTE (BY prefix): write 0xFF
    // For WORD: write 0xFFFFFFFF
    uint64_t set_value;
    uint64_t mask;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            set_value = 0xFF;
            mask = 0xFF;
            break;
        case ND500_DTYPE_HALFWORD:
            set_value = 0xFFFF;
            mask = 0xFFFF;
            break;
        case ND500_DTYPE_WORD:
        default:
            set_value = 0xFFFFFFFF;
            mask = 0xFFFFFFFF;
            break;
    }
    nd500_write_operand_value(cpu, &fi->operands[0], set_value, fi->data_type);

    // Set Z flag based on old value
    // Z=1 if old value was 0 (lock acquired successfully)
    // Z=0 if old value was non-zero (lock already held)
    if ((old_value & mask) == 0) {
        cpu->ST1 |= ND500_FLAG_Z;   // Set Z flag
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;  // Clear Z flag
    }

    // Set S flag from the OLD value sign bit (before the store). ND-500 Ref s16.3 data status:
    // "operand was negative before store -> S". The sign bit is the top bit of the operand WIDTH
    // (BY: bit7, H: bit15, W: bit31). This matches the functional CpuND500 Tset.cs. [TSET S-flag 2026-07-25]
    uint64_t sign_mask;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:     sign_mask = 0x80;       break;
        case ND500_DTYPE_HALFWORD: sign_mask = 0x8000;     break;
        case ND500_DTYPE_WORD:
        default:                   sign_mask = 0x80000000; break;
    }
    if ((old_value & sign_mask) != 0) {
        cpu->ST1 |= ND500_FLAG_S;   // Set S flag (old value negative)
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;  // Clear S flag
    }

    // Note: Other flags (C, V, K) are not modified by TSET

    if (getenv("ND500X_TSETDBG") && fi->address >= 0x83Au && fi->address <= 0x852u) {
        static uint64_t n = 0;
        if ((n++ % 200000) == 0) {
            uint64_t readback = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
            fprintf(stderr, "[TSETDBG] tset@0x%08X dtype=%d old=0x%llX -> wrote=0x%llX readback=0x%llX "
                    "Z=%d ST1=0x%08X R=0x%08X opmode=%d (n=%llu)\n",
                    fi->address, (int)fi->data_type,
                    (unsigned long long)old_value, (unsigned long long)set_value,
                    (unsigned long long)readback,
                    (cpu->ST1 & ND500_FLAG_Z) ? 1 : 0, cpu->ST1, cpu->R,
                    (int)fi->operands[0].mode, (unsigned long long)n);
        }
    }
}
