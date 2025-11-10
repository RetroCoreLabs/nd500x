# DMOF - Data Memory Management Off

## Overview

**Mnemonic:** `dmof`
**Function:** Disable data memory management
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `DMOF`

---

## Description

Disables the data memory management system, causing subsequent data memory accesses to be interpreted as direct physical addresses rather than virtual addresses requiring translation. This privileged instruction is essential for low-level system initialization, debugging, and direct hardware access.

When DMOF executes, the data memory management unit (MMU) is disabled for all load and store operations. This means:
- No page table translation for data accesses
- No segment descriptor checking for data
- No capability-based protection for data
- Direct physical memory access for all data operations

Operating with data memory management disabled provides direct hardware access but sacrifices memory protection and virtual addressing benefits. This mode is primarily used during:
- System boot and early initialization
- Low-level hardware diagnostics
- Direct I/O device register access
- Memory controller configuration
- Recovery from data memory management faults
- Physical memory testing

Unlike PMOF which affects instruction fetches and transfers control to the L register, DMOF only affects data memory operations (loads and stores) and does not alter program flow. The instruction pointer continues normally after DMOF execution.

If data memory management is already disabled when DMOF executes, the instruction has no effect, making it idempotent and safe to execute redundantly.

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF18 | DMOF |

---

## Operands

None - operates on implicit data MMU state.

---

## Trap Conditions

- **Illegal Instruction Code (IIC)**: If executed in user mode (privilege violation)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: System boot - physical memory initialization

```assembly
% Early boot code - disable data MMU to initialize memory
        DMOF                    % Data accesses now physical
        % Initialize memory controller
        W MOVE MEMCONFIG, MEMCTRL_BASE
        % Can now access physical memory directly
```

### Example 2: Direct I/O device register access

```assembly
% Access device registers at physical addresses
        DMOF                    % Disable data translation
        W MOVE COMMAND, IO_DEVICE_BASE
        W MOVE 0, IO_DEVICE_BASE+2
        DMON                    % Re-enable translation
```

### Example 3: Physical memory diagnostics

```assembly
% Memory test requires physical addressing
MEMORY_TEST:
        DMOF                    % Physical data access
        W MOVE TEST_PATTERN, PHYS_ADDR
        W MOVE PHYS_ADDR, R     % Read back
        W COMP TEST_PATTERN, R
        IF<>Z GO TEST_FAILED
        DMON                    % Restore virtual addressing
        RET
```

### Example 4: Data MMU fault recovery

```assembly
DATA_MMU_FAULT_HANDLER:
        % Disable data MMU to access recovery structures
        DMOF
        % Log fault information at physical addresses
        W MOVE FAULT_INFO, LOG_PHYS_BASE
        % Recovery code runs with physical data access
        CALL HANDLE_DATA_FAULT
        DMON                    % Re-enable if recovered
        RET
```

### Example 5: DMA buffer setup with physical addresses

```assembly
% Configure DMA with physical buffer address
        DMOF                    % Get physical addresses
        W MOVE BUFFER, R        % R = physical address
        DMON                    % Restore virtual addressing
        % Program DMA controller with R
        W MOVE R, DMA_ADDR_REG
```

### Example 6: Memory-mapped hardware configuration

```assembly
% Configure memory controller at power-on
INIT_MEMORY:
        DMOF                    % Physical access needed
        W MOVE BANKS, MEMCTRL_BANKS
        W MOVE TIMING, MEMCTRL_TIMING
        W MOVE REFRESH, MEMCTRL_REFRESH
        DMON                    % Enable MMU for OS
        RET
```

### Example 7: Redundant disable (idempotent)

```assembly
% Safely disable even if already disabled
        DMOF                    % First disable
        % ... physical memory operations ...
        DMOF                    % No effect - already disabled
        W MOVE VALUE, PHYS_LOC
```

---

## Performance Notes

- **Scope**: Affects only data memory accesses (load/store operations)
- **Program Flow**: Does not alter instruction pointer or control flow
- **Idempotent**: Safe to execute when already disabled
- **No Translation**: Subsequent data accesses bypass MMU entirely
- **Protection Disabled**: No segmentation or capability checking for data
- **Typical Use**: Boot initialization, hardware diagnostics, I/O access
- **Paired with DMON**: DMON re-enables data memory management
- **Contrast with PMOF**: PMOF affects instruction fetches and jumps to L register
- **Cache Implications**: Some implementations may flush data cache on mode change

---

## Reference Manual

**Section:** §16.15
**Title:** Data memory management off

---

## See Also

- [DMON](dmon.md) - Data memory management on
- [PMOF](pmof.md) - Program memory management off
- [PMON](pmon.md) - Program memory management on
- [DCTSB](dctsb.md) - Data translation speedup buffer clear
