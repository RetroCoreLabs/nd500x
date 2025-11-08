# INV - Inv

## Overview

**Mnemonic:** `inv`
**Function:** Inv
**Class:** LOGICAL
**Privilege:** user

**Format:** `INV`

---

## Description

The one's complement of the contents of the specified register is calculated and stored in the same register. When the datatype is BI, BY, or H only the lower part of the register is complemented and the rest of the register is cleared.

**Operands:** 0
**Variants:** 16 opcode(s)

---

## Variants

Total variants: 16

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/16 | 0x0098 | BI | 1 | ALL |
| 2/16 | 0x0099 | BI | 2 | ALL |
| 3/16 | 0x009A | BI | 3 | ALL |
| 4/16 | 0x009B | BI | 4 | ALL |
| 5/16 | 0xFE10 | BI | 1 | ALL |
| 6/16 | 0xFE11 | BI | 2 | ALL |
| 7/16 | 0xFE12 | BI | 3 | ALL |
| 8/16 | 0xFE13 | BI | 4 | ALL |
| 9/16 | 0xFE14 | BI | 1 | ALL |
| 10/16 | 0xFE15 | BI | 2 | ALL |
| 11/16 | 0xFE16 | BI | 3 | ALL |
| 12/16 | 0xFE17 | BI | 4 | ALL |
| 13/16 | 0xFE18 | BI | 1 | ALL |
| 14/16 | 0xFE19 | BI | 2 | ALL |
| 15/16 | 0xFE1A | BI | 3 | ALL |
| 16/16 | 0xFE1B | BI | 4 | ALL |

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
        ; Example for inv
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.13
**Title:** Invert

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
