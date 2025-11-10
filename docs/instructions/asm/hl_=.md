# HL:= - Load Upper limit register

## Overview

**Mnemonic:** `hl=`
**Function:** Load Upper limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `HL:= <operand>`

---

## Description

Loads the Upper limit register from the specified operand. This is a system register used for stack upper bound.

**Operation:** `HL = <operand>`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Upper limit register |

---

## Operands

### Operand 1 (Source)

Value to load into Upper limit register.

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
        HL:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        HL:= I1
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

- [HL=:](hl=_.md) - Store Upper limit register
- [Context Switching](../ContextSwitching.md)
