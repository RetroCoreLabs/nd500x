# P=: - Store Program counter

## Overview

**Mnemonic:** `p=`
**Function:** Store Program counter
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `P=: <operand>`

---

## Description

Stores the P (Program Counter) register to the specified operand. P contains the address of the next instruction to be executed, critical for control flow and debugging.

**Operation:**
```
P → <operand>
```

**Key Characteristics:**
- Stores next instruction address
- Essential for position-independent code
- Used in debugging and profiling
- Enables computed jumps and dispatch tables
- Critical for exception handling
- Required for context switching and snapshots

**Operands:** 1
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Program counter |

---

## Operands

### Operand 1 (Destination)

Where to store Program counter value.

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
        P=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        P=: I1
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

- [P:=](p_=.md) - Load Program counter
- [MOVE instruction reference](../../instruction-reference/MOVE.md)
