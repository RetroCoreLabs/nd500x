# FCONR - Float Convert with Rounding

## Overview

**Mnemonic:** `fconr`
**Function:** Convert integer or double to float with rounding
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `W FCONR <source>,<dest>` or `D FCONR <source>,<dest>`

---

## Description

Converts an integer word or double-precision floating-point number to a single-precision (32-bit) float value with rounding, storing the result in the destination operand. This instruction performs controlled precision conversion to 32-bit IEEE 754 floating-point format with proper rounding according to the current rounding mode.

The instruction has two distinct conversion modes:

**W FCONR (Word to Float)**:
- Converts 32-bit signed integer to float
- Always exact for integers within ±16,777,216 (2^24)
- Larger integers may require rounding due to 23-bit mantissa
- No overflow possible (all word integers fit in float exponent range)

**D FCONR (Double to Float)**:
- Converts 64-bit double-precision to 32-bit float
- Precision reduction from 52-bit to 23-bit mantissa
- May require rounding for values requiring extended precision
- Overflow possible if double exponent exceeds float range

Rounding follows IEEE 754 semantics, typically "round to nearest, ties to even" unless the rounding mode has been changed. The converted value maintains the sign and approximate magnitude of the source.

This instruction is commonly used when:
- Converting integer calculations to floating-point format
- Reducing double-precision results to single-precision for storage
- Interfacing integer algorithms with floating-point processing
- Implementing mixed-precision arithmetic
- Preparing data for graphics or signal processing (typically single-precision)

The Z (zero) and S (sign) status bits are set based on the converted result, allowing immediate conditional branching on the float value produced.

**Operands:** 2
**Variants:** 2 opcode(s)

---

## Variants

| Variant | Opcode | Prefix | Assembly Notation | Source Type | Dest Type |
|---------|--------|--------|-------------------|-------------|-----------|
| 1/2 | 0xFE83 | W | W FCONR | Word (32-bit int) | Float (32-bit) |
| 2/2 | 0xFE84 | D | D FCONR | Double (64-bit) | Float (32-bit) |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W prefix) or Double (D prefix)
- **Role**: Integer or double value to convert

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (32-bit IEEE 754)
- **Role**: Location to store converted float value

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Floating overflow (FO)**: Double source exceeds float exponent range

---

## Data Status Bits

- **Z (Zero)**: Set if converted result equals zero
- **S (Sign)**: Set if converted result is negative

---

## Examples

### Example 1: Convert integer counter to float

```assembly
% Convert loop counter to floating-point
        W FCONR I1, E1
        % I1 = 1000 -> E1 = 1000.0 (exact)
```

### Example 2: Integer array index to float coordinate

```assembly
% Convert array index to normalized coordinate
        W FCONR INDEX, COORDINATE
        % INDEX = 256 -> COORDINATE = 256.0
        F DIV COORDINATE, ARRAY_SIZE, NORMALIZED
```

### Example 3: Reduce double precision to float

```assembly
% Store double result as single-precision
        D FCONR B.RESULT_DOUBLE, E2
        % Reduces precision, saves memory
```

### Example 4: Mixed integer-float calculation

```assembly
% Calculate float average from integer sum
        W FCONR SUM, SUM_FLOAT
        W FCONR COUNT, COUNT_FLOAT
        F DIV SUM_FLOAT, COUNT_FLOAT, AVERAGE
```

### Example 5: Large integer with rounding

```assembly
% Convert large integer (precision loss)
        W FCONR LARGE_INT, APPROX_FLOAT
        % LARGE_INT = 123456789
        % APPROX_FLOAT ≈ 123456792.0 (rounded)
```

### Example 6: Graphics coordinate conversion

```assembly
% Convert pixel coordinates to float for transformation
        W FCONR PIXEL_X, FLOAT_X
        W FCONR PIXEL_Y, FLOAT_Y
        % Ready for matrix transformations
```

### Example 7: Scientific data reduction

```assembly
% Reduce high-precision data to float for storage
        D FCONR PRECISE_MEASUREMENT, STORED_VALUE
        % Save space while maintaining useful precision
```

---

## Performance Notes

- **Rounding Mode**: Uses current FPU rounding mode (typically round-to-nearest)
- **Integer Exact Range**: Integers ±16,777,216 (2^24) convert exactly
- **Integer Precision Loss**: Larger integers rounded to nearest representable float
- **Double Range Check**: Overflow trap if double exceeds ±3.4×10^38
- **No Integer Overflow**: All word integers fit in float range
- **Status Bits**: Z and S reflect final float value
- **Typical Use**: Integer-to-float conversion, precision reduction, mixed arithmetic
- **W FCONR Common**: Most frequent variant for integer-to-float conversion
- **D FCONR Lossy**: Always involves precision reduction from 52-bit to 23-bit mantissa

---

## Reference Manual

**Section:** §15.3
**Title:** Data type conversion with rounding

---

## See Also

- [BYCONR](byconr.md) - Convert to byte with rounding
- [HCONR](hconr.md) - Convert to halfword with rounding
- [WCONR](wconr.md) - Convert to word with rounding
- [FCON](fcon.md) - Convert to float (truncation, no rounding)
