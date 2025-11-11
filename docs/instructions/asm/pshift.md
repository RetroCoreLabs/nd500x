# PSHIFT - Packed Shift

## Overview

**Mnemonic:** `pshift`
**Function:** Shift packed BCD decimal point (rescale)
**Class:** SHIFT
**Privilege:** user

**Format:** `PSHIFT <source/BCD>, <dest/BCD>`

---

## Description

Shifts the source BCD value to match the destination's scaling factor, effectively changing the decimal point position. The value itself remains unchanged except for the scale adjustment. If source and destination have identical scales, performs a simple move operation. Destination is extended with leading zeros if necessary. No rounding is performed (use PSHIFTR for rounding).

**Operation:**
```
<source> → <dest> (rescaled to dest scale)
```

**Key Characteristics:**
- Rescales BCD value to match destination precision
- No rounding (truncates if necessary)
- If same scale: simple move operation
- Destination padded with leading zeros if needed
- Sign control via descriptor bit 26
- Value unchanged except for scale adjustment

**Common Use Cases:**
- Currency precision conversions
- Database field formatting
- Display formatting (internal → external precision)
- Multi-scale arithmetic preparation
- Report generation with specific precision
- Financial system integration

**Operands:** 2 (BCD source, BCD destination)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFEB2 | PSHIFT |

---

## Examples

### Example 1: Scale adjustment
```assembly
PSHIFT SUBTOTAL, TOTAL
```

### Example 2: Precision change
```assembly
PSHIFT HIGH_PRECISION, DISPLAY_VAL
```

### Example 3: Currency format
```assembly
PSHIFT CALC_RESULT, CURRENCY_AMT
```

### Example 4: Database storage
```assembly
PSHIFT INPUT_VALUE, DB_FIELD
```

### Example 5: Report formatting
```assembly
PSHIFT INTERNAL_AMT, REPORT_FIELD
```

### Example 6: Batch processing
```assembly
LOOP:
        PSHIFT SRC_ARRAY(W1), DST_ARRAY(W1)
        W1 INC
        W1 COMP COUNT
        IF<GO LOOP
```

### Example 7: Multi-scale operations
```assembly
PSHIFT VAR1, TEMP1
PSHIFT VAR2, TEMP2
PADD TEMP1, TEMP2, RESULT
```

---

## Reference Manual
**Section:** §17.6

---

## See Also
- [PSHIFTR](pshiftr.md) - With rounding
