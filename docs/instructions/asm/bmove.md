# BMOVE - Bmove

## Overview

**Mnemonic:** `bmove`
**Function:** Bmove
**Class:** MOVE
**Privilege:** user

**Format:** `{prefix}{register} BMOVE <operands>`

---

## Description

Allocate an element of size 2

**Operands:** 3
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0xFD20 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/5 | 0xFE78 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/5 | 0xFE79 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/5 | 0xFE7A | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/5 | 0xFE7B | BY | 1 | LOCAL, RECORD, CONSTANT... |

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
- **PRE_INDEXED**
- **ABSOLUTE**

### Operand 3

[Description for operand 3]

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
        ; Example for bmove
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §15.13
**Title:** Block move

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
