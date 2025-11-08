# := - AssignTo

## Overview

**Mnemonic:** `:=`
**Function:** AssignTo
**Class:** MOVE
**Privilege:** user

**Format:** `{prefix}{register} := <operand>`

---

## Description

[Description for := instruction to be written based on Reference Manual §10.1]

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0x0004 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/24 | 0x0005 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/24 | 0x0006 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/24 | 0x0007 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/24 | 0x0008 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/24 | 0x0009 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/24 | 0x000A | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/24 | 0x000B | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/24 | 0x000C | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/24 | 0x000D | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/24 | 0x000E | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/24 | 0x000F | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/24 | 0x0010 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/24 | 0x0011 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/24 | 0x0012 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/24 | 0x0013 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 17/24 | 0x0014 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 18/24 | 0x0015 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 19/24 | 0x0016 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 20/24 | 0x0017 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 21/24 | 0xFC04 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 22/24 | 0xFC05 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 23/24 | 0xFC06 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 24/24 | 0xFC07 | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of :=
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.1
**Title:** Load

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
