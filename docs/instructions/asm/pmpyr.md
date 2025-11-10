# PMPYR - Packed Multiply Rounded

## Overview

**Mnemonic:** `pmpyr`
**Function:** Multiply packed BCD numbers with rounding
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `PMPYR <a>, <b>, <c>`

---

## Description

Multiplies two packed BCD numbers and stores the rounded result. Identical to PMPY except the product is rounded before storing according to the destination's scale factor. Used for financial calculations requiring rounding.

**Operation:**
```
a * b → c (with scaling and rounding)
```

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
