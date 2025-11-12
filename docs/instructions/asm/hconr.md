# HCONR - Halfword Convert with Rounding

## Overview

**Mnemonic:** `hconr`
**Function:** Convert floating-point to halfword with rounding
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `F HCONR <source>,<dest>` or `D HCONR <source>,<dest>`

---

## Description

Converts a floating-point number (single-precision or double-precision) to a signed halfword (16-bit) integer value with rounding, storing the result in the destination operand. This instruction performs controlled precision reduction from floating-point to 16-bit integer representation with proper rounding according to the current rounding mode.

**Operation:**
```
rounded_value = round(<source>)
if (rounded_value < -32768 or rounded_value > 32767):
    trap (Integer Overflow)
<dest> = (halfword)rounded_value
```

**Key Characteristics:**
- Float/double to signed halfword (16-bit) conversion with rounding
- 2 source types supported (F=32-bit, D=64-bit)
- IEEE 754 rounding semantics (typically round-to-nearest)
- IOV trap when result exceeds -32,768 to +32,767 range
- FO trap on NaN or infinity source
- Sets Z and S flags based on converted result
- Essential for compact storage and sensor data conversion
- Memory efficient (50% space vs word integers)
- Used for audio samples, control values, fixed-point arithmetic
- More accurate than truncation (vs HCON)

The source operand contains a floating-point value that is:
1. Rounded to the nearest integer value
2. Range-checked to ensure it fits in signed halfword range (-32,768 to +32,767)
3. Converted to 16-bit two's complement format
4. Stored in the destination operand

Rounding follows IEEE 754 semantics, typically "round to nearest, ties to even" unless the rounding mode has been changed. If the rounded result exceeds the range of a signed halfword (-32,768 to +32,767), an integer overflow trap occurs.

This instruction is commonly used when:
- Converting floating-point calculations to 16-bit integer outputs
- Storing floating-point results in halfword arrays for memory efficiency
- Interfacing floating-point algorithms with 16-bit integer hardware
- Implementing fixed-point arithmetic with proper rounding
- Converting measurement data to compact integer representation

The halfword size provides a good balance between range (±32K) and storage efficiency, making it suitable for many sensor readings, control values, and intermediate calculations that don't require full word precision.

The Z (zero) and S (sign) status bits are set based on the converted result, allowing immediate conditional branching on the halfword value produced.

**Operands:** 2
**Variants:** 2 opcode(s)

---

## Variants

| Variant | Opcode | Prefix | Assembly Notation | Source Type | Dest Type |
|---------|--------|--------|-------------------|-------------|-----------|
| 1/2 | 0xFE72 | F | F HCONR | Float (32-bit) | Halfword (16-bit) |
| 2/2 | 0xFE73 | D | D HCONR | Double (64-bit) | Halfword (16-bit) |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Floating-point value to convert

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Halfword (16-bit signed integer)
- **Role**: Location to store converted halfword value

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Floating overflow (FO)**: Source is NaN or infinity
- **Integer overflow (O)**: Rounded result exceeds halfword range (-32,768 to +32,767)

---

## Data Status Bits

- **Z (Zero)**: Set if converted result equals zero
- **S (Sign)**: Set if converted result is negative (bit 15 = 1)

---

## Examples

### Example 1: Convert sensor reading to halfword

```assembly
% Convert calibrated sensor reading to 16-bit integer
        F HCONR E1, B.SENSOR_VALUE
        % E1 = 1234.7 -> SENSOR_VALUE = 1235 (rounded)
```

### Example 2: Array conversion from double to halfword

```assembly
% Convert double-precision array element to halfword
        D HCONR DESC(RESULTS)(R2), IND(B.ROUNDEDRESULT)(R2)
        % Access R2nd element of each array
```

### Example 3: Control system feedback conversion

```assembly
% Convert PID controller output to 16-bit DAC value
        F HCONR PID_OUTPUT, DAC_VALUE
        IF<0 S GO NEGATIVE_OUTPUT
        % DAC_VALUE ready for hardware interface
```

### Example 4: Fixed-point conversion loop

```assembly
% Convert float array to halfword array
        W1 CLR                    % Index = 0
CONVERT_LOOP:
        F HCONR FLOAT_DATA(I1), HALF_DATA(I1)
        IF=0 Z GO ZERO_RESULT
        W1 ADD 1, I1
        W1 COMP I1, ARRAY_SIZE
        IF<GO CONVERT_LOOP
```

### Example 5: Overflow detection

```assembly
% Convert with range validation
        D HCONR LARGE_VALUE, B.RESULT
        % Traps if LARGE_VALUE > 32767.5 or < -32768.5
        % Trap handler can clamp or report error
```

### Example 6: Audio sample conversion

```assembly
% Convert normalized audio to 16-bit PCM
        F HCONR NORMALIZED_SAMPLE, PCM_SAMPLE
        % NORMALIZED_SAMPLE range: -32768.0 to +32767.0
        % PCM_SAMPLE: standard 16-bit audio format
```

### Example 7: Temperature with fractional degrees

```assembly
% Convert temperature in Celsius to decidegrees (tenths)
% Input: temperature in degrees (e.g., 23.45°C)
        F HCONR TEMP_SCALED, DECIDEGREES
        % 234.5 -> 235 (rounded to nearest decidegree)
```

---

## Performance Notes

- **Rounding Mode**: Uses current FPU rounding mode (typically round-to-nearest)
- **Range Validation**: Hardware checks -32,768 to +32,767 range
- **Trap on Overflow**: Integer overflow trap if result out of range
- **Trap on NaN/Inf**: Floating overflow trap if source is NaN or infinity
- **Status Bits**: Z and S reflect final halfword value, not floating-point value
- **Typical Use**: Sensor data, control values, audio samples, fixed-point conversion
- **Precision Loss**: Fractional part discarded after rounding
- **Memory Efficient**: Halfword storage saves 50% vs word integers

---

## Reference Manual

**Section:** §15.3
**Title:** Data type conversion with rounding

---

## See Also

- [BYCONR](byconr.md) - Convert to byte with rounding
- [WCONR](wconr.md) - Convert to word with rounding
- [FCONR](fconr.md) - Convert to float with rounding
- [HCON](hcon.md) - Convert to halfword (truncation, no rounding)
