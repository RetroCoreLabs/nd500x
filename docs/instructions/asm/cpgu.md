# CPGU - Clear Page Used Table

## Overview

**Mnemonic:** `cpgu`
**Function:** Clear entire page used table
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `CPGU`

---

## Description

Clears the entire Page Used (PGU) table to zero. This privileged instruction is used by memory management and swapper routines to reset page usage tracking. The PGU table tracks which physical memory pages are currently in use, supporting demand paging and virtual memory management.

This is an installation-dependent instruction that requires detailed knowledge of the physical memory configuration. It is typically used during system initialization, swapper operations, and memory reclamation. Improper use can corrupt memory management state.

**Operation:**
```
0 → entire PGU table
```

**Common Use Cases:**
- Memory swapper initialization
- Page table reset during context switch
- System boot memory setup
- Memory reclamation operations

**Operands:** 0 (implicit table operation)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFF1A | CPGU |

---

## Operands

**No explicit operands** (operates on PGU table)

**Result**: Entire PGU table cleared to 0

---

## Trap Conditions
- **Illegal instruction code (IIC)**: If not in supervisor mode or instruction not available

---

## Data Status Bits
- **Z,S,C,V**: Unaffected

---

## Examples

### Example 1: Basic PGU table clear
```assembly
CPGU
```

### Example 2: Swapper initialization
```assembly
SWAP_INIT:
        % Clear page usage tracking
        CPGU
        % Clear written-in-page tracking
        CWIP
        % Initialize swapper state
        CALL INIT_SWAP_TABLES
        RET
```

### Example 3: Memory reclaim operation
```assembly
RECLAIM_MEMORY:
        % Save current PGU state
        CALL SAVE_PGU_STATE
        % Clear table
        CPGU
        % Rebuild from active processes
        CALL REBUILD_PGU
        RET
```

### Example 4: Context switch memory reset
```assembly
SWITCH_DOMAIN:
        % Prepare for domain switch
        CPGU
        CWIP
        % Load new domain context
        CALL LOAD_DOMAIN_TABLES
        RET
```

### Example 5: System boot sequence
```assembly
BOOT:
        % Initialize MMU tables
        CPGU
        CWIP
        % Set up initial page mappings
        CALL INIT_PAGE_TABLES
        % Enable MMU
        CALL ENABLE_MMU
        RET
```

### Example 6: Memory defragmentation
```assembly
DEFRAG:
        % Clear usage tracking
        CPGU
        % Compact physical memory
        CALL COMPACT_PAGES
        % Rebuild PGU from new layout
        CALL UPDATE_PGU
        RET
```

### Example 7: Emergency memory cleanup
```assembly
PANIC_RESET:
        % Emergency memory state reset
        CPGU
        CWIP
        % Minimal system restart
        CALL MINIMAL_INIT
        RET
```

---

## Performance Notes
- Execution: 10-20 cycles (table scan)
- Time depends on physical memory size
- Installation-dependent timing
- Requires supervisor mode

---

## Reference Manual
**Section:** §16.22
**Title:** Clear Page Used Table

---

## See Also
- [CWIP](cwip.md) - Clear written in page table
- [RPGU](rpgu.md) - Read page used
- [DDIRT](ddirt.md) - Dump dirty cache
