# PADDR - Packed Add Rounded

## Overview

**Mnemonic:** `paddr`
**Function:** Add packed BCD numbers with rounding
**Class:** ARITHMETIC (Packed BCD)
**Privilege:** user

**Format:** `PADDR <a=/r/BCD=>, <b=/r/BCD=>, <c=/w/BCD=>`

---

## Description

Adds two packed Binary Coded Decimal (BCD) numbers with automatic rounding applied to the result. This instruction is essential for financial calculations, monetary arithmetic, and any application requiring exact decimal representation without floating-point rounding errors.

**Key Characteristics:**
- Packed BCD addition with commercial rounding
- Automatic round-half-up (banker's rounding)
- Exact decimal representation (no float errors)
- Automatic decimal point alignment
- Essential for currency and financial calculations
- Eliminates 0.1 + 0.2 ≠ 0.3 problems
- Supports different scale factors per operand
- Common in accounting and banking systems

PADDR performs decimal addition on BCD-encoded numbers, where each 4-bit nibble represents a decimal digit (0-9). The result is automatically scaled according to the destination operand's scale factor (decimal point position) and rounded to fit the destination precision.

The rounding behavior follows commercial rounding rules (round half up), making PADDR ideal for currency calculations where fractional cents must be properly rounded. The instruction handles numbers with different scale factors by automatically aligning decimal points before addition.

BCD arithmetic avoids the precision issues inherent in binary floating-point representation, ensuring that decimal values like 0.1 + 0.2 = 0.3 exactly, which is critical for financial applications.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFE85 | PADDR |

---

## Operands

### Operand 1: `<a=/r/BCD=>`

First addend (BCD number).

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the first BCD number to add. The operand includes a scale factor indicating the decimal point position.

### Operand 2: `<b=/r/BCD=>`

Second addend (BCD number).

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the second BCD number to add. Scale factors of operands 1 and 2 can differ - the instruction automatically aligns them.

### Operand 3: `<c=/w/BCD=>`

Sum destination (BCD number).

**Role:** Destination
**Access:** Write
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies where the sum is stored. The result is scaled and rounded according to this operand's scale factor.

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

### Example 1: Add currency values with rounding

```assembly
% Add price and tax to compute total with rounding
PRICE:  % $19.99 (scale factor 2)
TAX:    % $1.599 (scale factor 3)
TOTAL:  % Result (scale factor 2)

        PADDR PRICE, TAX, TOTAL
        % TOTAL = $21.59 (rounded from $21.589)
```

### Example 2: Financial calculation with local variables

```assembly
% Add invoice amount and shipping cost
        PADDR B.AMOUNT, B.SHIPPING, B.TOTAL
        % Automatic decimal point alignment and rounding
```

### Example 3: Accumulator pattern

```assembly
% Running total accumulation
ACCUMULATOR: 0.00
ITEM_PRICE:  12.95

        PADDR ACCUMULATOR, ITEM_PRICE, ACCUMULATOR
        % ACCUMULATOR += ITEM_PRICE with rounding
```

### Example 4: Multi-precision addition

```assembly
% Add two monetary amounts with different scales
AMOUNT1: % 123.456 (3 decimal places)
AMOUNT2: % 78.9 (1 decimal place)
RESULT:  % (2 decimal places)

        PADDR AMOUNT1, AMOUNT2, RESULT
        % RESULT = 202.36 (rounded from 202.356)
```

### Example 5: Interest calculation

```assembly
% Add principal and computed interest
PRINCIPAL: % $1000.00
INTEREST:  % $45.123
BALANCE:   % (2 decimal places)

        PADDR PRINCIPAL, INTEREST, BALANCE
        % BALANCE = $1045.12 (rounded)
```

---

## Performance Notes

- **Decimal Alignment**: Automatically aligns operands with different scale factors
- **Rounding**: Applies commercial rounding (round half up) to result
- **BCD Format**: Each byte stores 2 decimal digits (packed format)
- **Precision**: No floating-point rounding errors - exact decimal arithmetic
- **Typical Use**: Financial calculations, monetary arithmetic, accounting systems
- **Scale Factor**: Specifies number of digits after decimal point

---

## Reference Manual

**Section:** §17.2
**Title:** Packed add

---

## See Also

- [PADD](padd.md) - Packed add without rounding
- [PSUB](psub.md) - Packed subtract
- [PMPY](pmpy.md) - Packed multiply
