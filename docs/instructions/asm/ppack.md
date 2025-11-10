# PPACK - Convert ASCII to Packed

## Overview

**Mnemonic:** `ppack`
**Function:** Convert ASCII decimal to packed BCD
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `PPACK <source/ASCII>, <dest/BCD>`

---

## Description

Converts ASCII coded decimal to packed BCD format. Source contains ASCII digits, destination receives packed BCD with automatic sign handling and scaling.

**Operands:** 2 | **Variants:** 1

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFEB5 | PPACK |

---

## Trap Conditions
- **BCD overflow (BO)**, **Invalid operation (IVO)**

---

## Data Status Bits
- **Z,S,BO,K**: Set based on result

---

## Examples

### Example 1: Convert input field
```assembly
PPACK IFIELD, VAR1
```

### Example 2: User input
```assembly
PPACK USER_INPUT, AMOUNT
```

### Example 3: String to number
```assembly
PPACK ASCII_NUM, BCD_VAL
```

### Example 4: Form processing
```assembly
PPACK FORM.FIELD1, RECORD.PRICE
```

### Example 5: Batch conversion
```assembly
LOOP:
        PPACK ASCII_FIELDS(W1), BCD_FIELDS(W1)
        W1 INC
        W1 COMP COUNT
        IF<GO LOOP
```

### Example 6: Validation
```assembly
PPACK INPUT, TEMP
IFKGO INVALID_INPUT
```

### Example 7: Financial entry
```assembly
PPACK AMOUNT_TEXT, TRANSACTION.AMOUNT
```

---

## Reference Manual
**Section:** §17.7

---

## See Also
- [PPACKR](ppackr.md) - Packed with rounding
- [PUPACK](pupack.md) - Unpack to ASCII
