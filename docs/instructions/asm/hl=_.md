# HL=: - Store Upper limit register

## Overview

**Mnemonic:** `hl=`
**Function:** Store Upper limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `HL=: <operand>`

---

## Description

Stores the Upper limit register to the specified operand. System register store operation.

**Operation:** `<operand> = HL`

**Operands:** 1
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
