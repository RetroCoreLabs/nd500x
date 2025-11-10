# PMON - Program Memory Management On

## Overview

**Mnemonic:** `pmon`
**Function:** Enable program memory management (MMU)
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `PMON`

---

## Description

Enables the program memory management system, activating virtual-to-physical address translation for instruction fetches. After this instruction executes, all subsequent instruction accesses are mapped through the MMU rather than being interpreted as direct physical addresses. The virtual address of the next instruction is loaded from the L register into the program counter.

If the MMU is already enabled, this instruction transfers control to the address in the L register without further effect. This allows safe enabling of memory management or controlled program counter updates within an MMU-enabled environment.

**Operation:**
```
enable MMU
L → P
```

**Common Use Cases:**
- OS kernel MMU initialization
- Virtual memory activation during boot
- Controlled switch from physical to virtual addressing
- Process context activation

**Operands:** 0 (implicit operation)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFF17 | PMON |

---

## Operands

**No explicit operands** (operates on L register and MMU state)

**Result**: MMU enabled, PC loaded from L register

---

## Trap Conditions
- **Illegal instruction code (IIC)**: If not in supervisor mode

---

## Data Status Bits
- **Z,S,C,V**: Unaffected

---

## Examples

### Example 1: Basic MMU enable
```assembly
PMON
```

### Example 2: OS boot sequence
```assembly
BOOT_ENABLE_MMU:
        % Set up page tables
        CALL INIT_PAGE_TABLES
        % Load virtual start address
        W MOVE KERNEL_START, L
        % Enable MMU and jump to virtual address
        PMON
        % Now running with MMU enabled
```

### Example 3: Process startup
```assembly
START_PROCESS:
        % Configure process page tables
        CALL SETUP_PROCESS_TABLES
        % Load process entry point
        W MOVE ENTRY_POINT, L
        % Activate MMU for process
        PMON
```

### Example 4: Safe MMU enable check
```assembly
ENABLE_IF_NEEDED:
        % Check if MMU already on
        % (PMON is safe to call repeatedly)
        W MOVE VIRTUAL_ADDR, L
        PMON
        % Guaranteed to be at VIRTUAL_ADDR with MMU on
```

### Example 5: Context switch with MMU
```assembly
SWITCH_TO_VIRTUAL:
        % Update page table base
        CALL SET_PAGE_TABLE
        % Load new PC value
        W MOVE NEW_PC, L
        % Activate MMU
        PMON
```

### Example 6: System initialization
```assembly
INIT_VIRTUAL_MEM:
        % Initialize PST
        CALL INIT_PST
        % Clear page tables
        CPGU
        CWIP
        % Set up kernel mappings
        CALL MAP_KERNEL
        % Enable MMU
        W MOVE KERNEL_MAIN, L
        PMON
```

### Example 7: Recovery from physical mode
```assembly
RETURN_TO_VIRTUAL:
        % Restore virtual addressing
        W MOVE RESUME_ADDR, L
        PMON
        % Back in virtual address space
        RET
```

---

## Performance Notes
- Execution: 4-6 cycles
- PC update included
- Subsequent instruction fetches use MMU
- No effect if MMU already enabled

---

## Reference Manual
**Section:** §16.14
**Title:** Program Memory Management On

---

## See Also
- [PMOFF](pmoff.md) - Program memory management off
- [CPGU](cpgu.md) - Clear page used table
- [CWIP](cwip.md) - Clear written in page table
