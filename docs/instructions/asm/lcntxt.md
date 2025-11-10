# LCNTXT - Load Context Block

## Overview

**Mnemonic:** `lcntxt`
**Function:** Load context block from physical address
**Class:** SYSTEM
**Privilege:** supervisor
**Format:** `LCNTXT <mask>, <address>, <process number>`

---

## Description

Loads processor context block registers from a specified physical address according to a register mask. This privileged instruction is part of the '87 architecture extension and is used for process context switching and state restoration. The mask specifies which CPU registers to load, the address points to the source memory location, and the process number identifies the target process context.

If the address is 0, the context save area of the current process is used. If the process number is negative, the current process number is maintained. Registers are loaded from memory locations at `address + (register_number * 4)`.

**Operation:**
```
For each bit set in mask:
    memory[address + register_number * 4] → register
```

**Common Use Cases:**
- Process context switching
- Thread state restoration
- Exception handler entry
- Domain switching operations

**Operands:** 3
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFF8 | LCNTXT |

---

## Operands

**Operand 1** (Mask, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word
- **Role**: Bitmask specifying which registers to load

**Operand 2** (Address, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word
- **Role**: Physical memory address or 0 for current context save area

**Operand 3** (Process Number, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word
- **Role**: Target process number or negative to keep current

**Result**: Selected registers loaded from physical memory

---

## Trap Conditions
- **Addressing traps**: Invalid memory access
- **Privilege violation**: If executed in user mode

---

## Data Status Bits
- **Z,S,C,V**: Dependent on loaded values

---

## Examples

### Example 1: Load full context
```assembly
W MOVE 0xFFFF, I1        % All registers
W MOVE CONTEXT_ADDR, I2  % Source address
W MOVE PROC_NUM, I3      % Process number
LCNTXT I1, I2, I3
```

### Example 2: Load partial context
```assembly
% Load only I registers (mask bits 0-3)
LCNTXT 0x000F, SAVE_AREA, CURRENT_PROC
```

### Example 3: Use current process context area
```assembly
% address=0 uses current process save area
LCNTXT REG_MASK, 0, PROC_ID
```

### Example 4: Maintain current process
```assembly
% process_number < 0 maintains current process
LCNTXT 0xFFFF, STATE_ADDR, -1
```

### Example 5: Thread context switch
```assembly
SWITCH_THREAD:
        % Load new thread context
        W MOVE THREAD_MASK, I1
        W MOVE THREAD_CONTEXT, I2
        W MOVE THREAD_ID, I3
        LCNTXT I1, I2, I3
        RET
```

### Example 6: Exception handler state restore
```assembly
RESTORE_STATE:
        % Restore CPU state after exception
        LCNTXT SAVED_MASK, EXCEPTION_CTX, -1
        RET
```

### Example 7: Domain switch with context
```assembly
SWITCH_DOMAIN:
        % Save current context first
        CALL SAVE_CURRENT
        % Load new domain context
        LCNTXT DOMAIN_MASK, DOMAIN_CTX, NEW_DOMAIN
        RET
```

---

## Performance Notes
- Execution: Variable (depends on number of registers in mask)
- Each register load adds 2-3 cycles
- Physical memory access (no MMU translation)
- Requires supervisor mode

---

## Reference Manual
**Section:** §16.27.4
**Title:** LCNTXT - Load context block ('87 extension)

---

## See Also
- [SCNTXT](scntxt.md) - Save context block
- [LREGBL](lregbl.md) - Load register block
- [CPGU](cpgu.md) - Clear page used table
