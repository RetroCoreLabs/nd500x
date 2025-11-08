# SMOVE - Smove

## Overview

**Mnemonic:** `smove`
**Function:** Smove
**Class:** STRING
**Privilege:** user

**Format:** `{prefix}{register} SMOVE <op1>,<op2>`

---

## Description

[Description for SMOVE instruction to be written based on Reference Manual §14.3]

**Operands:** 2
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0xFD66 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/6 | 0xFD67 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/6 | 0xFD68 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/6 | 0xFD69 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/6 | 0xFD6A | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/6 | 0xFD6B | BI | 2 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of SMOVE
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §14.3
**Title:** String move while

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
