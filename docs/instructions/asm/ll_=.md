# LL:= - Load Lower limit register

## Overview

**Mnemonic:** `ll=`
**Function:** Load Lower limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `LL:= <operand>`

---

## Description

Loads the LL (Lower Limit) register from the specified operand. LL contains the lower stack boundary address, used for stack overflow detection and protection.

**Operation:**
```
<operand> → LL
```

**Key Characteristics:**
- Loads lower stack boundary address
- Prevents stack underflow
- Essential for stack protection restoration
- Paired with HL for full stack bounds
- Used in context restoration
- Critical for runtime stack validation

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
