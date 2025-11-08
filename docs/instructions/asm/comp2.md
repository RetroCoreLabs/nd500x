# COMP2 - Comp2

## Overview

**Mnemonic:** `comp2`
**Function:** Comp2
**Class:** COMPARE
**Privilege:** user

**Format:** `{prefix}{register} COMP2 <op1>,<op2>`

---

## Description

The compare two operands instruction subtracts the second operand from the first. The result sets the data status bits accordingly, but the result is otherwise discarded.

**Operands:** 2
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0x002D | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/6 | 0x002E | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/6 | 0x002F | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/6 | 0x0040 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/6 | 0xFC15 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/6 | 0xFC16 | BI | 2 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for comp2
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.10
**Title:** Compare two operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
