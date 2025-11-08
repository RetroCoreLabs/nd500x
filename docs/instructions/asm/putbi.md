# PUTBI - Putbi

## Overview

**Mnemonic:** `putbi`
**Function:** Putbi
**Class:** BITFIELD
**Privilege:** user

**Format:** `{prefix}{register} PUTBI <op1>,<op2>`

---

## Description

Bit zero of the specified register is stored in bit `<bit No.>` of a BY, H, or W `<operand>`. The upper bits of the `<operand>` are unaffected, even when the destination is a word register. A `<bit No.>` greater than or equal to the number of bits of the data type or a negative `<bit No.>` will cause an illegal operand value trap condition.

**Operands:** 2
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFDD4 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/12 | 0xFDD5 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/12 | 0xFDD6 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/12 | 0xFDD7 | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/12 | 0xFDD8 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 6/12 | 0xFDD9 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 7/12 | 0xFDDA | BY | 3 | LOCAL, RECORD, REGISTER... |
| 8/12 | 0xFDDB | BY | 4 | LOCAL, RECORD, REGISTER... |
| 9/12 | 0xFDDC | BY | 1 | LOCAL, RECORD, REGISTER... |
| 10/12 | 0xFDDD | BY | 2 | LOCAL, RECORD, REGISTER... |
| 11/12 | 0xFDDE | BY | 3 | LOCAL, RECORD, REGISTER... |
| 12/12 | 0xFDDF | BY | 4 | LOCAL, RECORD, REGISTER... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
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
        ; Example for putbi
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.28
**Title:** Put bit

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
