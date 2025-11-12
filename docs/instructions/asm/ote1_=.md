# OTE1:= - Load Own trap enable 1

## Overview

**Mnemonic:** `ote1=`
**Function:** Load Own trap enable 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `OTE1:= <operand>`

---

## Description

Loads the OTE1 (Own Trap Enable 1) register from the specified operand. OTE1 contains trap enable bits 0-15, controlling which trap conditions are enabled for the current process.

**Operation:**
```
<operand> → OTE1
```

**Key Characteristics:**
- Loads trap enable bits 0-15
- Controls process-specific trap behavior
- Essential for context restoration
- Paired with OTE2 for full 32-bit trap mask
- Supervisor or user privilege depending on mode
- Used in exception handler state restoration

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
