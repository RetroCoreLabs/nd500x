# OTE2=: - Store Own trap enable 2

## Overview

**Mnemonic:** `ote2=`
**Function:** Store Own trap enable 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `OTE2=: <operand>`

---

## Description

Stores the Own trap enable 2 to the specified operand. System register store operation.

**Operation:** `<operand> = OTE2`

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
- [Context Switching](../ContextSwitching.md)
