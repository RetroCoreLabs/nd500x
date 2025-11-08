# CLR - Clr

## Overview

**Mnemonic:** `clr`
**Function:** Clr
**Class:** MOVE
**Privilege:** user

**Format:** `CLR`

---

## Description

The register is set to all zeroes. For all integer data types, the entire register is cleared.

**Operands:** 0
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0x0084 | BI | 1 | ALL |
| 2/24 | 0x0084 | BI | 2 | ALL |
| 3/24 | 0x0084 | BI | 3 | ALL |
| 4/24 | 0x0084 | BI | 4 | ALL |
| 5/24 | 0x0085 | BI | 1 | ALL |
| 6/24 | 0x0085 | BI | 2 | ALL |
| 7/24 | 0x0085 | BI | 3 | ALL |
| 8/24 | 0x0085 | BI | 4 | ALL |
| 9/24 | 0x0086 | BI | 1 | ALL |
| 10/24 | 0x0086 | BI | 2 | ALL |
| 11/24 | 0x0086 | BI | 3 | ALL |
| 12/24 | 0x0086 | BI | 4 | ALL |
| 13/24 | 0x0087 | BI | 1 | ALL |
| 14/24 | 0x0087 | BI | 2 | ALL |
| 15/24 | 0x0087 | BI | 3 | ALL |
| 16/24 | 0x0087 | BI | 4 | ALL |
| 17/24 | 0x0088 | BI | 1 | ALL |
| 18/24 | 0x0089 | BI | 2 | ALL |
| 19/24 | 0x008A | BI | 3 | ALL |
| 20/24 | 0x008B | BI | 4 | ALL |
| 21/24 | 0x008C | BI | 1 | ALL |
| 22/24 | 0x008D | BI | 2 | ALL |
| 23/24 | 0x008E | BI | 3 | ALL |
| 24/24 | 0x008F | BI | 4 | ALL |

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
        ; Example for clr
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.16
**Title:** Clear register

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
