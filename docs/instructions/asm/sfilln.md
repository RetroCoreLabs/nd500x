# SFILLN - Sfilln

## Overview

**Mnemonic:** `sfilln`
**Function:** Sfilln
**Class:** STRING
**Privilege:** user

**Format:** `{prefix}{register} SFILLN <op1>,<op2>`

---

## Description

[Description for SFILLN instruction to be written based on Reference Manual §14.9]

**Operands:** 2
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0xFD94 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/24 | 0xFD95 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/24 | 0xFD96 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/24 | 0xFD97 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/24 | 0xFD98 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/24 | 0xFD99 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/24 | 0xFD9A | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/24 | 0xFD9B | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/24 | 0xFD9C | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/24 | 0xFD9D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/24 | 0xFD9E | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/24 | 0xFD9F | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/24 | 0xFDA0 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/24 | 0xFDA1 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/24 | 0xFDA2 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/24 | 0xFDA3 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 17/24 | 0xFDA4 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 18/24 | 0xFDA5 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 19/24 | 0xFDA6 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 20/24 | 0xFDA7 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 21/24 | 0xFDA8 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 22/24 | 0xFDA9 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 23/24 | 0xFDAA | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 24/24 | 0xFDAB | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of SFILLN
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §14.9
**Title:** String fill n elements

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
