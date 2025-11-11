# TOS:= - Load Top of stack

## Overview

**Mnemonic:** `tos=`
**Function:** Load Top of stack
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TOS:= <operand>`

---

## Description

Loads the TOS (Top Of Stack) register from the specified operand. TOS contains the current stack pointer address, pointing to the top of the active stack frame.

**Operation:**
```
<operand> → TOS
```

**Key Characteristics:**
- Loads current stack pointer address
- Points to top of active stack frame
- Essential for context restoration
- Used in exception handling and returns
- Supervisor or user privilege depending on mode
- Critical for stack management and unwinding

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Top of stack |

---

## Operands

### Operand 1 (Source)

Value to load into Top of stack.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Load from local variable

```assembly
        TOS:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        TOS:= I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.7
**Title:** Load special register

---

## See Also

- [TOS=:](tos=_.md) - Store Top of stack
- [Context Switching](../ContextSwitching.md)
