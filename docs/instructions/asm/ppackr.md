# PPACKR - Convert ASCII to Packed Rounded

## Overview

**Mnemonic:** `ppackr`
**Function:** Convert ASCII to packed BCD with rounding
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `PPACKR <source/ASCII>, <dest/BCD>`

---

## Description

Converts ASCII-coded decimal to packed BCD format with rounding before storage. Identical to PPACK but rounds the value according to the destination descriptor's scale. Sign representation is controlled by bit 26 of the destination descriptor: if set, unsigned (1111); otherwise, preserves source sign.

**Operation:**
```
<source> → <dest> (rounded)
```

**Key Characteristics:**
- Converts ASCII decimal to packed BCD
- Rounds before storing in destination
- Sign control via destination descriptor bit 26
- Destination extended with leading zeros if needed
- BCD overflow if result exceeds destination size

**Common Use Cases:**
- Financial data entry with rounding
- Currency conversions requiring precision
- Form input processing with rounding rules
- Database field population with rounding
- Report totals requiring rounded values

**Operands:** 2 (ASCII source, BCD destination)
**Variants:** 1 opcode

---

## Examples

### Example 1: Financial form processing

```assembly
        % Convert user-entered amount with rounding
        PPACKR FORM.AMOUNT, RECORD.TOTAL
```

**Explanation:** Round currency input to destination precision.

### Example 2: Currency exchange with rounding

```assembly
        % Convert and round exchange rate result
        PPACKR CALCULATED_AMT, ROUNDED_RESULT
```

**Explanation:** Round calculated currency to two decimal places.

### Example 3: Invoice total rounding

```assembly
        % Round invoice subtotal to cents
        PPACKR SUBTOTAL_TEXT, INVOICE.AMOUNT
```

**Explanation:** Convert ASCII subtotal with banker's rounding.

### Example 4: Database field update

```assembly
        % Update BCD field with rounded value
        PPACKR USER_ENTRY, DB_FIELD
```

**Explanation:** Store user input as rounded BCD in database.

### Example 5: Report generation

```assembly
        % Round report totals for display
        PPACKR CALC_TOTAL, REPORT_LINE
```

**Explanation:** Format financial report with proper rounding.

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **BCD overflow (BO)**: Result exceeds destination size
- **Invalid operation (IVO)**: Invalid ASCII digit encoding

---

## Data Status Bits

- **Z (Zero)**: Value equals zero after rounding
- **S (Sign)**: Value sign bit
- **BO (BCD Overflow)**: Destination field too small
- **K (Error)**: BO or IVO occurred

---

## Reference Manual

**Section:** §17.7
**Title:** Convert ASCII to packed

---

## See Also

- [PPACK](ppack.md) - Convert ASCII to packed without rounding
- [PUPACK](pupack.md) - Convert packed to ASCII
- [PUPACKR](pupackr.md) - Convert packed to ASCII with rounding
