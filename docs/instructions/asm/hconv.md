# HCONV - Convert to Halfword

## Overview

**Mnemonic:** `hconv`
**Function:** Convert source operand to 16-bit halfword
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `t1 HCONV <source/r/t1>,<dest/w/H>`

---

## Description

Converts a source operand of any supported type (BI, BY, W, F, D) to 16-bit halfword (integer) format and stores the result in the destination.

The conversion handles:
- **Smaller integers to halfword:** Sign extension (BI, BY → H)
- **Larger integers to halfword:** Truncation with overflow check (W → H)
- **Float/double to halfword:** Rounding toward zero with range check
- **Identity:** Halfword to halfword is a simple copy

If the source value is outside the representable range of a 16-bit signed integer (-32768 to 32767), an illegal operand value (IOV) trap occurs.

This instruction is essential for:
- Packing data into 16-bit fields
- Converting to halfword array indices
- Interface with 16-bit data structures
- Type casting in high-level languages

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per source type)

| Variant | Opcode | Source Type | Dest Type | Assembly Notation |
|---------|--------|-------------|-----------|-------------------|
| 1/5 | 0xFD45 | BI | H | BI HCONV |
| 2/5 | 0xFD4A | BY | H | BY HCONV |
| 3/5 | 0xFD55 | W | H | W HCONV |
| 4/5 | 0xFD5A | F | H | F HCONV |
| 5/5 | 0xFD5F | D | H | D HCONV |

---

## Operands

### Operand 1 (Source)

Source value to convert.

**Type:** BI, BY, W, F, or D (determined by instruction prefix)
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Destination)

Destination for converted 16-bit halfword value.

**Type:** H (halfword)
**Access:** Write

**Supported modes:** LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Illegal operand value (IOV):** Source value outside halfword range (-32768 to 32767)

---

## Data Status Bits

- **Z (Zero):** Set if result = 0
- **S (Sign):** Set to sign bit of result
- **O (Overflow):** Set on conversion overflow
- **C (Carry):** Not affected

---

## Examples

### Example 1: Convert byte to halfword

```assembly
        % Convert signed byte to halfword with sign extension
        BY HCONV B.BYTE_VAL, B.HALF_RESULT
```

### Example 2: Convert word to halfword with check

```assembly
        % Convert word to halfword, trap on overflow
        W HCONV I1, B.SHORT_VAL
        IF-KGO CONV_OK
        % IOV trap - value too large for halfword
        CALL OVERFLOW_HANDLER
CONV_OK:
```

### Example 3: Float to halfword index

```assembly
        % Convert float to halfword array index
        F HCONV B.FLOAT_INDEX, I1
        H := B.ARRAY(I1)
```

### Example 4: Pack data into halfword

```assembly
        % Store 16-bit status code
        W HCONV I1, R.STATUS_CODE
```

---

## Performance Notes

- **Typical cycles:** 5-10 cycles
- **Integer conversions (BI/BY/W):** 5-7 cycles
- **Float conversions (F/D):** 8-10 cycles

---

## Reference Manual

**Section:** §15.2
**Title:** Data type conversion

---

## See Also

- [BYCONV](byconv.md) - Convert to byte
- [WCONV](wconv.md) - Convert to word
- [FCONV](fconv.md) - Convert to float
- [DCONV](dconv.md) - Convert to double
- [BICONV](biconv.md) - Convert to bit
