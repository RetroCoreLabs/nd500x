# DIV3 - Div3

## Overview

**Mnemonic:** `div3`
**Function:** Div3
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} DIV3 <operands>`

---

## Description

The `<a>` operand is divided by the `<b>` operand and the quotient is stored in the `<c>` operand. In integer division the remainder (unless it is zero) has the same sign as the `<a>` operand, i.e., the quotient is truncated towards zero. Integer overflow occurs if and only if the largest possible negative integer is divided by -1. The operands are assumed to have the same data type (see section 7.3 on page 73).

**Operands:** 3
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0xFC76 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/5 | 0xFC77 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/5 | 0xFC78 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/5 | 0xFC79 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/5 | 0xFC7A | BY | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for div3
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.12
**Title:** Divide three operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
