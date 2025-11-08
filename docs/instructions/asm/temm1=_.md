# TEMM1=: - Store Trap enable modification mask 1

## Overview

**Mnemonic:** `temm1=`
**Function:** Store Trap enable modification mask 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TEMM1=: <operand>`

---

## Description

Stores the Trap enable modification mask 1 to the specified operand. System register store operation.

**Operation:** `<operand> = TEMM1`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Trap enable modification mask 1 |

---

## Operands

### Operand 1 (Destination)

Where to store Trap enable modification mask 1 value.

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
        TEMM1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        TEMM1=: I1
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

- [TEMM1:=](temm1_=.md) - Load Trap enable modification mask 1
- [Context Switching](../ContextSwitching.md)
