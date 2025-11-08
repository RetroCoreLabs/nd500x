# COMP - Comp

## Overview

**Mnemonic:** `comp`
**Function:** Comp
**Class:** COMPARE
**Privilege:** user

**Format:** `{prefix}{register} COMP <operand>`

---

## Description

The compare instruction subtracts the operand from the contents of the specified register. The result of the subtraction is not saved, but rather compared to zero, and this result is saved in the data status bits. The instruction is a true comparison, hence the sign bit is changed in case of integer overflow.

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0x0030 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/24 | 0x0031 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/24 | 0x0032 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/24 | 0x0033 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/24 | 0x0034 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/24 | 0x0035 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/24 | 0x0036 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/24 | 0x0037 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/24 | 0x0038 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/24 | 0x0039 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/24 | 0x003A | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/24 | 0x003B | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/24 | 0x003C | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/24 | 0x003D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/24 | 0x003E | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/24 | 0x003F | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 17/24 | 0xFC18 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 18/24 | 0xFC19 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 19/24 | 0xFC1A | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 20/24 | 0xFC1B | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 21/24 | 0xFC1C | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 22/24 | 0xFC1D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 23/24 | 0xFC1E | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 24/24 | 0xFC1F | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for comp
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.9
**Title:** Compare

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
