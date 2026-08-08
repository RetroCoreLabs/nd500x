# OTE2=: - Store Own trap enable 2

## Overview

**Mnemonic:** `ote2=`
**Function:** Store Own trap enable 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `OTE2=: <operand>`

---

## Description

Stores the OTE2 (Own Trap Enable 2) register to the specified operand. OTE2 contains trap enable bits 16-31, controlling which trap conditions are enabled for the current process.

**Operation:**
```
OTE2 → <operand>
```

**Key Characteristics:**
- Stores trap enable bits 16-31
- Controls process-specific trap behavior
- Essential for context switching
- Paired with OTE1 for full 32-bit trap mask
- Supervisor or user privilege depending on mode
- Used in exception handler state management

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Own trap enable 2 |

---

## Operands

### Operand 1 (Destination)

Where to store Own trap enable 2 value.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Store to local variable

```assembly
        OTE2=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        OTE2=: I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.8
**Title:** Store special register

---

## See Also

- [OTE2:=](ote2_=.md) - Load Own trap enable 2
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
