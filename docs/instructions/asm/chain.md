# CHAIN - Chain

## Overview

**Mnemonic:** `chain`
**Function:** Chain
**Class:** CALL
**Privilege:** user

**Format:** `{prefix}{register} CHAIN <operands>`

---

## Description

[Description for CHAIN instruction to be written based on Reference Manual §13.9]

**Operands:** 3
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/4 | 0xFD6C | W | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 2/4 | 0xFD6D | W | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 3/4 | 0xFD6E | W | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 4/4 | 0xFD6F | W | 4 | LOCAL, RECORD, PRE_INDEXED... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
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
        ; Example usage of CHAIN
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.9
**Title:** Chain subroutine

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
