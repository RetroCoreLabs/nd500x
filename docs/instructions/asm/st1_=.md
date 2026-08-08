# ST1:= - Load First status register

## Overview

**Mnemonic:** `st1=`
**Function:** Load First status register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `ST1:= <operand>`

---

## Description

Loads the ST1 (First Status Register) from the specified operand. ST1 contains various CPU status and control bits including interrupt enables, privilege level, and system modes.

**Operation:**
```
<operand> → ST1
```

**Key Characteristics:**
- Loads CPU status and control bits
- Contains interrupt enable flags
- Includes privilege level indicators
- Essential for context restoration
- Supervisor-level register
- Used in exception handlers and OS context restoration

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
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
