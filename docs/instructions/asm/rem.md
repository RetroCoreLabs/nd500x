# REM - Remainder (Modulo)

## Overview

**Mnemonic:** `rem`
**Function:** Integer remainder (modulo) operation
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t REM <dividend>, <divisor>, <result>`

---

## Description

Computes the integer remainder of dividing the dividend by the divisor, storing the result in the destination operand. This instruction implements the modulo operation (dividend mod divisor), which returns the remainder after integer division.

The remainder has the same sign as the dividend and satisfies the mathematical relationship:
```
dividend = (dividend / divisor) * divisor + remainder
```

For example:
- 17 REM 5 = 2 (because 17 = 3×5 + 2)
- -17 REM 5 = -2 (because -17 = -4×5 + (-2))
- 17 REM -5 = 2 (because 17 = -3×(-5) + 2)

**Key Characteristics:**

1. **Three-Operand Form**: Allows flexible operand combinations (dividend and divisor can be memory or registers)
2. **Sign Handling**: Remainder takes the sign of the dividend (not the divisor)
3. **Division by Zero**: Traps with divide fault (DVF) if divisor is zero
4. **Data Types**: Supports float (F), double (D), and register (R) operands
5. **Quotient Discarded**: Only the remainder is stored; for quotient use DIV instruction

**Common Use Cases:**
- Modular arithmetic (hash functions, cyclic buffers)
- Even/odd number detection (n REM 2)
- Digit extraction (number REM 10 for last digit)
- Array index wrapping (index REM array_size)
- Time calculations (seconds REM 60, minutes REM 60)
- Remainder checking in division algorithms
- Constraint enforcement (value REM constraint)

REM is often used with DIV/DIV4 when both quotient and remainder are needed. The ND-500 provides both as separate instructions rather than a single instruction that returns both values.

**Operands:** 3
**Variants:** 8 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation | Data Types |
|---------|--------|-------------------|------------|
| 1/8 | 0xFE58 | F/D/R REM | Float/Double/Register |
| 2/8 | 0xFE59 | F/D/R REM | Float/Double/Register |
| 3/8 | 0xFE5A | F/D/R REM | Float/Double/Register |
| 4/8 | 0xFE5B | F/D/R REM | Float/Double/Register |
| 5/8 | 0xFE5C | F/D/R REM | Float/Double/Register |
| 6/8 | 0xFE5D | F/D/R REM | Float/Double/Register |
| 7/8 | 0xFE5E | F/D/R REM | Float/Double/Register |
| 8/8 | 0xFE5F | F/D/R REM | Float/Double/Register |

**Note**: Variants differ by register addressing and operand combinations.

---

## Operands

**Operand 1** (Dividend, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as instruction prefix (F/D/R)
- **Role**: Value to be divided (numerator)

**Operand 2** (Divisor, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as instruction prefix (F/D/R)
- **Role**: Value to divide by (denominator)

**Operand 3** (Result, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as instruction prefix (F/D/R)
- **Role**: Destination for remainder

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Divide fault (DVF)**: Divisor is zero
- **Floating-point exceptions**: For F/D types (overflow, underflow, invalid operation)

---

## Data Status Bits

- **Z (Zero)**: Set if remainder is zero (dividend evenly divisible by divisor)
- **S (Sign)**: Set if remainder is negative (matches dividend sign)
- **V (Overflow)**: Set if floating-point overflow occurs
- **C (Carry)**: Undefined for REM

---

## Examples

### Example 1: Even/odd detection

```assembly
% Check if number is even or odd
        W REM NUMBER, 2, REMAINDER
        IF=0 Z GO EVEN_HANDLER
        % Odd number
        GO ODD_HANDLER
EVEN_HANDLER:
        % Even number
```

### Example 2: Extract last digit

```assembly
% Get ones digit of a number
        W REM VALUE, 10, ONES_DIGIT
        % ONES_DIGIT now contains 0-9
```

### Example 3: Circular buffer index wrap

```assembly
% Wrap index around buffer size
        W1 ADD INDEX, 1, I1          % Increment index
        W1 REM I1, BUFFER_SIZE, I1   % Wrap if >= size
        W1 MOVE I1, INDEX             % Store wrapped index
```

### Example 4: Time calculation (seconds to hours/minutes/seconds)

```assembly
% Convert total seconds to H:M:S
        W REM TOTAL_SECS, 60, SECS   % Seconds component
        W DIV TOTAL_SECS, 60, TEMP   % Total minutes
        W REM TEMP, 60, MINS         % Minutes component
        W DIV TEMP, 60, HOURS        % Hours component
```

### Example 5: Hash function modulo

```assembly
% Hash key to table index
        W REM HASH_VALUE, TABLE_SIZE, TABLE_INDEX
        % TABLE_INDEX = HASH_VALUE mod TABLE_SIZE
```

### Example 6: Day of week calculation

```assembly
% Calculate day of week (0-6) from day number
        W REM DAY_NUMBER, 7, DAY_OF_WEEK
        % 0=Sunday, 1=Monday, ..., 6=Saturday
```

### Example 7: Sign handling demonstration

```assembly
% Demonstrate sign rules
        W REM 17, 5, RESULT1     % RESULT1 = 2 (positive dividend)
        W REM -17, 5, RESULT2    % RESULT2 = -2 (negative dividend)
        W REM 17, -5, RESULT3    % RESULT3 = 2 (positive dividend)
        % Remainder always has dividend's sign
```

---

## Performance Notes

- **vs DIV+MUL+SUB**: REM is significantly faster than computing remainder manually
- **Division by Power of 2**: For divisors like 2, 4, 8, use AND with mask instead (much faster)
  - `W REM N, 4, R` can be replaced by `W AND N, 3, R` (3 = 0x11binary)
- **Zero Check**: Always traps on zero divisor - check beforehand if needed
- **Quotient Needed Too**: If both quotient and remainder needed, use DIV4 (4-operand division)
- **Floating-Point**: REM on floating-point is slower than integer remainder
- **Optimization**: Compilers often optimize power-of-2 divisors to AND operations

---

## Reference Manual

**Section:** §11.x (estimated)
**Title:** Remainder operation

---

## See Also

- [DIV](div.md) - Division (quotient only)
- [DIV4](div4.md) - Division with both quotient and remainder
- [UDIV](udiv.md) - Unsigned division
- [AND](and.md) - Bitwise AND (for fast power-of-2 modulo)
- [UMUL](umul.md) - Unsigned multiplication
