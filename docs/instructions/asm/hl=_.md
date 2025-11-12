# HL=: - Store Upper limit register

## Overview

**Mnemonic:** `hl=`
**Function:** Store Upper limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `HL=: <operand>`

---

## Description

Stores the HL (High Limit) register to the specified operand. The HL register contains the upper address limit for stack bounds checking. Used in context switching and stack management operations.

**Operation:**
```
HL → <operand>
```

**Key Characteristics:**
- Stores stack upper limit boundary
- Part of stack bounds checking mechanism
- Used in context switching
- Works with LL (Low Limit) register
- System register access

**Common Use Cases:**
- Context switch save/restore
- Stack bounds inspection
- Memory protection setup
- Process switching
- Debugging stack issues

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Upper limit register |

---

## Operands

### Operand 1 (Destination)

Where to store Upper limit register value.

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
        HL=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        HL=: I1
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

- [HL:=](hl_=.md) - Load Upper limit register
- [Context Switching](../ContextSwitching.md)
