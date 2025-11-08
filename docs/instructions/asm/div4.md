# DIV4 - Div4

## Overview

**Mnemonic:** `div4`
**Function:** Div4
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} DIV4 <operands>`

---

## Description

DIV4 instruction

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFC2C | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/12 | 0xFC2D | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/12 | 0xFC2E | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/12 | 0xFC2F | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/12 | 0xFC30 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/12 | 0xFC31 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/12 | 0xFC32 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/12 | 0xFC33 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/12 | 0xFC7C | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/12 | 0xFC7D | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/12 | 0xFC7E | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/12 | 0xFC7F | BY | 4 | LOCAL, RECORD, CONSTANT... |

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

### Operand 3

[Description for operand 3]

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
        ; Example for div4
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.14
**Title:** Divide with overflow to register

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
