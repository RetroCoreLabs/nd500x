# RWIP - Rwip

## Overview

**Mnemonic:** `rwip`
**Function:** Rwip
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} RWIP <operand>`

---

## Description

Privileged instruction. A bit or 16 bit group is read from the Written In Page table into the specified register. The operand specifies the physical memory page number (BIn RWIP) or physical page number/16 (Hn RWIP). A bit set in this table indicates that the page has been written into and must be written back to disk before being replaced with another one. The bit is automatically set by hardware and is used by the swapper routines. In hardware there are separate WIP tables for program and data

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/8 | 0xFE94 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/8 | 0xFE95 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/8 | 0xFE96 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/8 | 0xFE97 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/8 | 0xFE98 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/8 | 0xFE99 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 7/8 | 0xFE9A | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 8/8 | 0xFE9B | BI | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for rwip
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.17
**Title:** Read Written In Page table

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
