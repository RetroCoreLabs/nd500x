# STZ - Stz

## Overview

**Mnemonic:** `stz`
**Function:** Stz
**Class:** MOVE
**Privilege:** user

**Format:** `{prefix}{register} STZ <operand>`

---

## Description

The contents of the destination operand are replaced by zero.

**Operands:** 1
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0x0048 | BI | 1 | LOCAL, RECORD, REGISTER... |
| 2/6 | 0x0049 | BI | 2 | LOCAL, RECORD, REGISTER... |
| 3/6 | 0x004A | BI | 3 | LOCAL, RECORD, REGISTER... |
| 4/6 | 0x004B | BI | 4 | LOCAL, RECORD, REGISTER... |
| 5/6 | 0x004C | BI | 1 | LOCAL, RECORD, REGISTER... |
| 6/6 | 0xFC85 | BI | 2 | LOCAL, RECORD, REGISTER... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
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
        ; Example for stz
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.17
**Title:** Store zero

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
