# CTE1=: - Store Child trap enable 1

## Overview

**Mnemonic:** `cte1=`
**Function:** Store Child trap enable 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CTE1=: <operand>`

---

## Description

Stores the Child trap enable 1 to the specified operand. System register store operation.

**Operation:** `<operand> = CTE1`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Child trap enable 1 |

---

## Operands

### Operand 1 (Destination)

Where to store Child trap enable 1 value.

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
        CTE1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        CTE1=: I1
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

- [CTE1:=](cte1_=.md) - Load Child trap enable 1
- [Context Switching](../ContextSwitching.md)
