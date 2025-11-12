# WCONV - Convert to Word

## Overview

**Mnemonic:** `wconv`
**Function:** Convert source operand to 32-bit word
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `t1 WCONV <source/r/t1>,<dest/w/W>`

---

## Description

Converts a source operand of any supported type (BI, BY, H, F, D) to 32-bit word (integer) format and stores the result in the destination.

**Operation:**
```
<dest> = (word)<source>
```

**Key Characteristics:**
- Universal type converter to 32-bit signed integer
- 5 source types supported (BI, BY, H, F, D)
- Integer conversions use sign extension
- Float conversions use truncation (round toward zero)
- IOV trap when float/double out of range (-2³¹ to 2³¹-1)
- Essential for type casting in high-level languages
- Integer conversions fast (5-7 cycles), float slower (8-10 cycles)
- Interface between integer and floating-point code
- Preserves sign for negative values

The conversion handles:
- **Smaller integers to word:** Sign extension (BI, BY, H → W)
- **Float/double to word:** Rounding toward zero (truncation)
- **Identity:** Word to word is a simple copy

Float-to-integer conversions use truncation (round toward zero). If the floating-point value is outside the representable range of a 32-bit signed integer (-2³¹ to 2³¹-1), an illegal operand value (IOV) trap occurs.

This instruction is essential for:
- Type casting in high-level languages
- Converting floating-point calculations to integers
- Preparing values for integer arithmetic
- Interface between integer and floating-point code

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per source type)

| Variant | Opcode | Source Type | Dest Type | Assembly Notation |
|---------|--------|-------------|-----------|-------------------|
| 1/5 | 0xFD46 | BI | W | BI WCONV |
| 2/5 | 0xFD4B | BY | W | BY WCONV |
| 3/5 | 0xFD50 | H | W | H WCONV |
| 4/5 | 0xFD5B | F | W | F WCONV |
| 5/5 | 0xFD60 | D | W | D WCONV |

---

## Operands

### Operand 1 (Source)

Source value to convert.

**Type:** BI, BY, H, F, or D (determined by instruction prefix)
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Destination)

Destination for converted 32-bit word value.

**Type:** W (word)
**Access:** Write

**Supported modes:** LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Illegal operand value (IOV):** Float/double value outside word range

---

## Data Status Bits

- **Z (Zero):** Set if result = 0
- **S (Sign):** Set to sign bit of result
- **O (Overflow):** Set on conversion overflow
- **C (Carry):** Not affected

---

## Examples

### Example 1: Convert byte to word

```assembly
        % Convert signed byte to word with sign extension
        BY WCONV B.BYTE_VAL, I1
        % I1 now contains sign-extended word value
```

### Example 2: Convert float to word

```assembly
        % Convert floating-point to integer (truncate)
        F WCONV B.FLOAT_VAL, I2
        % I2 contains truncated integer value
```

### Example 3: Array index calculation

```assembly
        % Convert halfword array index to word for calculation
        H WCONV B.INDEX, I1
        W MUL I1, 4, I1       % Scale by element size
```

### Example 4: Float to integer with range check

```assembly
        % Convert float with overflow check
        F WCONV B.FVAL, I1
        IF-KGO CONV_OK
        % IOV trap occurred - value out of range
        CALL OVERFLOW_HANDLER
CONV_OK:
        % I1 contains valid integer
```

### Example 5: Type conversion in arithmetic

```assembly
        % Calculate: word_result = (word)byte_a + (word)byte_b
        BY WCONV B.BYTE_A, I1
        BY WCONV B.BYTE_B, I2
        W ADD I1, I2, B.RESULT
```

---

## Performance Notes

- **Typical cycles:** 5-10 cycles
- **Integer conversions (BI/BY/H):** 5-7 cycles (sign extension)
- **Float conversions (F/D):** 8-10 cycles (IEEE 754 decoding + rounding)

**Note:** Float-to-integer conversions are slower due to IEEE 754 format decoding and rounding logic.

---

## Reference Manual

**Section:** §15.2
**Title:** Data type conversion

---

## See Also

- [BYCONV](byconv.md) - Convert to byte
- [HCONV](hconv.md) - Convert to halfword
- [FCONV](fconv.md) - Convert to float
- [DCONV](dconv.md) - Convert to double
- [BICONV](biconv.md) - Convert to bit
