# BICONV - Biconv

## Overview

**Mnemonic:** `biconv`
**Function:** Biconv
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `{prefix}{register} BICONV <op1>,<op2>`

---

## Description

BICONV instruction

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0xFD49 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/5 | 0xFD4E | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/5 | 0xFD53 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/5 | 0xFD58 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/5 | 0xFD5D | BY | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for biconv
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §15.2
**Title:** Bit convert

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
