# TEST - Test

## Overview

**Mnemonic:** `test`
**Function:** Test
**Class:** COMPARE
**Privilege:** user

**Format:** `{prefix}{register} TEST <operand>`

---

## Description

This instruction is similar to comparing two operands, except that the second operand is implicitly zero.

**Operands:** 1
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0x0041 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/6 | 0x0042 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/6 | 0x0043 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/6 | 0x0044 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/6 | 0x0045 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/6 | 0x0046 | BI | 2 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for test
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.11
**Title:** Test against zero

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
