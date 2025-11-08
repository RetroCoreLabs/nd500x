# HCONV - Hconv

## Overview

**Mnemonic:** `hconv`
**Function:** Hconv
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `{prefix}{register} HCONV <op1>,<op2>`

---

## Description

HCONV instruction

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0xFD45 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/5 | 0xFD4A | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/5 | 0xFD55 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/5 | 0xFD5A | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/5 | 0xFD5F | BI | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for hconv
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
**Title:** Halfword convert

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
