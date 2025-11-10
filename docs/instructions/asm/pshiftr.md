# PSHIFTR - Packed Shift Rounded

## Overview

**Mnemonic:** `pshiftr`
**Function:** Shift packed BCD with rounding
**Class:** SHIFT
**Privilege:** user
**Format:** `PSHIFTR <source/BCD>, <dest/BCD>`

---

## Description

Shifts packed BCD to destination scale with rounding. Identical to PSHIFT but rounds before storing.

**Operands:** 2 | **Variants:** 1

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE87 | PSHIFTR |

---

## Examples

### Example 1-7: Similar to PSHIFT with rounding
```assembly
PSHIFTR PRECISE_VAL, ROUNDED_RESULT
PSHIFTR CALC_AMT, DISPLAY_AMT
PSHIFTR INTERNAL, EXTERNAL
PSHIFTR HIGH_PREC, LOW_PREC
PSHIFTR SOURCE_SCALE, DEST_SCALE
PSHIFTR TEMP_VAL, FINAL_VAL
PSHIFTR WORKING_AMT, STORED_AMT
```

---

## Reference Manual
**Section:** §17.6

---

## See Also
- [PSHIFT](pshift.md) - Without rounding
