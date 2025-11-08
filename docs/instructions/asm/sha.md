# SHA - Sha

## Overview

**Mnemonic:** `sha`
**Function:** Sha
**Class:** SHIFT
**Privilege:** user

**Format:** `{prefix}{register} SHA <op1>,<op2>`

---

## Description

[Description for SHA instruction to be written based on Reference Manual §TBD]

**Operands:** 2
**Variants:** 3 opcode(s)

---

## Variants

Total variants: 3

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/3 | 0xFCAB | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/3 | 0xFCAC | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/3 | 0xFCAD | BY | 3 | LOCAL, RECORD, REGISTER... |

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
        ; Example usage of SHA
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §TBD
**Title:** TBD

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
