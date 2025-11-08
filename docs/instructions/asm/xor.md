# XOR - Xor

## Overview

**Mnemonic:** `xor`
**Function:** Xor
**Class:** LOGICAL
**Privilege:** user

**Format:** `{prefix}{register} XOR <operand>`

---

## Description

[Description for XOR instruction to be written based on Reference Manual §10.23]

**Operands:** 1
**Variants:** 16 opcode(s)

---

## Variants

Total variants: 16

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/16 | 0x00A4 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/16 | 0x00A5 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/16 | 0x00A6 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/16 | 0x00A7 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/16 | 0xFCA0 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/16 | 0xFCA1 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/16 | 0xFCA2 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/16 | 0xFCA3 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/16 | 0xFCA4 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/16 | 0xFCA5 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/16 | 0xFCA6 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/16 | 0xFCA7 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/16 | 0xFDFC | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/16 | 0xFDFD | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/16 | 0xFDFE | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/16 | 0xFDFF | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of XOR
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.23
**Title:** Exclusive or

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
