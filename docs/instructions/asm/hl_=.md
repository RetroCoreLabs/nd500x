# HL:= - Load Upper limit register

## Overview

**Mnemonic:** `hl=`
**Function:** Load Upper limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `HL:= <operand>`

---

## Description

Loads the HL (High/Upper Limit) register from the specified operand. HL contains the upper stack boundary address, used for stack overflow detection and protection.

**Operation:**
```
<operand> → HL
```

**Key Characteristics:**
- Loads upper stack boundary address
- Prevents stack overflow
- Essential for stack protection restoration
- Paired with LL for full stack bounds
- Used in context restoration
- Critical for runtime stack validation

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
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
