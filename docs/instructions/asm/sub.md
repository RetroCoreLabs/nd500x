# - - Subtract

## Overview

**Mnemonic:** `-`
**Function:** Subtract
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} - <operand>`

---

## Description

- instruction

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x0060 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/20 | 0x0061 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/20 | 0x0062 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/20 | 0x0063 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/20 | 0x0064 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/20 | 0x0065 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/20 | 0x0066 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/20 | 0x0067 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/20 | 0x0068 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/20 | 0x0069 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/20 | 0x006A | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/20 | 0x006B | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 13/20 | 0xFC3C | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 14/20 | 0xFC3D | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 15/20 | 0xFC3E | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 16/20 | 0xFC3F | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 17/20 | 0xFC40 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 18/20 | 0xFC41 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 19/20 | 0xFC42 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 20/20 | 0xFC43 | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for -
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.6
**Title:** Subtract two operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
