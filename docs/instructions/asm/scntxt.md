# SCNTXT - Save Context Block

## Overview

**Mnemonic:** `scntxt`
**Function:** Save CPU context to physical memory
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `SCNTXT <mask>, <address>`

---

## Description

Saves the context block (registers) of the current process to physical memory according to the specified mask. Each bit in the mask corresponds to a register; if set, that register is saved. Registers are stored at address + (register_number × 4). If address is 0, the process's designated context save area is used. This is the complement of LCNTXT.

**Operation:**
```
for each bit set in mask:
    register[bit] → physical_memory[address + bit*4]
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- Selective register save via mask
- Saves to physical memory (not virtual)
- Address 0 uses process context save area
- Used for process/thread switching
- Foundation for multitasking OS

**Common Use Cases:**
- Process context switching
- Thread state preservation
- Exception handler context save
- Debugging state capture
- Checkpointing for recovery

**Operands:** 2 (mask, physical address)
**Variants:** 1 opcode

---

## Examples

### Example 1: Save all registers

```assembly
        % Full context save
        W1 := 0xFFFF
        W2 := CTX_SAVE_ADDR
        SCNTXT W1, W2
```

**Explanation:** Save complete CPU state to specified address.

### Example 2: Use process save area

```assembly
        % Save to process's designated area
        W1 := 0xFFFF
        W2 CLR                 % address = 0
        SCNTXT W1, W2
```

**Explanation:** Save to OS-defined process context area.

### Example 3: Partial context save

```assembly
        % Save only I registers (bits 0-3)
        W1 := 0x000F
        W2 := PARTIAL_CTX
        SCNTXT W1, W2
```

**Explanation:** Save subset of registers for lightweight switching.

### Example 4: Exception handler

```assembly
        % Save state on exception
EXCEPTION_HANDLER:
        W1 := 0xFFFF
        W2 := EXCEPTION_CTX
        SCNTXT W1, W2
        % Process exception
        % Restore with LCNTXT
```

**Explanation:** Preserve CPU state during exception handling.

### Example 5: Thread context switch

```assembly
        % Save current thread context
        W1 := 0xFFFF
        W2 := THREADS(CURRENT_TID)
        SCNTXT W1, W2
        % Load next thread with LCNTXT
```

**Explanation:** Save thread state before switching to another.

---

## Register Mask Bits

| Bit(s) | Register(s) | Description |
|--------|-------------|-------------|
| 0-3 | I1-I4 | Index registers |
| 4-7 | A1-A4 | Address registers |
| 8-11 | E1-E4 | Extension registers |
| 12 | L | Link register |
| 13 | B | Base register |
| 14 | R | Result register |
| 15 | PC | Program counter |

---

## Process Context Save Area

When address = 0, the context save area address is calculated as:
```
(process_number + 1) × 256 + OS_defined_base
```

---

## Trap Conditions

- **Addressing traps**: Invalid physical address
- **Privilege violation**: Executed in user mode

---

## Data Status Bits

- **Unaffected**: All flags remain unchanged

---

## Reference Manual

**Section:** §16.27.3
**Title:** SCNTXT - Save context block ('87 extension)

---

## See Also

- [LCNTXT](lcntxt.md) - Load context block
- [SREGBL](sregbl.md) - Save register block
- [LREGBL](lregbl.md) - Load register block
- [SWITCH](switch.md) - Process switch
