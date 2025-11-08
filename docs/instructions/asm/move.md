# MOVE - Move

## Overview

**Mnemonic:** `move`
**Function:** Move
**Class:** MOVE
**Privilege:** user

**Format:** `{prefix}{register} MOVE <op1>,<op2>`

---

## Description

[Description for MOVE instruction to be written based on Reference Manual §10.7]

**Operands:** 2
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0x0019 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/6 | 0x001A | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/6 | 0x001B | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/6 | 0x002C | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/6 | 0xFC0B | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/6 | 0xFC14 | BI | 2 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of MOVE
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.7
**Title:** Move

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
