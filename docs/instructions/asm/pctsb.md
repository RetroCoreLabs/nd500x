# PCTSB - Program Clear Translation Speedup Buffer

## Overview

**Mnemonic:** `pctsb`
**Function:** Clear program memory translation buffer
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `PCTSB`

---

## Description

Clears the entire program translation speedup buffer (TSB), forcing subsequent program memory accesses to reinitialize address translations from the capability table, segment table, and page index table. This privileged instruction maintains address translation coherence after changes to memory management structures.

The program TSB caches virtual-to-physical address mappings for instruction fetches, dramatically improving performance by avoiding repeated page table walks. PCTSB invalidates all cached translations, ensuring that modifications to page tables, segment descriptors, or capability entries are immediately reflected in program memory access.

When the program TSB is cleared, the associated program cache is also automatically cleared, since cached instructions may have been fetched using now-invalid address translations.

This instruction is essential after:
- Modifying page table entries
- Changing segment descriptors
- Updating capability tables
- Context switches between processes
- Enabling/disabling memory protection

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF1C | PCTSB |

---

## Operands

None - operates on implicit program TSB.

---

## Trap Conditions

- **Illegal Instruction Code (IIC)**: If executed in user mode (privilege violation)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: After page table modification

```assembly
% Modify page table entry
        W MOVE NEW_PTE, PAGE_TABLE(I1)
        % Clear TSB to ensure new mapping used
        PCTSB
        % Future instruction fetches use new translation
```

### Example 2: Process context switch

```assembly
% Switch to new process context
        W MOVE NEW_PCB, CURRENT_PCB
        % Clear translation caches for new address space
        PCTSB                   % Program translations
        DCTSB                   % Data translations
        % New process can execute safely
```

### Example 3: Enabling memory protection

```assembly
% Enable protection on code segment
        CALL SET_SEGMENT_PROTECTION
        % Clear TSB to activate protection
        PCTSB
        % Protected code region now enforced
```

### Example 4: Dynamic code relocation

```assembly
% Relocate code to new physical pages
        CALL REMAP_CODE_SEGMENT
        PCTSB                   % Invalidate old mappings
        % Code executes from new location
```

### Example 5: TLB shootdown for SMP

```assembly
% Invalidate TSB on all CPUs after page table change
        CALL MODIFY_SHARED_PAGE_TABLE
        PCTSB                   % Clear local CPU's TSB
        % Send IPI to other CPUs to clear their TSBs
        CALL BROADCAST_TSB_FLUSH
```

---

## Performance Notes

- **TSB Purpose**: Caches virtual-to-physical address translations
- **Automatic Cache Clear**: Program cache cleared along with TSB
- **Performance Impact**: Next instruction fetch incurs page table walk
- **Scope**: Clears ALL entries, not selective
- **Related Operation**: Similar to TLB flush in modern architectures
- **Typical Use**: Page table updates, context switches, protection changes
- **Paired with DCTSB**: Often both TSBs cleared together

---

## Reference Manual

**Section:** §16.24
**Title:** Clear translation speedup buffer

---

## See Also

- [DCTSB](dctsb.md) - Data translation speedup buffer clear
- [PCC](pcc.md) - Program cache clear
- [PMOF](pmof.md) - Program memory management off
- [PMON](pmon.md) - Program memory management on
