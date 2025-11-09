# DCONV - Convert to Double

## Overview

**Mnemonic:** `dconv`
**Function:** Convert source operand to 64-bit IEEE 754 double
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `t1 DCONV <source/r/t1>,<dest/w/D>`

---

## Description

Converts a source operand of any supported type (BI, BY, H, W, F) to 64-bit IEEE 754 double-precision floating-point format and stores the result in the destination.

The conversion handles:
- **Integer to double:** Exact conversion (doubles can represent all 32-bit integers exactly)
- **Float to double:** Exact widening conversion without loss
- **Identity:** Double to double is a simple copy

This is the safest numeric conversion as double-precision can represent:
- All 32-bit integer values exactly (2⁵³ > 2³²)
- All single-precision float values exactly
- No precision loss in any conversion to double

This instruction is essential for:
- High-precision arithmetic
- Upcasting from single precision
- Lossless integer conversion
- Scientific computing

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per source type)

| Variant | Opcode | Source Type | Dest Type | Assembly Notation |
|---------|--------|-------------|-----------|-------------------|
| 1/5 | 0xFD48 | BI | D | BI DCONV |
| 2/5 | 0xFD4D | BY | D | BY DCONV |
| 3/5 | 0xFD52 | H | D | H DCONV |
| 4/5 | 0xFD57 | W | D | W DCONV |
| 5/5 | 0xFD5C | F | D | F DCONV |

---

## Operands

### Operand 1 (Source)

Source value to convert.

**Type:** BI, BY, H, W, or F (determined by instruction prefix)
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Destination)

Destination for converted 64-bit double value.

**Type:** D (double)
**Access:** Write

**Supported modes:** LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation

**Note:** DCONV never traps on IOV as all source types fit in double range.

---

## Data Status Bits

- **Z (Zero):** Set if result = 0.0
- **S (Sign):** Set if result is negative
- **O (Overflow):** Never set (no overflow possible)
- **C (Carry):** Not affected

---

## Examples

### Example 1: Integer to double for precision

```assembly
        % Convert word to double for high-precision calculation
        W DCONV I1, B.DBL_VAL
        D MUL B.DBL_VAL, 1.23456789, B.RESULT
```

### Example 2: Upcast float to double

```assembly
        % Widen float to double before calculation
        F DCONV B.SINGLE_VAL, B.DOUBLE_VAL
```

### Example 3: Exact integer representation

```assembly
        % Store exact integer value in double
        W DCONV B.INTEGER, B.DBL_INTEGER
        % No precision loss - all 32-bit ints fit exactly
```

---

## Performance Notes

- **Typical cycles:** 7-12 cycles
- **Integer to double:** 7-9 cycles
- **Float to double:** 10-12 cycles (bit reorganization)

---

## Reference Manual

**Section:** §15.2
**Title:** Data type conversion

---

## See Also

- [BYCONV](byconv.md) - Convert to byte
- [HCONV](hconv.md) - Convert to halfword
- [WCONV](wconv.md) - Convert to word
- [FCONV](fconv.md) - Convert to float
- [BICONV](biconv.md) - Convert to bit
