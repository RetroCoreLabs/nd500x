# DCTSB - Data Clear Translation Speedup Buffer

## Overview

**Mnemonic:** `dctsb`
**Function:** Clear data memory translation buffer
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `DCTSB`

---

## Description

Clears the entire data translation speedup buffer (TSB), forcing subsequent data memory accesses to reinitialize address translations from the capability table, segment table, and page index table. This privileged instruction maintains address translation coherence after changes to data memory management structures.

The data TSB caches virtual-to-physical address mappings for data accesses, dramatically improving performance by avoiding repeated page table walks. DCTSB invalidates all cached translations, ensuring that modifications to page tables, segment descriptors, or capability entries are immediately reflected in data memory access.

When the data TSB is cleared, the associated data cache is also automatically cleared and dirty data is written back to memory, since cached data may have been accessed using now-invalid address translations.

This instruction is essential after:
- Modifying data page table entries
- Changing data segment descriptors
- Updating capability tables for data
- Context switches between processes
- Enabling/disabling data memory protection

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF1D | DCTSB |

---

## Operands

None - operates on implicit data TSB.

---

## Trap Conditions

- **Illegal Instruction Code (IIC)**: If executed in user mode (privilege violation)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: After data page table modification

```assembly
% Modify data page table entry
        W MOVE NEW_PTE, DATA_PAGE_TABLE(I1)
        % Clear TSB to ensure new mapping used
        DCTSB
        % Future data accesses use new translation
```

### Example 2: Process context switch

```assembly
% Switch to new process address space
        W MOVE NEW_PCB, CURRENT_PCB
        % Clear both translation caches
        PCTSB                   % Program translations
        DCTSB                   % Data translations
        % New process address space active
```

### Example 3: Shared memory mapping change

```assembly
% Remap shared memory segment
        CALL MODIFY_SHARED_SEGMENT
        % Invalidate translations for new mapping
        DCTSB
        % Data accesses use new segment mapping
```

### Example 4: Dynamic memory protection

```assembly
% Enable write protection on data segment
        CALL SET_DATA_PROTECTION
        % Clear TSB to activate protection
        DCTSB
        % Write protection now enforced
```

### Example 5: TLB shootdown for multiprocessor

```assembly
% Coordinate TSB flush across CPUs
        CALL MODIFY_SHARED_PAGE_TABLE
        DCTSB                   % Clear local CPU's data TSB
        % Send inter-processor interrupt to other CPUs
        CALL BROADCAST_TSB_INVALIDATE
```

---

## Performance Notes

- **TSB Purpose**: Caches data virtual-to-physical translations
- **Automatic Cache Flush**: Data cache cleared and dirty data written back
- **Performance Impact**: Next data access incurs page table walk
- **Scope**: Clears ALL data TSB entries
- **Related Operation**: Similar to data TLB flush in modern architectures
- **Typical Use**: Page table updates, context switches, protection changes
- **Paired with PCTSB**: Often both TSBs cleared together for complete flush

---

## Reference Manual

**Section:** §16.24
**Title:** Clear translation speedup buffer

---

## See Also

- [PCTSB](pctsb.md) - Program translation speedup buffer clear
- [DCC](dcc.md) - Data cache clear
- [DMOF](dmof.md) - Data memory management off
- [DMON](dmon.md) - Data memory management on
