# LREGBL - Load Register Block

## Overview

**Mnemonic:** `lregbl`
**Function:** Load multiple registers from logical memory
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `LREGBL <mask>, <address>`

---

## Description

Loads multiple CPU registers from logical memory according to a bitmask. This privileged instruction is part of the '87 architecture extension and efficiently restores register state from memory. The mask specifies which registers to load, and registers are loaded from consecutive memory locations starting at the specified address plus the register number times 4.

**Operation:**
```
For each bit set in mask:
    memory[address + register_number * 4] → register
```

**Key Characteristics:**
- Supervisor-mode bulk register loading ('87 extension)
- Selective register restoration via bitmask
- Logical memory access (uses MMU)
- Mask reduction in user mode (safety)
- Faster than individual register loads (2-4 cycles per reg)
- Essential for context switching and exception handling
- Addresses based on register number (addr + reg# * 4)
- 2 operands (mask, address)
- Domain registers loaded via PS/CED pointers

When executed in non-privileged mode, the mask is automatically reduced to include only registers modifiable by non-privileged code. Registers residing in the domain information table are loaded to the logical addresses pointed to by PS (Process Status) and CED (Current Execution Domain) registers.

**Common Use Cases:**
- Fast context restoration
- Function prologue register restoration
- Exception handler state reload
- Multi-register initialization

**Operands:** 2
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFF6 | LREGBL |

---

## Operands

**Operand 1** (Mask, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word
- **Role**: Bitmask specifying which registers to load

**Operand 2** (Address, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word
- **Role**: Logical memory address of register block

**Result**: Selected registers loaded from memory

---

## Trap Conditions
- **Addressing traps**: Invalid memory access or protection violation
- **Privilege violation**: Full mask requires supervisor mode

---

## Data Status Bits
- **Z,S,C,V**: Dependent on loaded values

---

## Examples

### Example 1: Load all general registers
```assembly
LREGBL 0xFFFF, SAVE_AREA
```

### Example 2: Load specific register subset
```assembly
% Load only I and A registers (bits 0-7)
LREGBL 0x00FF, REG_BUFFER
```

### Example 3: Function return with register restore
```assembly
RESTORE_AND_RETURN:
        LREGBL SAVED_MASK, B.SAVE_AREA
        RET
```

### Example 4: Exception handler epilogue
```assembly
EXCEPTION_EXIT:
        % Restore registers from exception frame
        LREGBL 0xFFFF, EXCEPTION_REGS
        RET
```

### Example 5: Context switch register load
```assembly
SWITCH_IN:
        % Load incoming thread registers
        W MOVE THREAD_MASK, I1
        W MOVE THREAD_REGS, I2
        LREGBL I1, I2
        RET
```

### Example 6: Partial register restoration
```assembly
% Restore only working registers (I1-I4)
LREGBL 0x000F, LOCAL_SAVE
```

### Example 7: Stack-based register restore
```assembly
EPILOGUE:
        % Restore registers from stack frame
        LREGBL FRAME_MASK, B.REG_SAVE
        % Clean up stack
        W ADD FRAME_SIZE, I7, SP
        RET
```

---

## Performance Notes
- Execution: Variable (2-4 cycles per register)
- Much faster than individual MOV instructions
- Logical address access (uses MMU)
- Mask reduction overhead in user mode

---

## Reference Manual
**Section:** §16.27.2
**Title:** LREGBL - Load register block ('87 extension)

---

## See Also
- [SREGBL](sregbl.md) - Save register block
- [LCNTXT](lcntxt.md) - Load context block
- [SCNTXT](scntxt.md) - Save context block
