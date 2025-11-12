# IF >> GO - Conditional Jump Greater Magnitude

## Overview

**Mnemonic:** `if>>go`
**Function:** Conditional jump greater magnitude
**Class:** BRANCH
**Privilege:** user

**Format:** `IF >> GO <<displacement>>`

---

## Description

Transfers control if C=1 and Z=0 (greater magnitude), typically following a magnitude comparison. Tests that carry flag is set AND zero flag is clear, indicating the first operand has greater absolute value than the second.

**Operation:**
```
if C=1 and Z=0 then PC += displacement
```

**Key Characteristics:**
- Tests magnitude comparison (unsigned/absolute value)
- Requires both C=1 (carry) and Z=0 (not zero)
- Follows COMP or TEST instructions
- Two displacement ranges (byte: ±127, halfword: ±32767)
- Pipeline flush on taken branch
- Common in unsigned arithmetic and absolute value comparisons

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00D4 | Byte |
| 2/2 | 0x00D5 | Halfword |

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
        IF >> GO GREATER_MAG
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF >> GO:B LOOP
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
