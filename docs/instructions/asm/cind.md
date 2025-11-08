# CIND - Cind

## Overview

**Mnemonic:** `cind`
**Function:** Cind
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} CIND <operands>`

---

## Description

The address of an element in a multi-dimensional array is calculated. The range of the dimension, `<upper>` - `<lower>` + 1, is multiplied by the contents of the specified register. `<index>` is added to the product and the result loaded into the specified register. If `<index>` is less than the `<lower>` operand or greater than the `<upper>` operand, the flag bit (K) is set and an illegal index trap condition occurs.

**Operands:** 3
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x00B0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/20 | 0x00B1 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/20 | 0x00B2 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/20 | 0x00B3 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/20 | 0xFD14 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/20 | 0xFD15 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/20 | 0xFD16 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/20 | 0xFD17 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/20 | 0xFD18 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/20 | 0xFD19 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/20 | 0xFD1A | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/20 | 0xFD1B | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 13/20 | 0xFFD0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 14/20 | 0xFFD1 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 15/20 | 0xFFD2 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 16/20 | 0xFFD3 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 17/20 | 0xFFD4 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 18/20 | 0xFFD5 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 19/20 | 0xFFD6 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 20/20 | 0xFFD7 | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for cind
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §15.9
**Title:** Calculate Index

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
