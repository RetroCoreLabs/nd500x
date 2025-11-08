# COS - Cos

## Overview

**Mnemonic:** `cos`
**Function:** Cos
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `{prefix}{register} COS <operand>`

---

## Description

The trigonometric arcsine of ⟨argument⟩ is loaded into the specified float or double float register. The result value gives the angle in radians, in the range -pi/2 to pi/2. ⟨argument⟩ should be in the range -1 to +1, otherwise an invalid operation trap condition will occur and the specified register will be set to zero.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/8 | 0xFF60 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 2/8 | 0xFF61 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 3/8 | 0xFF62 | F | 3 | LOCAL, RECORD, CONSTANT... |
| 4/8 | 0xFF63 | F | 4 | LOCAL, RECORD, CONSTANT... |
| 5/8 | 0xFF8C | F | 1 | LOCAL, RECORD, CONSTANT... |
| 6/8 | 0xFF8D | F | 2 | LOCAL, RECORD, CONSTANT... |
| 7/8 | 0xFF8E | F | 3 | LOCAL, RECORD, CONSTANT... |
| 8/8 | 0xFF8F | F | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for cos
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §12.6
**Title:** Cosine

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
