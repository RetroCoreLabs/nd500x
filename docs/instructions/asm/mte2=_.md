# MTE2=: - Store Mother trap enable 2

## Overview

**Mnemonic:** `mte2=`
**Function:** Store Mother trap enable 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `MTE2=: <operand>`

---

## Description

Stores the Mother trap enable 2 to the specified operand. System register store operation.

**Operation:** `<operand> = MTE2`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Mother trap enable 2 |

---

## Operands

### Operand 1 (Destination)

Where to store Mother trap enable 2 value.

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
        MTE2=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        MTE2=: I1
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

- [MTE2:=](mte2_=.md) - Load Mother trap enable 2
- [Context Switching](../ContextSwitching.md)
