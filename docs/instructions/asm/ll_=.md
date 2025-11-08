# LL:= - Load Lower limit register

## Overview

**Mnemonic:** `ll=`
**Function:** Load Lower limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `LL:= <operand>`

---

## Description

Loads the Lower limit register from the specified operand. This is a system register used for stack lower bound.

**Operation:** `LL = <operand>`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Lower limit register |

---

## Operands

### Operand 1 (Source)

Value to load into Lower limit register.

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
        LL:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        LL:= I1
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

- [LL=:](ll=_.md) - Store Lower limit register
- [Context Switching](../ContextSwitching.md)
