# GETBF - Getbf

## Overview

**Mnemonic:** `getbf`
**Function:** Getbf
**Class:** BITFIELD
**Privilege:** user

**Format:** `{prefix}{register} GETBF <operands>`

---

## Description

Bit 0 to <field size> - 1 of the specified register is loaded with the specified bit field. In the <operand>, the bit field is composed of the <bit No.> bit and as many higher numbered bits as necessary to obtain a field size of <field size> bits. (See the section on data types in memory for an explanation of bit numbers within data types.) The <operand> may have BY, H, or W as the data type. <bit No.> and <field size> are interpreted as signed byte integers. An illegal operand value trap condit

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFDE0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/12 | 0xFDE1 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/12 | 0xFDE2 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/12 | 0xFDE3 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/12 | 0xFDE4 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/12 | 0xFDE5 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/12 | 0xFDE6 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/12 | 0xFDE7 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/12 | 0xFDE8 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/12 | 0xFDE9 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/12 | 0xFDEA | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/12 | 0xFDEB | BY | 4 | LOCAL, RECORD, CONSTANT... |

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

### Operand 2

[Description for operand 2]

**Supported modes:**
- **LOCAL**
- **RECORD**
- **CONSTANT**
- **REGISTER**
- **PRE_INDEXED**
- **ABSOLUTE**

### Operand 3

[Description for operand 3]

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
        ; Example for getbf
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.31
**Title:** Get bit field

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
