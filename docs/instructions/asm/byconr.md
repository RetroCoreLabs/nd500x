# BYCONR - Byte Convert with Rounding

## Overview

**Mnemonic:** `byconr`
**Function:** Convert floating-point to byte with rounding
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `F BYCONR <source>,<dest>` or `D BYCONR <source>,<dest>`

---

## Description

Converts a floating-point number (single-precision or double-precision) to a signed byte integer value with rounding, storing the result in the destination operand. This instruction performs controlled precision reduction from floating-point to 8-bit integer representation with proper rounding according to the current rounding mode.

**Operation:**
```
rounded_value = round(<source>)
if (rounded_value < -128 or rounded_value > 127):
    trap (Integer Overflow)
<dest> = (byte)rounded_value
```

**Key Characteristics:**
- Float/double to signed byte conversion with rounding
- 2 source types supported (F=32-bit, D=64-bit)
- IEEE 754 rounding semantics (typically round-to-nearest)
- IOV trap when result exceeds -128 to +127 range
- FO trap on NaN or infinity source
- Sets Z and S flags based on converted result
- Essential for quantization and compact storage
- Used in sensor data, image processing, signal processing
- More accurate than truncation (vs BYCON)

The source operand contains a floating-point value that is:
1. Rounded to the nearest integer value
2. Range-checked to ensure it fits in signed byte range (-128 to +127)
3. Converted to 8-bit two's complement format
4. Stored in the destination operand

Rounding follows IEEE 754 semantics, typically "round to nearest, ties to even" unless the rounding mode has been changed. If the rounded result exceeds the range of a signed byte (-128 to +127), an integer overflow trap occurs.

This instruction is commonly used when:
- Converting floating-point calculations to byte-sized integer outputs
- Storing floating-point results in compact byte arrays
- Interfacing floating-point algorithms with byte-oriented protocols
- Implementing quantization for signal processing or image manipulation

The Z (zero) and S (sign) status bits are set based on the converted result, allowing immediate conditional branching on the byte value produced.

**Operands:** 2
**Variants:** 2 opcode(s)

---

## Variants

| Variant | Opcode | Prefix | Assembly Notation | Source Type | Dest Type |
|---------|--------|--------|-------------------|-------------|-----------|
| 1/2 | 0xFE70 | F | F BYCONR | Float (32-bit) | Byte |
| 2/2 | 0xFE71 | D | D BYCONR | Double (64-bit) | Byte |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Floating-point value to convert

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Byte (8-bit signed integer)
- **Role**: Location to store converted byte value

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Floating overflow (FO)**: Source is NaN or infinity
- **Integer overflow (O)**: Rounded result exceeds byte range (-128 to +127)

---

## Data Status Bits

- **Z (Zero)**: Set if converted result equals zero
- **S (Sign)**: Set if converted result is negative (bit 7 = 1)

---

## Examples

### Example 1: Convert temperature reading to byte

```assembly
% Convert float temperature to signed byte for storage
        F BYCONR E1, B.TEMP_BYTE
        % E1 = 23.7 -> TEMP_BYTE = 24 (rounded)
```

### Example 2: Double precision scientific result to byte

```assembly
% Round double-precision result to byte
        D BYCONR B.RESULT_DOUBLE, B.OUTPUT
        % RESULT_DOUBLE = -42.3 -> OUTPUT = -42
```

### Example 3: Array element conversion

```assembly
% Convert float array to byte array
CONVERT_LOOP:
        F BYCONR FLOAT_ARRAY(I1), BYTE_ARRAY(I1)
        IF=0 Z GO ZERO_VALUE      % Branch if converted to 0
        W1 ADD 1, I1              % Next element
        W1 COMP I1, COUNT
        IF<GO CONVERT_LOOP
```

### Example 4: Range checking with overflow trap

```assembly
% Convert with overflow detection
        F BYCONR E1, R
        % Traps if E1 > 127.5 or E1 < -128.5
        % R contains valid byte if no trap
```

### Example 5: Signal quantization

```assembly
% Quantize audio sample to 8-bit
        F BYCONR SAMPLE, QUANTIZED
        IF<0 S GO NEGATIVE_SAMPLE
        % QUANTIZED contains rounded 8-bit value
```

### Example 6: Image pixel conversion

```assembly
% Convert normalized pixel value to byte
        D BYCONR NORMALIZED_PIXEL, PIXEL_BYTE
        % NORMALIZED_PIXEL = 127.8 -> PIXEL_BYTE = 128
        % Result would trap (overflow) as 128 exceeds +127
```

### Example 7: Constant rounding

```assembly
% Round constant to byte
        F BYCONR 3.14159, B.PI_BYTE
        % PI_BYTE = 3
        D BYCONR 2.71828, B.E_BYTE
        % E_BYTE = 3
```

---

## Performance Notes

- **Rounding Mode**: Uses current FPU rounding mode (typically round-to-nearest)
- **Range Validation**: Hardware checks -128 to +127 range
- **Trap on Overflow**: Integer overflow trap if result out of range
- **Trap on NaN/Inf**: Floating overflow trap if source is NaN or infinity
- **Status Bits**: Z and S reflect final byte value, not floating-point value
- **Typical Use**: Sensor data quantization, protocol conversion, compact storage
- **Precision Loss**: Fractional part discarded after rounding

---

## Reference Manual

**Section:** §15.3
**Title:** Data type conversion with rounding

---

## See Also

- [HCONR](hconr.md) - Convert to halfword with rounding
- [WCONR](wconr.md) - Convert to word with rounding
- [FCONR](fconr.md) - Convert to float with rounding
- [BYCON](bycon.md) - Convert to byte (truncation, no rounding)
