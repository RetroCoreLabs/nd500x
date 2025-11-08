# SUBC - Subc

## Overview

**Mnemonic:** `subc`
**Function:** Subc
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} SUBC <operand>`

---

## Description

The carry bit in the status register (treated as 0 or 1) and the one's complement of \<subtrahend\> are added to the contents of the specified register. The result is then stored in the specified register. This instruction is used for multiple precision arithmetic.

**Operands:** 1
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/4 | 0xFE44 | W | 1 | LOCAL, RECORD, CONSTANT... |
| 2/4 | 0xFE45 | W | 2 | LOCAL, RECORD, CONSTANT... |
| 3/4 | 0xFE46 | W | 3 | LOCAL, RECORD, CONSTANT... |
| 4/4 | 0xFE47 | W | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for subc
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.18
**Title:** Subtract with carry

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
