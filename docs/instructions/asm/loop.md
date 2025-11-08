# LOOP - Loop

## Overview

**Mnemonic:** `loop`
**Function:** Loop
**Class:** BRANCH
**Privilege:** user

**Format:** `{prefix}{register} LOOP <operands>`

---

## Description

[Description for LOOP instruction to be written based on Reference Manual §13.6]

**Operands:** 4
**Variants:** 10 opcode(s)

---

## Variants

Total variants: 10

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/10 | 0xFD2D | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/10 | 0xFD2E | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/10 | 0xFD2F | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/10 | 0xFD30 | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/10 | 0xFD31 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 6/10 | 0xFD32 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 7/10 | 0xFD33 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 8/10 | 0xFD34 | BY | 4 | LOCAL, RECORD, REGISTER... |
| 9/10 | 0xFD35 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 10/10 | 0xFD36 | BY | 2 | LOCAL, RECORD, REGISTER... |

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

**Supported modes:**
- **LOCAL**
- **RECORD**
- **CONSTANT**
- **REGISTER**
- **PRE_INDEXED**
- **ABSOLUTE**

### Operand 4

[Description for operand 4]

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
        ; Example usage of LOOP
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.6
**Title:** Loop General

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
