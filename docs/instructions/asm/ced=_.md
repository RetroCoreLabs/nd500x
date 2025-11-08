# CED=: - Store Current executing domain

## Overview

**Mnemonic:** `ced=`
**Function:** Store Current executing domain
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CED=: <operand>`

---

## Description

Stores the Current executing domain to the specified operand. System register store operation.

**Operation:** `<operand> = CED`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Current executing domain |

---

## Operands

### Operand 1 (Destination)

Where to store Current executing domain value.

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
        CED=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        CED=: I1
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

- [CED:=](ced_=.md) - Load Current executing domain
- [Context Switching](../ContextSwitching.md)
