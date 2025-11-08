# GETB - Getb

## Overview

**Mnemonic:** `getb`
**Function:** Getb
**Class:** BITFIELD
**Privilege:** user

**Format:** `{prefix}{register} GETB <operand>`

---

## Description

[Description for GETB instruction to be written based on Reference Manual §10.27]

**Operands:** 1
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/4 | 0xFE4C | W | 1 | LOCAL, RECORD, CONSTANT... |
| 2/4 | 0xFE4D | W | 2 | LOCAL, RECORD, CONSTANT... |
| 3/4 | 0xFE4E | W | 3 | LOCAL, RECORD, CONSTANT... |
| 4/4 | 0xFE4F | W | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example usage of GETB
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.27
**Title:** Get bit

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
