# IF <= GO - Conditional Jump Less/Equal (Signed)

## Overview

**Mnemonic:** `if<=go`
**Function:** Conditional jump less/equal (signed)
**Class:** BRANCH
**Privilege:** user

**Format:** `IF <= GO <<displacement>>`

---

## Description

Transfers control if S=1 or Z=1 (less than or equal, signed comparison). Tests that sign flag is set OR zero flag is set, indicating the first operand is less than or equal to the second in signed arithmetic.

**Operation:**
```
if S=1 or Z=1 then PC += displacement
```

**Key Characteristics:**
- Tests signed comparison (two's complement)
- Requires S=1 (sign set) OR Z=1 (equal)
- Follows COMP or TEST with signed operands
- Two displacement ranges (byte: ±127, halfword: ±32767)
- Pipeline flush on taken branch
- Common in loop bounds and signed range checking

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00CE | Byte |
| 2/2 | 0x00CF | Halfword |

---

## Operands

### Operand 1 (Displacement)

Signed displacement.

---

## Trap Conditions

- **Branch trap (BT):** If enabled

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Basic usage

```assembly
        W COMP B.A, B.B
        IF <= GO LE
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF <= GO:B LOOP
```

---

## Performance Notes

- **Cycles:** 2-3 (not taken), 3-4 (taken)

---

## Reference Manual

**Section:** §13.3
**Title:** Conditional Jump

---

## See Also

- [IF=GO](if=go.md) - Jump if equal
- [COMP](comp.md) - Compare
- [GO](go.md) - Unconditional jump
