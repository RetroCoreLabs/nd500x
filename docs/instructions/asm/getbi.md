# GETBI - Getbi

## Overview

**Mnemonic:** `getbi`
**Function:** Getbi
**Class:** BITFIELD
**Privilege:** user

**Format:** `{prefix}{register} GETBI <op1>,<op2>`

---

## Description

[Description for GETBI instruction to be written based on Reference Manual §TBD]

**Operands:** 2
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFCB4 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/12 | 0xFCB5 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/12 | 0xFCB6 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/12 | 0xFCB7 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/12 | 0xFCB8 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/12 | 0xFCB9 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/12 | 0xFCBA | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/12 | 0xFCBB | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/12 | 0xFDD0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/12 | 0xFDD1 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/12 | 0xFDD2 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/12 | 0xFDD3 | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of GETBI
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §TBD
**Title:** TBD

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
