# SFILL - Sfill

## Overview

**Mnemonic:** `sfill`
**Function:** Sfill
**Class:** STRING
**Privilege:** user

**Format:** `{prefix}{register} SFILL <operand>`

---

## Description

The contents of the specified register are put into every element of the `<-dest=>` string starting at the element specified by the I2 register.

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0xFD7C | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/24 | 0xFD7D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/24 | 0xFD7E | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/24 | 0xFD7F | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/24 | 0xFD80 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/24 | 0xFD81 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/24 | 0xFD82 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/24 | 0xFD83 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/24 | 0xFD84 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/24 | 0xFD85 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/24 | 0xFD86 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/24 | 0xFD87 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/24 | 0xFD88 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/24 | 0xFD89 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/24 | 0xFD8A | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/24 | 0xFD8B | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 17/24 | 0xFD8C | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 18/24 | 0xFD8D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 19/24 | 0xFD8E | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 20/24 | 0xFD8F | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 21/24 | 0xFD90 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 22/24 | 0xFD91 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 23/24 | 0xFD92 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 24/24 | 0xFD93 | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for sfill
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §14.8
**Title:** String fill

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
