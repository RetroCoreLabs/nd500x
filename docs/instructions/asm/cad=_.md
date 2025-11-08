# CAD=: - Store Current alternative domain

## Overview

**Mnemonic:** `cad=`
**Function:** Store Current alternative domain
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `CAD=: <operand>`

---

## Description

Stores the Current alternative domain to the specified operand. System register store operation.

**Operation:** `<operand> = CAD`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Current alternative domain |

---

## Operands

### Operand 1 (Destination)

Where to store Current alternative domain value.

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
        CAD=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        CAD=: I1
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

- [CAD:=](cad_=.md) - Load Current alternative domain
- [Context Switching](../ContextSwitching.md)
