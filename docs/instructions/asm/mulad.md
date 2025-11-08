# MULAD - Mulad

## Overview

**Mnemonic:** `mulad`
**Function:** Mulad
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} MULAD <op1>,<op2>`

---

## Description

The contents of the specified register is multiplied by the `<x>` operand, the `<y>` operand is added to the product and the result loaded into the register.

**Operands:** 2
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x00A8 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/20 | 0x00A9 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/20 | 0x00AA | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/20 | 0x00AB | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/20 | 0xFCE8 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/20 | 0xFCE9 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/20 | 0xFCEA | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/20 | 0xFCEB | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/20 | 0xFCEC | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/20 | 0xFCED | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/20 | 0xFCEE | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/20 | 0xFCEF | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 13/20 | 0xFCF0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 14/20 | 0xFCF1 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 15/20 | 0xFCF2 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 16/20 | 0xFCF3 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 17/20 | 0xFCF4 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 18/20 | 0xFCF5 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 19/20 | 0xFCF6 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 20/20 | 0xFCF7 | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for mulad
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.19
**Title:** Multiply and add

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
