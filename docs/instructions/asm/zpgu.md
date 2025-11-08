# ZPGU - Zpgu

## Overview

**Mnemonic:** `zpgu`
**Function:** Zpgu
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} ZPGU <operand>`

---

## Description

Privileged instruction. The specified bit in the page used table is cleared. This instruction is used by the swapper routines after a new page has been read from disk into physical memory. In hardware there are separate PGU tables for program and data. ZPGU will clear the specified bit in both tables. Consequently, an ND-500 system cannot have physically separate memory for program and data at the same physical address. This instruction is installation dependent; using it requires knowledge of t

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFE90 | BI | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for zpgu
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.21
**Title:** Clear page used bit

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
