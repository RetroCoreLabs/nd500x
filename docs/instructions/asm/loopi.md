# LOOPI - Loopi

## Overview

**Mnemonic:** `loopi`
**Function:** Loopi
**Class:** BRANCH
**Privilege:** user

**Format:** `{prefix}{register} LOOPI <operands>`

---

## Description

[Description for LOOPI instruction to be written based on Reference Manual §13.4]

**Operands:** 3
**Variants:** 10 opcode(s)

---

## Variants

Total variants: 10

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/10 | 0x00BF | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/10 | 0x00E1 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/10 | 0xFCDE | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/10 | 0xFCDF | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/10 | 0xFD1C | BY | 1 | LOCAL, RECORD, REGISTER... |
| 6/10 | 0xFD1D | BY | 2 | LOCAL, RECORD, REGISTER... |
| 7/10 | 0xFD1E | BY | 3 | LOCAL, RECORD, REGISTER... |
| 8/10 | 0xFD1F | BY | 4 | LOCAL, RECORD, REGISTER... |
| 9/10 | 0xFD21 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 10/10 | 0xFD22 | BY | 2 | LOCAL, RECORD, REGISTER... |

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
        ; Example usage of LOOPI
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.4
**Title:** Loop with increment

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
