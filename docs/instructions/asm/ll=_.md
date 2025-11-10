# LL=: - Store Lower limit register

## Overview

**Mnemonic:** `ll=`
**Function:** Store Lower limit register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `LL=: <operand>`

---

## Description

Stores the Lower limit register to the specified operand. System register store operation.

**Operation:** `<operand> = LL`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Lower limit register |

---

## Operands

### Operand 1 (Destination)

Where to store Lower limit register value.

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
        LL=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        LL=: I1
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

- [LL:=](ll_=.md) - Load Lower limit register
- [Context Switching](../ContextSwitching.md)
