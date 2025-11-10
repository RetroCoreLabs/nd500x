# PS=: - Store Process segment

## Overview

**Mnemonic:** `ps=`
**Function:** Store Process segment
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `PS=: <operand>`

---

## Description

Stores the Process segment to the specified operand. System register store operation.

**Operation:** `<operand> = PS`

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Process segment |

---

## Operands

### Operand 1 (Destination)

Where to store Process segment value.

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
        PS=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        PS=: I1
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

- [PS:=](ps_=.md) - Load Process segment
- [Context Switching](../ContextSwitching.md)
