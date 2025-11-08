# ST1=: - Store First status register

## Overview

**Mnemonic:** `st1=`
**Function:** Store First status register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `ST1=: <operand>`

---

## Description

Stores the First status register to the specified operand. System register store operation.

**Operation:** `<operand> = ST1`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store First status register |

---

## Operands

### Operand 1 (Destination)

Where to store First status register value.

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
        ST1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        ST1=: I1
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

- [ST1:=](st1_=.md) - Load First status register
- [Context Switching](../ContextSwitching.md)
