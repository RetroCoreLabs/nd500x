# DCONV - Dconv

## Overview

**Mnemonic:** `dconv`
**Function:** Dconv
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `{prefix}{register} DCONV <op1>,<op2>`

---

## Description

[Description for DCONV instruction to be written based on Reference Manual §TBD]

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0xFD48 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/5 | 0xFD4D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/5 | 0xFD52 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/5 | 0xFD57 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/5 | 0xFD5C | BI | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of DCONV
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
