# PPACKR - Convert ASCII to Packed Rounded

## Overview

**Mnemonic:** `ppackr`
**Function:** Convert ASCII to packed BCD with rounding
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `PPACKR <source/ASCII>, <dest/BCD>`

---

## Description

Converts ASCII decimal to packed BCD with rounding. Identical to PPACK but rounds the value before storing in destination.

**Operands:** 2 | **Variants:** 1

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE92 | PPACKR |

---

## Examples

### Example 1-7: Similar to PPACK with rounding
```assembly
PPACKR ASCII_INPUT, ROUNDED_BCD
PPACKR FORM.AMOUNT, RECORD.TOTAL
PPACKR TEXT_FIELD, BCD_STORAGE
PPACKR USER_ENTRY, VALIDATED_AMT
PPACKR INPUT_STR, ROUNDED_VAL
PPACKR CONVERSION_SRC, DEST_BCD
PPACKR DECIMAL_TEXT, FINAL_VALUE
```

---

## Reference Manual
**Section:** §17.7

---

## See Also
- [PPACK](ppack.md) - Without rounding
