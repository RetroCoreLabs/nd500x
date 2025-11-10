# PUPACKR - Convert Packed BCD to ASCII with Rounding

## Overview

**Mnemonic:** `pupackr`
**Function:** Unpack BCD to ASCII with rounding
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `PUPACKR <source>, <dest>`

---

## Description

Converts a packed BCD value to ASCII string format with rounding applied before conversion. This is identical to PUPACK except the value is rounded according to the destination's precision before being unpacked into ASCII digits. Essential for display and output formatting of financial data.

**Operation:**
```
<source> (packed BCD) → <dest> (ASCII string) [rounded]
value after rounding = 0 → Z flag
value.signbit → S flag
overflow → BO flag
```

**Key Characteristics:**
- Rounds packed BCD before converting to ASCII
- Sign representation determined by destination descriptor SGN field
- Destination extended with leading ASCII zeros if needed
- Parity bit = 0 for all ASCII digits
- Handles different scaling factors

**Common Use Cases:**
- Display formatting for financial values
- Report generation with rounded amounts
- Screen output of currency values
- Converting calculations to printable text
- Database export to text format
- Invoice and receipt printing

**Operands:** 2 (source packed BCD, destination ASCII)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE93 | PUPACKR |

---

## Operands

### Operand 1 (Source)

**Type:** Packed BCD
**Access:** Read

The packed BCD value to convert.

### Operand 2 (Destination)

**Type:** ASCII string
**Access:** Write

Receives the ASCII representation of the rounded value. SGN field in descriptor controls sign representation.

**Supported modes (both operands):**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **PRE_INDEXED** - Indexed array access
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **BCD overflow (BO):** Value doesn't fit in destination
- **Invalid operation (IVO):** Malformed BCD data

---

## Data Status Bits

- **Z (Zero):** Set if rounded value = 0
- **S (Sign):** Set if value is negative
- **BO (BCD Overflow):** Set if overflow occurred
- **K (Overflow Flag):** Set if BO or IVO
- **C, V:** Unaffected

---

## Examples

### Example 1: Basic rounded unpack

```assembly
        % Convert packed to ASCII with rounding
        PUPACKR VAR1, IFIELD
```

**Explanation:** Basic conversion with rounding for display.

### Example 2: Display with rounding

```assembly
        % Prepare precise value for display
        PUPACKR PRECISE_VAL, DISPLAY
```

**Explanation:** Round calculated value before displaying.

### Example 3: Currency formatting

```assembly
        % Convert currency amount with rounding
        PUPACKR AMOUNT, TEXT_FIELD
```

**Explanation:** Financial display with proper rounding to cents.

### Example 4: Calculation result output

```assembly
        % Output calculation result
        PUPACKR CALC, OUTPUT
```

**Explanation:** Convert calculation to ASCII for output.

### Example 5: Balance display

```assembly
        % Display account balance rounded
        PUPACKR BALANCE, SCREEN
```

**Explanation:** Bank balance display with appropriate rounding.

### Example 6: Report generation

```assembly
        % Format value for report with rounding
        PUPACKR TOTAL, REPORT
```

**Explanation:** Financial report formatting with standardized rounding.

### Example 7: Batch array conversion

```assembly
        % Convert array of packed values to ASCII
        W1 := 0
CONV_LOOP:
        PUPACKR VALUES(W1), TEXTS(W1)
        W1 INC
        W1 COMP ARRAY_SIZE
        IF<GO CONV_LOOP
```

**Explanation:** Batch conversion of packed BCD array to ASCII strings for export.

---

## Performance Notes

- **Execution:** 25-35 cycles
  - Rounding: 5-10 cycles
  - BCD to ASCII conversion: 15-20 cycles
  - String formatting: +5 cycles
- **Implementation:** Software conversion with rounding logic
- **String length:** Depends on destination precision

**Usage recommendations:**
- Use for all display/output formatting
- Ensure destination has sufficient length
- Check BO flag for overflow
- Prefer PUPACKR over PUPACK for financial display
- Pair with WPCONV for full BCD workflow

**Sign representation:**
- Controlled by SGN field in destination descriptor
- Options: leading sign, trailing sign, unsigned
- Consistent with ASCII text formatting standards

**Comparison with related instructions:**
- `PUPACKR` vs `PUPACK`: PUPACKR rounds, PUPACK truncates
- `PUPACKR` vs `PWCONV`: PUPACKR → ASCII, PWCONV → binary
- Use PUPACKR for text output
- Use PWCONV for binary calculations

---

## Reference Manual

**Section:** §17.8
**Title:** Convert packed to ASCII

---

## See Also

- [PUPACK](pupack.md) - Convert packed to ASCII without rounding
- [WPCONV](wpconv.md) - Convert binary to packed BCD
- [PWCONV](pwconv.md) - Convert packed BCD to binary
- [PSUBR](psubr.md) - Packed subtraction with rounding
- [PADDR](paddr.md) - Packed addition with rounding
