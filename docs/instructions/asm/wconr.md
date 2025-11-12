# WCONR - Word Convert with Rounding

## Overview

**Mnemonic:** `wconr`
**Function:** Convert floating-point to word with rounding
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `F WCONR <source>,<dest>` or `D WCONR <source>,<dest>`

---

## Description

Converts a floating-point number (single-precision or double-precision) to a signed word (32-bit) integer value with rounding, storing the result in the destination operand. This instruction performs controlled precision reduction from floating-point to 32-bit integer representation with proper rounding according to the current rounding mode.

**Operation:**
```
rounded_value = round(<source>)
if (rounded_value < -2147483648 or rounded_value > 2147483647):
    trap (Integer Overflow)
<dest> = (word)rounded_value
```

**Key Characteristics:**
- Float/double to signed word (32-bit) conversion with rounding
- 2 source types supported (F=32-bit, D=64-bit)
- IEEE 754 rounding semantics (typically round-to-nearest)
- IOV trap when result exceeds ±2,147,483,647 range
- FO trap on NaN or infinity source
- Sets Z and S flags based on converted result
- Most common float-to-int conversion instruction
- All single-precision floats fit in word range
- Essential for general-purpose floating-point to integer conversion
- More accurate than truncation (vs WCON)

The source operand contains a floating-point value that is:
1. Rounded to the nearest integer value
2. Range-checked to ensure it fits in signed word range (-2,147,483,648 to +2,147,483,647)
3. Converted to 32-bit two's complement format
4. Stored in the destination operand

Rounding follows IEEE 754 semantics, typically "round to nearest, ties to even" unless the rounding mode has been changed. If the rounded result exceeds the range of a signed 32-bit word, an integer overflow trap occurs.

This instruction is the most commonly used floating-point to integer conversion because:
- Word integers provide full 32-bit range (±2.1 billion)
- Single-precision floats (23-bit mantissa) fit entirely within word range
- Double-precision conversions can represent most practical values
- Many algorithms require full-precision integer results
- Operating system interfaces typically use word-sized integers

The word size provides sufficient range for most applications including financial calculations, scientific measurements, array indices, counters, and general-purpose arithmetic. This makes WCONR the preferred conversion instruction for most scenarios where floating-point results need to be converted to integer representation.

The Z (zero) and S (sign) status bits are set based on the converted result, allowing immediate conditional branching on the word value produced.

**Operands:** 2
**Variants:** 2 opcode(s)

---

## Variants

| Variant | Opcode | Prefix | Assembly Notation | Source Type | Dest Type |
|---------|--------|--------|-------------------|-------------|-----------|
| 1/2 | 0xFE74 | F | F WCONR | Float (32-bit) | Word (32-bit) |
| 2/2 | 0xFE75 | D | D WCONR | Double (64-bit) | Word (32-bit) |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Floating-point value to convert

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (32-bit signed integer)
- **Role**: Location to store converted word value

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Floating overflow (FO)**: Source is NaN or infinity
- **Integer overflow (O)**: Rounded result exceeds word range (±2,147,483,647)

---

## Data Status Bits

- **Z (Zero)**: Set if converted result equals zero
- **S (Sign)**: Set if converted result is negative (bit 31 = 1)

---

## Examples

### Example 1: Convert financial calculation to cents

```assembly
% Convert dollar amount to integer cents
        F WCONR TOTAL_DOLLARS, TOTAL_CENTS
        % 1234.567 -> 123457 cents (rounded)
```

### Example 2: Array index calculation

```assembly
% Convert floating-point index to integer
        D WCONR INDEX_FLOAT, I1
        % Use I1 as array index after rounding
        W MOVE ARRAY(I1), R
```

### Example 3: Scientific measurement to integer

```assembly
% Convert measurement in mm to micrometers
        F WCONR MM_VALUE, MICROMETERS
        % 123.4567 mm -> 123457 µm (rounded)
```

### Example 4: Loop counter from calculation

```assembly
% Calculate iteration count from formula
        F WCONR ITERATION_COUNT, I2
        % I2 now contains rounded iteration count
LOOP:
        % ... loop body ...
        W1 SUB 1, I2
        IF>0 GO LOOP
```

### Example 5: Time conversion to milliseconds

```assembly
% Convert seconds (float) to milliseconds (int)
        F WCONR SECONDS_FLOAT, MILLISECONDS
        % 12.3456 seconds -> 12346 ms (rounded)
```

### Example 6: Overflow detection for large doubles

```assembly
% Convert large double with overflow check
        D WCONR LARGE_DOUBLE, B.RESULT
        % Traps if LARGE_DOUBLE > 2147483647.5
        % Trap handler can substitute max value
```

### Example 7: Coordinate conversion

```assembly
% Convert screen coordinates from normalized to pixels
        F WCONR NORMALIZED_X, PIXEL_X
        F WCONR NORMALIZED_Y, PIXEL_Y
        % Both coordinates rounded to nearest pixel
```

---

## Performance Notes

- **Rounding Mode**: Uses current FPU rounding mode (typically round-to-nearest)
- **Range Validation**: Hardware checks ±2,147,483,647 range
- **Trap on Overflow**: Integer overflow trap if result out of range
- **Trap on NaN/Inf**: Floating overflow trap if source is NaN or infinity
- **Status Bits**: Z and S reflect final word value, not floating-point value
- **Typical Use**: General-purpose float-to-int conversion, most common variant
- **Precision Loss**: Fractional part discarded after rounding
- **Float Safety**: All single-precision float values fit in word range
- **Double Risk**: Large doubles (> ±2.1B) will trap on overflow

---

## Reference Manual

**Section:** §15.3
**Title:** Data type conversion with rounding

---

## See Also

- [BYCONR](byconr.md) - Convert to byte with rounding
- [HCONR](hconr.md) - Convert to halfword with rounding
- [FCONR](fconr.md) - Convert to float with rounding
- [WCON](wcon.md) - Convert to word (truncation, no rounding)
