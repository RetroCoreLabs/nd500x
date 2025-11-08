# OTE1:= - Load Own trap enable 1

## Overview

**Mnemonic:** `ote1=`
**Function:** Load Own trap enable 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `OTE1:= <operand>`

---

## Description

Loads the Own trap enable 1 from the specified operand. This is a system register used for trap enable bits 0-15.

**Operation:** `OTE1 = <operand>`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Own trap enable 1 |

---

## Operands

### Operand 1 (Source)

Value to load into Own trap enable 1.

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
        OTE1:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        OTE1:= I1
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

- [OTE1=:](ote1=_.md) - Store Own trap enable 1
- [Context Switching](../ContextSwitching.md)
