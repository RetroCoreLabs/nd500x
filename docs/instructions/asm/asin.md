# ASIN - Asin

## Overview

**Mnemonic:** `asin`
**Function:** Asin
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `{prefix}{register} ASIN <operand>`

---

## Description

The trigonometric cosine of <argument> is loaded into the specified float or double float register. The maximum absolute value of <argument> is 65536.0 radians; a larger value will cause an invalid operation trap condition and the specified register will be set to zero.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/8 | 0xFF5C | F | 1 | LOCAL, RECORD, CONSTANT... |
| 2/8 | 0xFF5D | F | 2 | LOCAL, RECORD, CONSTANT... |
| 3/8 | 0xFF5E | F | 3 | LOCAL, RECORD, CONSTANT... |
| 4/8 | 0xFF5F | F | 4 | LOCAL, RECORD, CONSTANT... |
| 5/8 | 0xFF88 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 6/8 | 0xFF89 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 7/8 | 0xFF8A | F | 3 | LOCAL, RECORD, CONSTANT... |
| 8/8 | 0xFF8B | F | 4 | LOCAL, RECORD, CONSTANT... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
- **CONSTANT**
- **REGISTER**
- **PRE_INDEXED**
- **ABSOLUTE**

---

## Trap Conditions

- **OPERAND_ERROR (Bit 5):** Invalid addressing mode or alignment

[Additional trap conditions based on instruction type]

---

## Data Status Bits

- **Z (Zero):** [Effect on zero flag]
- **S (Sign):** [Effect on sign flag]
- **O (Overflow):** [Effect on overflow flag]
- **K (Flag):** [Effect on K flag]

---

## Examples

### Example 1: Basic Usage

```assembly
        ; Example for asin
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §12.7
**Title:** Arc sine

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
