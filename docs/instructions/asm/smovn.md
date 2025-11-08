# SMOVN - Smovn

## Overview

**Mnemonic:** `smovn`
**Function:** Smovn
**Class:** STRING
**Privilege:** user

**Format:** `{prefix}{register} SMOVN <operands>`

---

## Description

M items are moved from the ⟨source⟩ to the ⟨dest⟩ operand, unless the end of the ⟨source⟩ operand is reached or the ⟨dest⟩ operand full. Overlap is taken care of.

**Operands:** 3
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0xFD76 | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 2/6 | 0xFD77 | BI | 2 | LOCAL, RECORD, CONSTANT... |
| 3/6 | 0xFD78 | BI | 3 | LOCAL, RECORD, CONSTANT... |
| 4/6 | 0xFD79 | BI | 4 | LOCAL, RECORD, CONSTANT... |
| 5/6 | 0xFD7A | BI | 1 | LOCAL, RECORD, CONSTANT... |
| 6/6 | 0xFD7B | BI | 2 | LOCAL, RECORD, CONSTANT... |

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

### Operand 2

[Description for operand 2]

**Supported modes:**
- **LOCAL**
- **RECORD**
- **CONSTANT**
- **REGISTER**
- **PRE_INDEXED**
- **ABSOLUTE**

### Operand 3

[Description for operand 3]

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
        ; Example for smovn
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §14.7
**Title:** String move n elements

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
