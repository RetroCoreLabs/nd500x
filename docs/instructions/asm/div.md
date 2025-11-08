# / - Divide

## Overview

**Mnemonic:** `/`
**Function:** Divide
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} / <operand>`

---

## Description

The \<a> operand is divided by the \<b> operand and the quotient is stored in the \<a> operand. In integer division the remainder (unless it is zero) has the same sign as the \<a> operand, i.e. the quotient is truncated towards zero. Integer overflow occurs if and only if the largest possible negative integer is divided by -1.

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x0078 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/20 | 0x0079 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/20 | 0x007A | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/20 | 0x007B | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/20 | 0x007C | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/20 | 0x007D | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/20 | 0x007E | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/20 | 0x007F | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/20 | 0x00E8 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/20 | 0x00E9 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/20 | 0x00EA | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/20 | 0x00EB | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 13/20 | 0xFC4C | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 14/20 | 0xFC4D | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 15/20 | 0xFC4E | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 16/20 | 0xFC4F | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 17/20 | 0xFC50 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 18/20 | 0xFC51 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 19/20 | 0xFC52 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 20/20 | 0xFC53 | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for /
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.8
**Title:** Divide two operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
