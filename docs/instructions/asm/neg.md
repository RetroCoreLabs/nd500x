# NEG - Negate

## Overview

**Mnemonic:** `neg`
**Function:** Negate (two's complement or sign inversion)
**CLASS:** ARITHMETIC
**Privilege:** user

**Format:** `tn NEG`

---

## Description

Negates the contents of the specified register. Integer values are negated using two's complement. Floating point values are negated by inverting the sign bit.

Operation: `Rn = -Rn`

Byte and halfword negate clear the upper part of the register. Integer overflow occurs only when negating the largest negative integer. Carry is zero except when integer zero is negated.

**Operands:** 0
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20 (5 data types × 4 registers)

| Data Type | Registers | Opcodes |
|-----------|-----------|---------|
| BY | 1-4 | 0x0090-0x0093 |
| H | 1-4 | 0x0094-0x0097 (some duplicates) |
| W | 1-4 | 0x0096-0x0097 (overlaps) |
| F | 1-4 | 0xFE08-0xFE0B |
| D | 1-4 | 0xFE0C-0xFE0F |

---

## Trap Conditions

- **Integer overflow (O):** Only when negating largest negative integer

---

## Data Status Bits

- **Z:** Set if result = 0
- **S:** Set to sign bit of result
- **O:** Set if overflow
- **C:** Zero except when negating zero

---

## Examples

### Example 1: Negate integer

```assembly
        % Negate word register
        W1 NEG
```

### Example 2: Change sign of float

```assembly
        % Invert sign of float value
        F2 NEG
```

### Example 3: Absolute value

```assembly
        % Get absolute value
        W1 := B.VALUE
        W TEST I1
        IF<GO DONE
        W1 NEG
DONE:
```

---

## Performance Notes

- **Typical:** 2-3 cycles
- **Float:** 2 cycles (just sign bit flip)

---

## Reference Manual

**Section:** §10.12
**Title:** Negate

---

## See Also

- [SUB2](sub2.md), [ABS](abs.md)
