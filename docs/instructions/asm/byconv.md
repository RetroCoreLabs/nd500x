# BYCONV - Convert to Byte

## Overview

**Mnemonic:** `byconv`
**Function:** Convert source operand to 8-bit byte
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `t1 BYCONV <source/r/t1>,<dest/w/BY>`

---

## Description

Converts a source operand of any supported type (BI, H, W, F, D) to 8-bit byte (integer) format and stores the result in the destination.

The conversion handles:
- **Bit to byte:** Zero extension (BI → BY)
- **Larger integers to byte:** Truncation with overflow check (H/W → BY)
- **Float/double to byte:** Rounding toward zero with range check
- **Identity:** Byte to byte is a simple copy

If the source value is outside the representable range of an 8-bit signed integer (-128 to 127), an illegal operand value (IOV) trap occurs.

This instruction is essential for:
- Packing data into 8-bit fields
- Character/string processing
- Interface with 8-bit data structures
- Type casting in high-level languages

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per source type)

| Variant | Opcode | Source Type | Dest Type | Assembly Notation |
|---------|--------|-------------|-----------|-------------------|
| 1/5 | 0xFD44 | BI | BY | BI BYCONV |
| 2/5 | 0xFD4F | H | BY | H BYCONV |
| 3/5 | 0xFD54 | W | BY | W BYCONV |
| 4/5 | 0xFD59 | F | BY | F BYCONV |
| 5/5 | 0xFD5E | D | BY | D BYCONV |

---

## Operands

### Operand 1 (Source)

Source value to convert.

**Type:** BI, H, W, F, or D (determined by instruction prefix)
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Destination)

Destination for converted 8-bit byte value.

**Type:** BY (byte)
**Access:** Write

**Supported modes:** LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Illegal operand value (IOV):** Source value outside byte range (-128 to 127)

---

## Data Status Bits

- **Z (Zero):** Set if result = 0
- **S (Sign):** Set to sign bit of result
- **O (Overflow):** Set on conversion overflow
- **C (Carry):** Not affected

---

## Examples

### Example 1: Convert word to byte

```assembly
        % Convert word to byte, check for overflow
        W BYCONV I1, B.BYTE_VAL
        IF-KGO CONV_OK
        % Value too large for byte
        CALL OVERFLOW_HANDLER
CONV_OK:
```

### Example 2: Float to byte conversion

```assembly
        % Convert float percentage to byte (0-100)
        F BYCONV B.PERCENT, R.BYTE_PERCENT
```

### Example 3: Store character

```assembly
        % Store ASCII character code
        W BYCONV I1, B.STRING(I2)
```

---

## Performance Notes

- **Typical cycles:** 5-10 cycles
- **Integer conversions:** 5-7 cycles
- **Float conversions:** 8-10 cycles

---

## Reference Manual

**Section:** §15.2
**Title:** Data type conversion

---

## See Also

- [HCONV](hconv.md) - Convert to halfword
- [WCONV](wconv.md) - Convert to word
- [FCONV](fconv.md) - Convert to float
- [DCONV](dconv.md) - Convert to double
- [BICONV](biconv.md) - Convert to bit
