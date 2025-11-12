# PSUBR - Packed Subtract with Rounding

## Overview

**Mnemonic:** `psubr`
**Function:** Subtract packed BCD with rounding
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `PSUBR <a>, <b>, <c>`

---

## Description

Subtracts packed BCD operand `<b>` from `<a>` and stores the rounded result in `<c>`. This is identical to PSUB except the result is rounded according to the destination's scaling factor before storing. Essential for financial calculations where rounding rules must be strictly followed.

**Operation:**
```
<a> - <b> → <c> (rounded)
Result scaled according to <c> scaling factor
difference = 0 → Z flag
difference.signbit → S flag
overflow → BO flag
```

**Key Characteristics:**
- Three-operand packed BCD subtraction
- Automatic rounding based on destination scaling
- Supports different scaling factors for each operand
- Sets BCD overflow flag if result doesn't fit
- Proper decimal rounding (banker's rounding or similar)

**Common Use Cases:**
- Financial calculations with required rounding
- Currency operations (dollars and cents)
- Tax calculations with rounding requirements
- Discount and markdown calculations
- Invoice and billing computations
- Accounting ledger operations

**Operands:** 3 (minuend, subtrahend, difference)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE86 | PSUBR |

---

## Operands

### Operand 1 (Minuend - a)

**Type:** Packed BCD
**Access:** Read

The value to subtract from (a - b).

### Operand 2 (Subtrahend - b)

**Type:** Packed BCD
**Access:** Read

The value to subtract (a - b).

### Operand 3 (Difference - c)

**Type:** Packed BCD
**Access:** Write

Receives the rounded result of a - b. Scaling factor determines rounding precision.

**Supported modes (all operands):**
- **LOCAL** - Local variable (B.variable)
- **RECORD** - Record field (R.field)
- **PRE_INDEXED** - Indexed array access
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **BCD overflow (BO):** Result doesn't fit in destination
- **Invalid operation (IVO):** Malformed BCD data

---

## Data Status Bits

- **Z (Zero):** Set if difference = 0, cleared otherwise
- **S (Sign):** Set if difference is negative
- **BO (BCD Overflow):** Set if overflow occurred
- **K (Overflow Flag):** Set if BO or IVO is set
- **C (Carry):** Unaffected
- **V (Overflow):** Unaffected

---

## Examples

### Example 1: Discount with rounding

```assembly
        % Subtract discount from total, with rounding
        PSUBR TOTAL, B.DISCOUNT, TOTAL
```

**Explanation:** Apply discount to total, rounding result per total's scaling factor. From manual example.

### Example 2: Balance update after withdrawal

```assembly
        % Deduct withdrawal from account balance
        PSUBR BALANCE, WITHDRAWAL, BALANCE
```

**Explanation:** Bank account withdrawal with proper rounding to cents.

### Example 3: Tax calculation

```assembly
        % Calculate net after tax
        PSUBR GROSS, TAX, NET
```

**Explanation:** Subtract tax from gross amount, rounding net value appropriately.

### Example 4: Price markdown calculation

```assembly
        % Calculate sale price after markdown
        PSUBR ORIG_PRICE, MARKDOWN, SALE
```

**Explanation:** Retail markdown: subtract markdown amount from original price.

### Example 5: Inventory depletion

```assembly
        % Update quantity on hand after sale
        PSUBR QTY_ON_HAND, QTY_SOLD, QTY_REM
```

**Explanation:** Inventory tracking: subtract sold quantity from stock.

### Example 6: Payment processing

```assembly
        % Update amount due after payment
        PSUBR AMOUNT_DUE, PAYMENT, BALANCE
```

**Explanation:** Invoice payment: subtract payment from amount due, showing remaining balance.

### Example 7: Budget expense tracking

```assembly
        % Calculate remaining budget
        PSUBR BUDGET, EXPENSES, REMAINING
```

**Explanation:** Budget management: subtract expenses from budget allocation.

---

## Performance Notes

- **Execution:** 20-30 cycles
  - BCD subtraction: 15-25 cycles
  - Rounding: +3-5 cycles
  - Scaling adjustment: +2-3 cycles
- **Implementation:** Software BCD arithmetic with rounding
- **Precision:** Rounding follows destination scaling factor

**Usage recommendations:**
- Use for all financial calculations requiring rounding
- Ensure proper scaling factors on destination
- Check BO flag for overflow detection
- Prefer PSUBR over PSUB for money calculations
- Pair with PADDR for complete financial arithmetic

**Rounding behavior:**
- Rounding applied based on destination scaling factor
- Typically banker's rounding (round half to even)
- Ensures consistent decimal precision
- Required for regulatory compliance in financial software

**Comparison with related instructions:**
- `PSUBR` vs `PSUB`: PSUBR rounds, PSUB truncates
- `PSUBR` vs `SUB`: PSUBR is BCD decimal, SUB is binary
- Use PSUBR for financial/accounting (exact decimal)
- Use SUB for general integer arithmetic

---

## Reference Manual

**Section:** §17.3
**Title:** Packed BCD Subtraction with Rounding

---

## See Also

- [PSUB](psub.md) - Packed BCD subtraction without rounding
- [PADDR](paddr.md) - Packed BCD addition with rounding
- [PADD](padd.md) - Packed BCD addition without rounding
- [PMUL](pmul.md) - Packed BCD multiplication
- [PDIV](pdiv.md) - Packed BCD division
- [WPCONV](wpconv.md) - Convert binary to packed BCD
- [PWCONV](pwconv.md) - Convert packed BCD to binary
