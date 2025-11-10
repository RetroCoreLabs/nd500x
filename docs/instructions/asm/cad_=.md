# CAD:= - Load Current alternative domain

## Overview

**Mnemonic:** `cad=`
**Function:** Load Current alternative domain
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CAD:= <operand>`

---

## Description

Loads the Current alternative domain from the specified operand. This is a system register used for domain register.

**Operation:** `CAD = <operand>`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Current alternative domain |

---

## Operands

### Operand 1 (Source)

Value to load into Current alternative domain.

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
        CAD:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        CAD:= I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.33
**Title:** Load special register

---

## See Also

- [CAD=:](cad=_.md) - Store Current alternative domain
- [Context Switching](../ContextSwitching.md)
