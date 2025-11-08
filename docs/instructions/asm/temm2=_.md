# TEMM2=: - Store Trap enable modification mask 2

## Overview

**Mnemonic:** `temm2=`
**Function:** Store Trap enable modification mask 2
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `TEMM2=: <operand>`

---

## Description

Stores the Trap enable modification mask 2 to the specified operand. System register store operation.

**Operation:** `<operand> = TEMM2`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Trap enable modification mask 2 |

---

## Operands

### Operand 1 (Destination)

Where to store Trap enable modification mask 2 value.

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
        TEMM2=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        TEMM2=: I1
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

- [TEMM2:=](temm2_=.md) - Load Trap enable modification mask 2
- [Context Switching](../ContextSwitching.md)
