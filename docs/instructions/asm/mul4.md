# MUL4 - Mul4

## Overview

**Mnemonic:** `mul4`
**Function:** Mul4
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} MUL4 <operands>`

---

## Description

The `<a>` operand is multiplied by the `<b>` operand. The product is stored in the `<c>` operand. The upper half of the double length result is stored in the specified register. The operands are assumed to have the same data type (see section 7.3 on page 73).

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFC20 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/12 | 0xFC21 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/12 | 0xFC22 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/12 | 0xFC23 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/12 | 0xFC24 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/12 | 0xFC25 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/12 | 0xFC26 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/12 | 0xFC27 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/12 | 0xFC28 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/12 | 0xFC29 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/12 | 0xFC2A | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/12 | 0xFC2B | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for mul4
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.13
**Title:** Multiply with overflow to register

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
