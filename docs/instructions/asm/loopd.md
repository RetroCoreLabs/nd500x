# LOOPD - Loopd

## Overview

**Mnemonic:** `loopd`
**Function:** Loopd
**Class:** BRANCH
**Privilege:** user

**Format:** `{prefix}{register} LOOPD <operands>`

---

## Description

[Description for LOOPD instruction to be written based on Reference Manual §13.5]

**Operands:** 3
**Variants:** 10 opcode(s)

---

## Variants

Total variants: 10

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/10 | 0xFD23 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/10 | 0xFD24 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/10 | 0xFD25 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/10 | 0xFD26 | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/10 | 0xFD27 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 6/10 | 0xFD28 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 7/10 | 0xFD29 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 8/10 | 0xFD2A | BY | 4 | LOCAL, RECORD, REGISTER... |
| 9/10 | 0xFD2B | BY | 1 | LOCAL, RECORD, REGISTER... |
| 10/10 | 0xFD2C | BY | 2 | LOCAL, RECORD, REGISTER... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
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
        ; Example usage of LOOPD
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.5
**Title:** Loop with decrement

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
