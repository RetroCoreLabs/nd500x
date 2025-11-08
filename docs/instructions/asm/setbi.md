# SETBI - Setbi

## Overview

**Mnemonic:** `setbi`
**Function:** Setbi
**Class:** BITFIELD
**Privilege:** user

**Format:** `{prefix}{register} SETBI <op1>,<op2>`

---

## Description

The specified bit of a BY, H, or W \<operand> is set. A \<bit No.> greater than or equal to the number of bits of the data type or a negative \<bit No.> will cause an illegal operand value trap condition.

**Operands:** 2
**Variants:** 3 opcode(s)

---

## Variants

Total variants: 3

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/3 | 0xFE80 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/3 | 0xFE81 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/3 | 0xFE82 | BY | 3 | LOCAL, RECORD, REGISTER... |

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
        ; Example for setbi
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.30
**Title:** Set bit

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
