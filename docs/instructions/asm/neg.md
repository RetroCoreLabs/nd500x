# NEG - Neg

## Overview

**Mnemonic:** `neg`
**Function:** Neg
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `NEG`

---

## Description

The contents of the specified register are negated. An integer value is negated by taking the two's complement of its value. A floating point value is negated by inverting its sign bit. Byte and halfword negate will clear the upper part of the register. Integer overflow occurs if and only if the greatest negative integer is negated. Carry is zero except when integer zero is negated.

**Operands:** 0
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x0090 | BY | 1 | ALL |
| 2/20 | 0x0091 | BY | 2 | ALL |
| 3/20 | 0x0092 | BY | 3 | ALL |
| 4/20 | 0x0093 | BY | 4 | ALL |
| 5/20 | 0x0094 | BY | 1 | ALL |
| 6/20 | 0x0094 | BY | 2 | ALL |
| 7/20 | 0x0095 | BY | 3 | ALL |
| 8/20 | 0x0095 | BY | 4 | ALL |
| 9/20 | 0x0096 | BY | 1 | ALL |
| 10/20 | 0x0096 | BY | 2 | ALL |
| 11/20 | 0x0097 | BY | 3 | ALL |
| 12/20 | 0x0097 | BY | 4 | ALL |
| 13/20 | 0xFE08 | BY | 1 | ALL |
| 14/20 | 0xFE09 | BY | 2 | ALL |
| 15/20 | 0xFE0A | BY | 3 | ALL |
| 16/20 | 0xFE0B | BY | 4 | ALL |
| 17/20 | 0xFE0C | BY | 1 | ALL |
| 18/20 | 0xFE0D | BY | 2 | ALL |
| 19/20 | 0xFE0E | BY | 3 | ALL |
| 20/20 | 0xFE0F | BY | 4 | ALL |

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
        ; Example for neg
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.12
**Title:** Negate

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
