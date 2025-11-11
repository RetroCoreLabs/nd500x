# TOS=: - Store Top of stack

## Overview

**Mnemonic:** `tos=`
**Function:** Store Top of stack
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TOS=: <operand>`

---

## Description

Stores the TOS (Top Of Stack) register to the specified operand. The TOS register contains the current stack pointer position. Essential for stack management and context switching.

**Operation:**
```
TOS → <operand>
```

**Key Characteristics:**
- Stores current stack pointer value
- Essential for context switching
- Used in stack unwinding
- Part of process state
- System register access

**Common Use Cases:**
- Context switch save/restore
- Stack frame inspection
- Exception handling
- Process switching
- Stack overflow detection

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Top of stack |

---

## Operands

### Operand 1 (Destination)

Where to store Top of stack value.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Store to local variable

```assembly
        TOS=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        TOS=: I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.8
**Title:** Store special register

---

## See Also

- [TOS:=](tos_=.md) - Load Top of stack
- [Context Switching](../ContextSwitching.md)
