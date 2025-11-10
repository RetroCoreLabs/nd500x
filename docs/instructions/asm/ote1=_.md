# OTE1=: - Store Own trap enable 1

## Overview

**Mnemonic:** `ote1=`
**Function:** Store Own trap enable 1
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `OTE1=: <operand>`

---

## Description

Stores the Own trap enable 1 to the specified operand. System register store operation.

**Operation:** `<operand> = OTE1`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Own trap enable 1 |

---

## Operands

### Operand 1 (Destination)

Where to store Own trap enable 1 value.

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
        OTE1=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        OTE1=: I1
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

- [OTE1:=](ote1_=.md) - Load Own trap enable 1
- [Context Switching](../ContextSwitching.md)
