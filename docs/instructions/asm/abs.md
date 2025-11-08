# ABS - Abs

## Overview

**Mnemonic:** `abs`
**Function:** Abs
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `ABS`

---

## Description

The absolute value of the contents of the specified register is calculated and stored in the same register. When the datatype is either BY or H, the result is stored in the least significant bits and the rest of the register is cleared. Overflow occurs if and only if the greatest negative integer is negated.

**Operands:** 0
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0xFF00 | BY | 1 | ALL |
| 2/20 | 0xFF01 | BY | 2 | ALL |
| 3/20 | 0xFF02 | BY | 3 | ALL |
| 4/20 | 0xFF03 | BY | 4 | ALL |
| 5/20 | 0xFF04 | BY | 1 | ALL |
| 6/20 | 0xFF05 | BY | 2 | ALL |
| 7/20 | 0xFF06 | BY | 3 | ALL |
| 8/20 | 0xFF07 | BY | 4 | ALL |
| 9/20 | 0xFF08 | BY | 1 | ALL |
| 10/20 | 0xFF09 | BY | 2 | ALL |
| 11/20 | 0xFF0A | BY | 3 | ALL |
| 12/20 | 0xFF0B | BY | 4 | ALL |
| 13/20 | 0xFF0C | BY | 1 | ALL |
| 14/20 | 0xFF0C | BY | 2 | ALL |
| 15/20 | 0xFF0D | BY | 3 | ALL |
| 16/20 | 0xFF0D | BY | 4 | ALL |
| 17/20 | 0xFF0E | BY | 1 | ALL |
| 18/20 | 0xFF0E | BY | 2 | ALL |
| 19/20 | 0xFF0F | BY | 3 | ALL |
| 20/20 | 0xFF0F | BY | 4 | ALL |

---

## Operands

This instruction takes no operands.
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
        ; Example for abs
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.15
**Title:** Absolute value

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
