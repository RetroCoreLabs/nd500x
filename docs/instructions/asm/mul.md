# * - Multiply

## Overview

**Mnemonic:** `*`
**Function:** Multiply
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} * <operand>`

---

## Description

The `<a>` operand is multiplied by the `<b>` operand and the product is stored in the `<a>` operand. Integer overflow occurs if the upper half of the double length result is not equal to the sign extension of the lower half.

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x006C | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/20 | 0x006D | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/20 | 0x006E | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/20 | 0x006F | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/20 | 0x0070 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/20 | 0x0071 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/20 | 0x0072 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/20 | 0x0073 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/20 | 0x0074 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/20 | 0x0075 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/20 | 0x0076 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/20 | 0x0077 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 13/20 | 0xFC44 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 14/20 | 0xFC45 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 15/20 | 0xFC46 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 16/20 | 0xFC47 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 17/20 | 0xFC48 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 18/20 | 0xFC49 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 19/20 | 0xFC4A | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 20/20 | 0xFC4B | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for *
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.7
**Title:** Multiply two operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
