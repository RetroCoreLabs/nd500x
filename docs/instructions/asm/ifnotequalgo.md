# IF><GO - Conditional Jump If Not Equal

## Overview

**Mnemonic:** `if><go`
**Function:** Conditional jump if zero flag clear (not equal)
**Class:** BRANCH
**Privilege:** user

**Format:** `IF >< GO <<displacement>>`

---

## Description

Transfers control if Z=0 (zero flag clear), indicating values are not equal. Used after COMP or TEST to branch when operands differ.

**Operation:**
```
if Z=0 then PC += displacement
```

**Key Characteristics:**
- Tests not-equal condition (Z=0)
- Opposite of IF=GO (which tests Z=1)
- Follows COMP, TEST, or arithmetic operations
- Two displacement ranges (byte: ±127, halfword: ±32767)
- Pipeline flush on taken branch
- Most common conditional branch in loops and comparisons

**Operands:** 1
**Variants:** 2 opcode(s)

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00C6 | Byte |
| 2/2 | 0x00C7 | Halfword |

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

### Example 1: Loop while not zero

```assembly
LOOP:
        W DECR B.COUNTER
        IF >< GO LOOP
```

### Example 2: Compare and branch if different

```assembly
        W COMP B.A, B.B
        IF >< GO DIFFERENT
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
