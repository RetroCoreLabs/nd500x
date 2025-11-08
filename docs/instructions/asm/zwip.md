# ZWIP - Zwip

## Overview

**Mnemonic:** `zwip`
**Function:** Zwip
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} ZWIP <operand>`

---

## Description

Privileged instruction. The specified bit in the Written In Page table is cleared. This instruction is used by the swapper routines after a new page has been read from disk into physical memory. In hardware there are separate WIP tables for program and data. ZWIP will clear both tables. Consequently, an ND-500 system cannot have physically separate memory for program and data at the same physical addresses. This instruction is installation dependent; using it requires knowledge of the physical m

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFE9C | BI | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for zwip
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.18
**Title:** Clear written in page bit

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
