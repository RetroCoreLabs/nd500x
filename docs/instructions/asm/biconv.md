# BICONV - Convert to Bit

## Overview

**Mnemonic:** `biconv`
**Function:** Convert source operand to single bit (0 or 1)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `t1 BICONV <source/r/t1>,<dest/w/BI>`

---

## Description

Converts a source operand of any supported type (BY, H, W, F, D) to a single bit value (0 or 1) and stores the result in the destination.

The conversion follows C semantics:
- **Zero value → 0 (false)**
- **Non-zero value → 1 (true)**

For floating-point:
- 0.0 or -0.0 → 0
- Any other value (including NaN, infinity) → 1

This instruction is essential for:
- Boolean conversion
- Conditional flag generation
- Implementing C-style truth testing
- Converting values to boolean predicates

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per source type)

| Variant | Opcode | Source Type | Dest Type | Assembly Notation |
|---------|--------|-------------|-----------|-------------------|
| 1/5 | 0xFD49 | BY | BI | BY BICONV |
| 2/5 | 0xFD4E | H | BI | H BICONV |
| 3/5 | 0xFD53 | W | BI | W BICONV |
| 4/5 | 0xFD58 | F | BI | F BICONV |
| 5/5 | 0xFD5D | D | BI | D BICONV |

---

## Operands

### Operand 1 (Source)

Source value to convert to boolean.

**Type:** BY, H, W, F, or D (determined by instruction prefix)
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Destination)

Destination for converted bit value (0 or 1).

**Type:** BI (bit)
**Access:** Write

**Supported modes:** LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation

**Note:** BICONV never traps on IOV as any value maps to 0 or 1.

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared if result = 1
- **S (Sign):** Cleared (bit values are unsigned)
- **O (Overflow):** Cleared
- **C (Carry):** Not affected

---

## Examples

### Example 1: Boolean conversion

```assembly
        % Convert word to boolean
        W BICONV I1, B.BOOL_RESULT
        % B.BOOL_RESULT = 1 if I1 != 0, else 0
```

### Example 2: Float zero test

```assembly
        % Test if float is non-zero
        F BICONV B.FLOAT_VAL, B.IS_NONZERO
        W TEST B.IS_NONZERO
        IF<>GO VALUE_EXISTS
```

### Example 3: Conditional flag

```assembly
        % Set flag based on value
        W BICONV B.ERROR_CODE, B.HAS_ERROR
```

---

## Performance Notes

- **Typical cycles:** 5-8 cycles
- **Integer conversion:** 5-6 cycles (simple compare to zero)
- **Float conversion:** 7-8 cycles (check exponent/mantissa)

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
- [DCONV](dconv.md) - Convert to double
