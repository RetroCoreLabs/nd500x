# PSHIFT - Packed Shift

## Overview

**Mnemonic:** `pshift`
**Function:** Shift packed BCD decimal point
**Class:** SHIFT
**Privilege:** user
**Format:** `PSHIFT <source/BCD>, <dest/BCD>`

---

## Description

Shifts packed BCD value to match destination's scaling factor. Changes decimal position without modifying value (except for scaling). If scales match, performs a move.

**Operands:** 2 | **Variants:** 1

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
