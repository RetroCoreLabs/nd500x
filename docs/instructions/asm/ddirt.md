# DDIRT - Dump Dirty Cache

## Overview

**Mnemonic:** `ddirt`
**Function:** Write dirty cache data to memory
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `DDIRT`

---

## Description

Writes all data marked as 'dirty' in the data cache back to main memory. This privileged instruction ensures memory consistency by flushing modified cache lines. Data becomes dirty when it is written to the cache but not yet written back to memory. If no data cache is present in the system, the instruction executes as a no-operation.

**Operation:**
```
For each cache line:
    if dirty_bit set:
        write cache_line → memory
        clear dirty_bit
```

**Key Characteristics:**
- Supervisor-only cache management instruction
- Flushes all dirty cache lines to memory
- Ensures memory consistency for write-back caches
- No-op if no data cache present
- Part of '87 architecture extension
- 5-50 cycles (depends on number of dirty lines)
- Critical for I/O and context switch coherence
- No operands required (implicit cache operation)
- May cause memory bus contention during flush

This instruction is critical for maintaining memory coherence in systems with write-back caches. It is used before context switches, I/O operations, and when memory consistency must be guaranteed. The instruction is part of the '87 architecture extension.

**Common Use Cases:**
- Cache flush before context switch
- Ensuring I/O consistency
- Memory coherence operations
- Pre-shutdown cache synchronization

**Operands:** 0 (implicit cache operation)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE1E | DDIRT |

---

## Operands

**No explicit operands** (operates on data cache)

**Result**: All dirty cache lines written to memory, dirty bits cleared

---

## Trap Conditions
- **None**: Instruction completes without traps

---

## Data Status Bits
- **Z,S,C,V**: Unaffected

---

## Examples

### Example 1: Basic cache flush
```assembly
DDIRT
```

### Example 2: Pre-context switch flush
```assembly
CONTEXT_SWITCH:
        % Flush dirty cache data
        DDIRT
        % Save CPU state
        CALL SAVE_REGISTERS
        % Load new context
        CALL LOAD_CONTEXT
        RET
```

### Example 3: Before DMA operation
```assembly
DMA_SETUP:
        % Ensure memory is current
        DDIRT
        % Program DMA controller
        CALL INIT_DMA
        % Start transfer
        CALL START_DMA
        RET
```

### Example 4: Memory checkpoint
```assembly
CHECKPOINT:
        % Flush all pending writes
        DDIRT
        % Mark memory state
        CALL SAVE_CHECKPOINT
        RET
```

### Example 5: System shutdown
```assembly
SHUTDOWN:
        % Flush all cached data
        DDIRT
        % Write page tables
        CWIP
        CPGU
        % Safe to power down
        CALL HALT_SYSTEM
```

### Example 6: Cache coherence for multiprocessor
```assembly
RELEASE_LOCK:
        % Ensure writes visible
        DDIRT
        % Release lock
        W MOVE 0, LOCK_VAR
        RET
```

### Example 7: Debug memory inspection
```assembly
DUMP_MEMORY:
        % Flush cache first
        DDIRT
        % Read memory for debugging
        CALL READ_MEM_RANGE
        RET
```

---

## Performance Notes
- Execution: 5-50 cycles (depends on dirty lines)
- No-op if no cache present
- Time proportional to number of dirty cache lines
- May cause memory bus contention

---

## Reference Manual
**Section:** §16.11
**Title:** DDIRT - Dump 'Dirty' ('87 extension)

---

## See Also
- [CWIP](cwip.md) - Clear written in page table
- [CPGU](cpgu.md) - Clear page used table
- [RPHS](rphs.md) - Read physical segment
