# PSUB - Packed Subtract

## Overview

**Mnemonic:** `psub`
**Function:** Subtract packed BCD numbers
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `PSUB <a>, <b>, <c>`

---

## Description

Subtracts packed BCD operand b from a, storing the result in c with automatic scaling adjustment. The operation respects the scale factors defined in each operand's descriptor and handles sign representation according to the destination descriptor (bit 26).

**Operation:**
```
<a> - <b> → <c> (with scale adjustment)
```

**Key Characteristics:**
- Three-operand BCD subtraction
- Automatic scale factor adjustment
- Descriptor-based precision control
- Sign handling via destination descriptor bit 26
- No rounding (use PSUBR for rounding)
- Result zero-extended if necessary

**Common Use Cases:**
- Financial calculations (discounts, balances)
- Inventory management (stock deductions)
- Tax and fee calculations
- Payment processing
- Budget tracking
- Account reconciliation

**Operands:** 3 (minuend, subtrahend, difference)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFEB1 | PSUB |

---

## Trap Conditions
- **BCD overflow (BO)**, **Invalid operation (IVO)**

---

## Data Status Bits
- **Z,S,BO,K**: Set based on result

---

## Examples

### Example 1: Discount calculation
```assembly
PSUB TOTAL, B.DISCOUNT, TOTAL
```

### Example 2: Balance update
```assembly
PSUB BALANCE, WITHDRAWAL, NEW_BALANCE
```

### Example 3: Tax deduction
```assembly
PSUB GROSS, TAX, NET
```

### Example 4: Price reduction
```assembly
PSUB ORIG_PRICE, MARKDOWN, SALE_PRICE
```

### Example 5: Inventory
```assembly
PSUB QTY_ON_HAND, QTY_SOLD, QTY_REMAINING
```

### Example 6: Payment processing
```assembly
PSUB AMOUNT_DUE, PAYMENT, BALANCE
```

### Example 7: Expense tracking
```assembly
LOOP:
        PSUB BUDGET, EXPENSES(W1), REMAINING
        W1 INC
        W1 COMP EXP_COUNT
        IF<GO LOOP
```

---

## Reference Manual
**Section:** §17.3

---

## See Also
- [PSUBR](psubr.md) - Subtract with rounding
- [PADD](padd.md) - Packed add
