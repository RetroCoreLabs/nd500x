# TOS:= - Load Top of stack

## Overview

**Mnemonic:** `tos=`
**Function:** Load Top of stack
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TOS:= <operand>`

---

## Description

Loads the Top of stack from the specified operand. This is a system register used for stack pointer.

**Operation:** `TOS = <operand>`

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
