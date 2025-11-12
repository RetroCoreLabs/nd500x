# IF >>= GO - Conditional Jump Greater/Equal Magnitude

## Overview

**Mnemonic:** `if>>=go`
**Function:** Conditional jump greater/equal magnitude
**Class:** BRANCH
**Privilege:** user

**Format:** `IF >>= GO <<displacement>>`

---

## Description

Transfers control if C=1 (greater or equal magnitude), typically following a magnitude comparison. Tests carry flag set, indicating the first operand has greater than or equal absolute value to the second.

**Operation:**
```
if C=1 then PC += displacement
```

**Key Characteristics:**
- Tests magnitude comparison (unsigned/absolute value)
- Only requires C=1 (carry set)
- Follows COMP or TEST instructions
- Two displacement ranges (byte: ±127, halfword: ±32767)
- Pipeline flush on taken branch
- Common in unsigned arithmetic and range checking

**Operands:** 1
**Variants:** 2

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00D6 | Byte |
| 2/2 | 0x00D7 | Halfword |

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
        IF >>= GO GE_MAG
```

### Example 2: Loop control

```assembly
LOOP:
        % loop body
        IF >>= GO:B LOOP
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
