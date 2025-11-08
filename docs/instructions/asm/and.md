# AND - And

## Overview

**Mnemonic:** `and`
**Function:** And
**Class:** LOGICAL
**Privilege:** user

**Format:** `{prefix}{register} AND <operand>`

---

## Description

A bitwise AND is performed between the contents of the specified register and the ⟨operand⟩ and the result is stored in the register. When the data type is BI, BY, or H, the upper part of the register is zero filled.

**Operands:** 1
**Variants:** 16 opcode(s)

---

## Variants

Total variants: 16

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/16 | 0x00E4 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/16 | 0x00E5 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/16 | 0x00E6 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/16 | 0x00E7 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/16 | 0xFC90 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/16 | 0xFC91 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/16 | 0xFC92 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/16 | 0xFC93 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/16 | 0xFC94 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/16 | 0xFC95 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/16 | 0xFC96 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/16 | 0xFC97 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/16 | 0xFDCC | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/16 | 0xFDCD | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/16 | 0xFDCE | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/16 | 0xFDCF | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for and
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.21
**Title:** And

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
