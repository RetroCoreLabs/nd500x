# L=: - Store Link register

## Overview

**Mnemonic:** `l=`
**Function:** Store Link register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `L=: <operand>`

---

## Description

Stores the Link register to the specified operand. System register store operation.

**Operation:** `<operand> = L`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Link register |

---

## Operands

### Operand 1 (Destination)

Where to store Link register value.

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
        L=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        L=: I1
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

- [L:=](l_=.md) - Load Link register
- [Context Switching](../ContextSwitching.md)
