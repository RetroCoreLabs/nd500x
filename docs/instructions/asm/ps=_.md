# PS=: - Store Process segment

## Overview

**Mnemonic:** `ps=`
**Function:** Store Process segment
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `PS=: <operand>`

---

## Description

Stores the PS (Process Segment) register to the specified operand. PS contains the segment number of the current process's data segment, used in memory segmentation and addressing.

**Operation:**
```
PS → <operand>
```

**Key Characteristics:**
- Stores process data segment identifier
- Essential for memory segmentation
- Used in context switching and debugging
- Controls process memory space
- Supervisor or user privilege depending on mode
- Part of process state management

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
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
