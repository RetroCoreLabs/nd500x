# PADD - Packed Add

## Overview

**Mnemonic:** `padd`
**Function:** Add packed BCD numbers without rounding
**Class:** ARITHMETIC (Packed BCD)
**Privilege:** user

**Format:** `PADD <a=/r/BCD=>, <b=/r/BCD=>, <c=/w/BCD=>`

---

## Description

Adds two packed Binary Coded Decimal (BCD) numbers with automatic scaling but without rounding. This instruction provides exact decimal addition for applications that require full precision or implement custom rounding strategies.

PADD performs the same decimal addition as PADDR but does not apply rounding to the final result. When the sum has more decimal places than the destination can hold, the excess digits are truncated (not rounded). This behavior is useful when maximum precision must be preserved or when rounding will be applied separately as part of a larger computation.

Like all BCD arithmetic instructions, PADD automatically aligns operands with different scale factors (decimal point positions) before performing the addition. The result is then scaled to match the destination's scale factor through truncation.

BCD arithmetic guarantees exact decimal representation, eliminating floating-point rounding errors that can accumulate in financial and scientific calculations requiring decimal precision.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFEB0 | PADD |

---

## Operands

### Operand 1: `<a=/r/BCD=>`

First addend (BCD number).

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the first BCD number to add. Includes scale factor for decimal point position.

### Operand 2: `<b=/r/BCD=>`

Second addend (BCD number).

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the second BCD number to add. Scale factor can differ from operand 1.

### Operand 3: `<c=/w/BCD=>`

Sum destination (BCD number).

**Role:** Destination
**Access:** Write
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies where the sum is stored. Result is scaled (truncated, not rounded) to match this operand's scale factor.

---

## Trap Conditions

- **BCD Overflow (BO)**: Result magnitude exceeds destination capacity
- **Invalid Operation (IVO)**: Invalid BCD digit encoding detected
- **Addressing traps**: Standard memory access violations

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| Z | Zero | Set if sum equals zero |
| S | Sign | Set to sign bit of sum |
| BO | BCD Overflow | Set if result overflows destination |
| K | Carry | Set if BO or IVO occurred |

---

## Examples

### Example 1: Add local variables

```assembly
% Add PRICE and TAX to form TOTAL
        PADD B.PRICE, B.TAX, TOTAL
        % TOTAL = PRICE + TAX (truncated to TOTAL's scale)
```

### Example 2: Intermediate calculation preserving precision

```assembly
% Multi-step calculation without intermediate rounding
SUBTOTAL1: % 123.456
SUBTOTAL2: % 78.789
INTERMEDIATE: % (6 decimal places - preserve precision)

        PADD SUBTOTAL1, SUBTOTAL2, INTERMEDIATE
        % INTERMEDIATE = 202.245 (full precision)
        % Apply rounding only at final step
```

### Example 3: Accumulation loop

```assembly
% Sum array of values
TOTAL:  0
ARRAY:  % Array of BCD values

LOOP:   PADD TOTAL, ARRAY(I1), TOTAL
        W ADD I1, ITEM_SIZE
        W COMP I1, ARRAY_END
        IF<GO LOOP
```

### Example 4: Multi-precision addition

```assembly
% Add values with different decimal scales
VALUE1: % 99.9999 (4 decimals)
VALUE2: % 0.00012 (5 decimals)
RESULT: % (4 decimals)

        PADD VALUE1, VALUE2, RESULT
        % RESULT = 100.0000 (truncated from 100.00002)
```

### Example 5: Record field addition

```assembly
INVOICE:
        .AMOUNT:   (BCD field)
        .DISCOUNT: (BCD field)
        .NET:      (BCD field)

        PADD R.AMOUNT, R.DISCOUNT, R.NET
```

---

## Performance Notes

- **No Rounding**: Result is truncated, not rounded
- **Decimal Alignment**: Automatically aligns different scale factors
- **Truncation**: Excess precision beyond destination scale is lost
- **BCD Format**: Packed format with 2 digits per byte
- **Precision Control**: Use when custom rounding needed or maximum precision required
- **Typical Use**: Intermediate calculations, accumulation, custom rounding strategies

---

## Reference Manual

**Section:** §17.2
**Title:** Packed add

---

## See Also

- [PADDR](paddr.md) - Packed add with rounding
- [PSUB](psub.md) - Packed subtract without rounding
- [PSUBR](psubr.md) - Packed subtract with rounding
