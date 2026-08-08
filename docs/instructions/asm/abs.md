# ABS - Absolute Value

## Overview

**Mnemonic:** `abs`
**Function:** Absolute value
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn ABS`

---

## Description

Calculates the absolute value of the contents of the specified register and stores the result in the same register. The instruction operates on any of the four general registers (n=1 to 4) with various data types (BY, H, W, F, D).

**Operation:**
```
if Rn < 0 then Rn = -Rn
else Rn = Rn (unchanged)
```

**Key Characteristics:**
- In-place absolute value (register modified directly)
- Conditional negation (only negative values changed)
- Supports 5 data types (BY, H, W, F, D)
- Works with 4 index registers (I1-I4)
- Overflow only when ABS(MIN_INT) exceeds range (e.g., -32768 for H)
- Upper bits cleared for BY/H types
- Common in distance calculations and magnitude operations
- More efficient than conditional NEG sequence

When the datatype is either BY (byte) or H (halfword), the result is stored in the least significant bits and the rest of the register is cleared.

Overflow occurs if and only if the greatest negative integer is negated (e.g., for word integers, negating 80000000H causes overflow since +2147483648 cannot be represented in 32-bit two's complement).

**Operands:** 0
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20 (5 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/20 | 0xFF00 | BY | 1 | Byte |
| 2/20 | 0xFF01 | BY | 2 | Byte |
| 3/20 | 0xFF02 | BY | 3 | Byte |
| 4/20 | 0xFF03 | BY | 4 | Byte |
| 5/20 | 0xFF04 | H | 1 | Halfword |
| 6/20 | 0xFF05 | H | 2 | Halfword |
| 7/20 | 0xFF06 | H | 3 | Halfword |
| 8/20 | 0xFF07 | H | 4 | Halfword |
| 9/20 | 0xFF08 | W | 1 | Word |
| 10/20 | 0xFF09 | W | 2 | Word |
| 11/20 | 0xFF0A | W | 3 | Word |
| 12/20 | 0xFF0B | W | 4 | Word |
| 13/20 | 0xFF0C | F | 1 | Float |
| 14/20 | 0xFF0D | F | 2 | Float |
| 15/20 | 0xFF0E | F | 3 | Float |
| 16/20 | 0xFF0F | F | 4 | Float |
| 17/20 | 0xFF0C | D | 1 | Double Float |
| 18/20 | 0xFF0D | D | 2 | Double Float |
| 19/20 | 0xFF0E | D | 3 | Double Float |
| 20/20 | 0xFF0F | D | 4 | Double Float |

---

## Operands

This instruction takes no operands. It operates directly on the specified register.

---

## Trap Conditions

- **Integer overflow (O):** Occurs when negating the greatest negative integer (e.g., -2147483648 for word integers)

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Always cleared (0) - absolute value is always non-negative
- **O (Overflow):** Set if integer overflow occurs, cleared otherwise (integer types only)
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Double float absolute value

```assembly
        % Take absolute value of double precision register D1
        D1 ABS
```

### Example 2: Word absolute value

```assembly
        % Take absolute value of word register I2
        W2 ABS
```

### Example 3: Byte absolute value with overflow check

```assembly
        % Take absolute value of byte in I3
        BY3 ABS
        IF-KGO OVERFLOW_HANDLER    % Branch if overflow occurred
```

### Example 4: Float absolute value in computation

```assembly
        % Compute absolute difference: |A - B|
        F1 := B.VALUE_A
        F1 - B.VALUE_B
        F1 ABS                     % Get absolute value
        F1 =: B.RESULT
```

---

## Performance Notes

- **Typical cycles:** 2-3 cycles
- **Best case:** 2 cycles (value already positive)
- **Worst case:** 3 cycles (value negative, requires negation)

---

## Reference Manual

**Section:** §10.15
**Title:** Absolute value

---

## See Also

- [NEG](neg.md) - Negate (two's complement)
- [INT](int.md) - Integer part (truncate floating point)
- [Trap System](../../ND-500-TRAPS.md)
