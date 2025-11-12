# OTE2:= - Load Own trap enable 2

## Overview

**Mnemonic:** `ote2=`
**Function:** Load Own trap enable 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `OTE2:= <operand>`

---

## Description

Loads the OTE2 (Own Trap Enable 2) register from the specified operand. OTE2 contains trap enable bits 16-31, controlling which trap conditions are enabled for the current process.

**Operation:**
```
<operand> → OTE2
```

**Key Characteristics:**
- Loads trap enable bits 16-31
- Controls process-specific trap behavior
- Essential for context restoration
- Paired with OTE1 for full 32-bit trap mask
- Supervisor or user privilege depending on mode
- Used in exception handler state restoration

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Own trap enable 2 |

---

## Operands

### Operand 1 (Source)

Value to load into Own trap enable 2.

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
        OTE2:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        OTE2:= I1
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

- [OTE2=:](ote2=_.md) - Store Own trap enable 2
- [Context Switching](../ContextSwitching.md)
