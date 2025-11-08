# PUTBF - Putbf

## Overview

**Mnemonic:** `putbf`
**Function:** Putbf
**Class:** BITFIELD
**Privilege:** user

**Format:** `{prefix}{register} PUTBF <operands>`

---

## Description

PUTBF instruction

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFDEC | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/12 | 0xFDED | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/12 | 0xFDEE | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/12 | 0xFDEF | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/12 | 0xFDF0 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 6/12 | 0xFDF1 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 7/12 | 0xFDF2 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 8/12 | 0xFDF3 | BY | 4 | LOCAL, RECORD, REGISTER... |
| 9/12 | 0xFDF4 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 10/12 | 0xFDF5 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 11/12 | 0xFDF6 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 12/12 | 0xFDF7 | BY | 4 | LOCAL, RECORD, REGISTER... |

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
        ; Example for putbf
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.32
**Title:** Put bit field

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
