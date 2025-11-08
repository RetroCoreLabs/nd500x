# RDUS - Rdus

## Overview

**Mnemonic:** `rdus`
**Function:** Rdus
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} RDUS <operand>`

---

## Description

Data in the data cache are marked as invalid. Data marked 'dirty' is dumped to memory. In connection with DMA transfers, the cache should be cleared to ensure that the cache contents are consistent with the main memory contents. If no cache is present, the instruction has no effect.

**Operands:** 1
**Variants:** 16 opcode(s)

---

## Variants

Total variants: 16

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/16 | 0xFEA0 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/16 | 0xFEA1 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/16 | 0xFEA2 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/16 | 0xFEA3 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/16 | 0xFEA4 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/16 | 0xFEA5 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/16 | 0xFEA6 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/16 | 0xFEA7 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 9/16 | 0xFEA8 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 10/16 | 0xFEA9 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 11/16 | 0xFEAA | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 12/16 | 0xFEAB | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 13/16 | 0xFEAC | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 14/16 | 0xFEAD | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 15/16 | 0xFEAE | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 16/16 | 0xFEAF | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for rdus
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.10
**Title:** Read user status

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
