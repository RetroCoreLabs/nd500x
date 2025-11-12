# PMPYR - Packed Multiply Rounded

## Overview

**Mnemonic:** `pmpyr`
**Function:** Multiply packed BCD numbers with rounding
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `PMPYR <a>, <b>, <c>`

---

## Description

Multiplies two packed BCD numbers and stores the rounded result. Identical to PMPY except the product is rounded before storing according to the destination's scale factor. Essential for financial calculations requiring proper rounding to avoid cumulative errors.

**Operation:**
```
a * b → c (with automatic scaling and rounding)
- Multiply BCD operands
- Adjust scale to match destination descriptor
- Round to destination precision (banker's rounding)
- Store result with sign handling
```

**Key Characteristics:**
- Three-operand BCD multiplication with rounding
- Banker's rounding (round-to-even) for financial accuracy
- Automatic scale factor adjustment
- Descriptor-based precision control
- Prevents cumulative rounding errors in financial calculations
- Sign handling via descriptor bit 26
- Common in currency conversion, tax, discount, and interest calculations

**Operands:** 3
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE91 | PMPYR |

---

## Operands

All operands: Packed BCD format with descriptor-based scaling

---

## Trap Conditions
- **BCD overflow (BO)**, **Invalid operation (IVO)**

---

## Data Status Bits
- **Z,C,BO,K**: Set based on result

---

## Examples

### Example 1: Price calculation with rounding
```assembly
PMPYR B.PRICE, DISCOUNT, B.NET
```

### Example 2: Tax with rounding
```assembly
PMPYR AMOUNT, TAX_RATE, TAX
```

### Example 3: Discount calculation
```assembly
PMPYR SUBTOTAL, 0.15, DISCOUNT
```

### Example 4: Commission
```assembly
PMPYR SALES, COMM_RATE, COMMISSION
```

### Example 5: Interest calculation
```assembly
PMPYR PRINCIPAL, RATE, INTEREST
```

### Example 6: Currency with precision
```assembly
PMPYR FOREIGN_AMT, EXCHANGE_RATE, LOCAL_AMT
```

### Example 7: Batch processing
```assembly
LOOP:
        PMPYR ITEMS(W1).QTY, ITEMS(W1).PRICE, ITEMS(W1).TOTAL
        W1 INC
        W1 COMP COUNT
        IF<GO LOOP
```

---

## Reference Manual
**Section:** §17.4

---

## See Also
- [PMPY](pmpy.md) - Packed multiply (no rounding)
