# PUPACK - Convert Packed to ASCII

## Overview

**Mnemonic:** `pupack`
**Function:** Convert packed BCD to ASCII decimal
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `PUPACK <source/BCD>, <dest/ASCII>`

---

## Description

Converts packed decimal (BCD) format to ASCII decimal format for display and output operations. The destination string is extended with leading ASCII zeros if necessary. Parity bits for all digits are set to zero. Sign representation is determined by the SGN field in the destination descriptor.

**Operation:**
```
<source> → <dest> (unpacked)
```

**Key Characteristics:**
- Converts packed BCD to ASCII digits
- Destination padded with leading zeros if needed
- Parity bits cleared on all digits
- Sign representation controlled by dest descriptor
- No rounding (use PUPACKR for rounding)
- Descriptor-based field sizing

**Common Use Cases:**
- Financial report generation
- Display formatting for user interfaces
- Data export to text formats
- Screen/printer output preparation
- Debugging BCD values

**Operands:** 2 (BCD source, ASCII destination)
**Variants:** 1 opcode

---

## Examples

### Example 1: Basic conversion

```assembly
        % Convert BCD value to ASCII for display
        PUPACK VAR1, IFIELD
```

**Explanation:** Unpack BCD variable to ASCII output field.

### Example 2: Financial display

```assembly
        % Display currency amount
        PUPACK AMOUNT, DISPLAY_FIELD
        % DISPLAY_FIELD now contains ASCII digits
```

**Explanation:** Convert BCD currency value for screen display.

### Example 3: Report generation

```assembly
        % Generate report line with totals
        PUPACK SUBTOTAL, LINE_FIELD1
        PUPACK TAX, LINE_FIELD2
        PUPACK TOTAL, LINE_FIELD3
```

**Explanation:** Format multiple BCD values for printed report.

### Example 4: Array batch conversion

```assembly
        % Convert array of BCD records
        W1 CLR
LOOP:   PUPACK RECORDS(W1), OUTPUTS(W1)
        W1 ADD 1
        W1 COMP COUNT
        IF<GO LOOP
```

**Explanation:** Batch convert BCD array to ASCII output.

### Example 5: Database export

```assembly
        % Export BCD field to CSV
        PUPACK DB_BALANCE, CSV_FIELD
        % CSV_FIELD now ready for file output
```

**Explanation:** Prepare BCD database value for text file export.

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **BCD overflow (BO)**: Result exceeds destination size
- **Invalid operation (IVO)**: Invalid BCD digit encoding

---

## Data Status Bits

- **Z (Zero)**: Value equals zero after conversion
- **S (Sign)**: Value sign bit
- **BO (BCD Overflow)**: Destination field too small
- **K (Error)**: BO or IVO occurred

---

## Reference Manual

**Section:** §17.8
**Title:** Convert packed to ASCII

---

## See Also

- [PUPACKR](pupackr.md) - Convert packed to ASCII with rounding
- [PPACK](ppack.md) - Convert binary to packed BCD
- [WPCONV](wpconv.md) - Convert word to packed BCD
