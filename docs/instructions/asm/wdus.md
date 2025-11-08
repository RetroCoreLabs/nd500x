# WDUS - Wdus

## Overview

**Mnemonic:** `wdus`
**Function:** Wdus
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} WDUS <operand>`

---

## Description

WDUS instruction

**Operands:** 1
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/12 | 0xFEC0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 2/12 | 0xFEC1 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 3/12 | 0xFEC2 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 4/12 | 0xFEC3 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 5/12 | 0xFED0 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 6/12 | 0xFED0 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 7/12 | 0xFED1 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 8/12 | 0xFED1 | BY | 4 | LOCAL, RECORD, CONSTANT... |
| 9/12 | 0xFED2 | BY | 1 | LOCAL, RECORD, CONSTANT... |
| 10/12 | 0xFED2 | BY | 2 | LOCAL, RECORD, CONSTANT... |
| 11/12 | 0xFED3 | BY | 3 | LOCAL, RECORD, CONSTANT... |
| 12/12 | 0xFED3 | BY | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for wdus
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §TBD
**Title:** Write data unsynchronized (not documented in manual)

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
