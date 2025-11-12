# CWIP - Clear Written In Page Table

## Overview

**Mnemonic:** `cwip`
**Function:** Clear entire written-in-page table
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `CWIP`

---

## Description

Clears the entire Written In Page (WIP) table to zero. This privileged instruction is used by memory management and swapper routines to reset write-tracking for memory pages. The WIP table tracks which pages have been modified since they were loaded into physical memory, enabling efficient page swapping by identifying which pages need to be written back to disk.

**Operation:**
```
0 → entire WIP table
```

**Key Characteristics:**
- Supervisor-only memory management instruction
- Clears entire Written In Page tracking table
- Resets dirty page tracking for all physical memory
- Essential for swapper initialization and checkpoints
- Installation-dependent (varies with memory size)
- 10-20 cycles (depends on physical memory configuration)
- No operands required (implicit table operation)
- Critical for demand paging systems
- Paired with CPGU for complete tracking reset

This is an installation-dependent instruction requiring detailed knowledge of the physical memory configuration. It is typically used during system initialization, swapper operations, and when resetting memory management state. The WIP table is critical for demand paging systems.

**Common Use Cases:**
- Memory swapper initialization
- Page write-tracking reset
- System initialization
- Memory checkpoint operations

**Operands:** 0 (implicit table operation)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFF1B | CWIP |

---

## Operands

**No explicit operands** (operates on WIP table)

**Result**: Entire WIP table cleared to 0

---

## Trap Conditions
- **Illegal instruction code (IIC)**: If not in supervisor mode or instruction not available

---

## Data Status Bits
- **Z,S,C,V**: Unaffected

---

## Examples

### Example 1: Basic WIP table clear
```assembly
CWIP
```

### Example 2: Swapper initialization
```assembly
INIT_SWAPPER:
        % Clear page usage tracking
        CPGU
        % Clear write tracking
        CWIP
        % Initialize swap buffers
        CALL INIT_SWAP_BUFFERS
        RET
```

### Example 3: Page checkpoint
```assembly
CHECKPOINT_MEMORY:
        % Save all modified pages
        CALL FLUSH_DIRTY_PAGES
        % Clear write tracking
        CWIP
        % Mark checkpoint complete
        CALL SET_CHECKPOINT_FLAG
        RET
```

### Example 4: Memory snapshot
```assembly
SNAPSHOT:
        % Write all dirty pages to disk
        CALL WRITE_MODIFIED_PAGES
        % Reset write tracking
        CWIP
        % Snapshot is now consistent
        RET
```

### Example 5: System boot memory init
```assembly
BOOT_MEMORY:
        % Initialize memory tables
        CPGU
        CWIP
        % Load initial pages
        CALL LOAD_BOOT_PAGES
        % MMU ready
        RET
```

### Example 6: Domain switch preparation
```assembly
SWITCH_CONTEXT:
        % Save current domain pages
        CALL SAVE_DOMAIN_STATE
        % Clear tracking tables
        CWIP
        CPGU
        % Load new domain
        CALL LOAD_DOMAIN
        RET
```

### Example 7: Memory consistency check
```assembly
VERIFY_MEMORY:
        % Ensure all writes flushed
        CALL FLUSH_ALL_CACHES
        % Clear WIP for clean state
        CWIP
        % Verify page consistency
        CALL CHECK_PAGES
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
**Section:** §16.19
**Title:** Clear Written In Page Table

---

## See Also
- [CPGU](cpgu.md) - Clear page used table
- [RWIP](rwip.md) - Read written in page
- [DDIRT](ddirt.md) - Dump dirty cache
