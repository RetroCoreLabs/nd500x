# FCONV - Convert to Float

## Overview

**Mnemonic:** `fconv`
**Function:** Convert source operand to 32-bit IEEE 754 float
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `t1 FCONV <source/r/t1>,<dest/w/F>`

---

## Description

Converts a source operand of any supported type (BI, BY, H, W, D) to 32-bit IEEE 754 single-precision floating-point format and stores the result in the destination.

**Operation:**
```
<dest> = (float)<source>
```

**Key Characteristics:**
- Type converter to IEEE 754 single-precision float
- 5 source types supported (BI, BY, H, W, D)
- Exact conversion for integers ≤ 2²⁴
- Precision loss for large integers and double downcasts
- Rounding to nearest representable float value
- IOV trap when double outside float range
- Essential for mixed precision arithmetic
- 7-12 cycles (integer 7-9, double 10-12)
- Interface with 32-bit float APIs

The conversion handles:
- **Integer to float:** Exact conversion for values within float precision
- **Double to float:** Rounding to nearest representable float value
- **Identity:** Float to float is a simple copy

Precision loss may occur when converting:
- Integers larger than 2²⁴ (float mantissa has only 24 bits)
- Double values requiring more precision than single precision

An illegal operand value (IOV) trap occurs if the source double value is outside the representable float range.

This instruction is essential for:
- Mixed precision arithmetic
- Converting integers to floating-point
- Downcasting from double precision
- Interface with 32-bit float APIs

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per source type)

| Variant | Opcode | Source Type | Dest Type | Assembly Notation |
|---------|--------|-------------|-----------|-------------------|
| 1/5 | 0xFD47 | BI | F | BI FCONV |
| 2/5 | 0xFD4C | BY | F | BY FCONV |
| 3/5 | 0xFD51 | H | F | H FCONV |
| 4/5 | 0xFD56 | W | F | W FCONV |
| 5/5 | 0xFD61 | D | F | D FCONV |

---

## Operands

### Operand 1 (Source)

Source value to convert.

**Type:** BI, BY, H, W, or D (determined by instruction prefix)
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Destination)

Destination for converted 32-bit float value.

**Type:** F (float)
**Access:** Write

**Supported modes:** LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Illegal operand value (IOV):** Double value outside float range

---

## Data Status Bits

- **Z (Zero):** Set if result = 0.0
- **S (Sign):** Set if result is negative
- **O (Overflow):** Set on conversion overflow
- **C (Carry):** Not affected

---

## Examples

### Example 1: Convert integer to float

```assembly
        % Convert word integer to float for calculation
        W FCONV I1, B.FLOAT_RESULT
        F MUL B.FLOAT_RESULT, 3.14159, B.FINAL
```

### Example 2: Downcast double to float

```assembly
        % Convert double to single precision
        D FCONV B.DOUBLE_VAL, B.FLOAT_VAL
```

### Example 3: Mixed precision arithmetic

```assembly
        % Convert byte count to float for percentage calc
        BY FCONV B.COUNT, E1
        F DIV E1, B.TOTAL, E1
        F MUL E1, 100.0, B.PERCENTAGE
```

---

## Performance Notes

- **Typical cycles:** 7-12 cycles
- **Integer to float:** 7-9 cycles (conversion + normalization)
- **Double to float:** 10-12 cycles (rounding logic)

---

## Reference Manual

**Section:** §15.2
**Title:** Data type conversion

---

## See Also

- [BYCONV](byconv.md) - Convert to byte
- [HCONV](hconv.md) - Convert to halfword
- [WCONV](wconv.md) - Convert to word
- [DCONV](dconv.md) - Convert to double
- [BICONV](biconv.md) - Convert to bit
