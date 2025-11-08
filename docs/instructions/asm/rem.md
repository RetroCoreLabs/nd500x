# REM - Rem

## Overview

**Mnemonic:** `rem`
**Function:** Rem
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} REM <operands>`

---

## Description

[Description for REM instruction to be written based on Reference Manual §11.20]

**Operands:** 3
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/8 | 0xFE58 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 2/8 | 0xFE59 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 3/8 | 0xFE5A | F | 3 | LOCAL, RECORD, CONSTANT... |
| 4/8 | 0xFE5B | F | 4 | LOCAL, RECORD, CONSTANT... |
| 5/8 | 0xFE5C | F | 1 | LOCAL, RECORD, CONSTANT... |
| 6/8 | 0xFE5D | F | 2 | LOCAL, RECORD, CONSTANT... |
| 7/8 | 0xFE5E | F | 3 | LOCAL, RECORD, CONSTANT... |
| 8/8 | 0xFE5F | F | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of REM
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.20
**Title:** Remainder

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
