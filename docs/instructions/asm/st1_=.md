# ST1:= - Load First status register

## Overview

**Mnemonic:** `st1=`
**Function:** Load First status register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `ST1:= <operand>`

---

## Description

Loads the First status register from the specified operand. This is a system register used for CPU status bits.

**Operation:** `ST1 = <operand>`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load First status register |

---

## Operands

### Operand 1 (Source)

Value to load into First status register.

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
        ST1:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        ST1:= I1
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

- [ST1=:](st1=_.md) - Store First status register
- [Context Switching](../ContextSwitching.md)
