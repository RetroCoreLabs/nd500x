# DECR - Decr

## Overview

**Mnemonic:** `decr`
**Function:** Decr
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} DECR <operand>`

---

## Description

[Description for DECR instruction to be written based on Reference Manual §10.20]

**Operands:** 1
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0x0051 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/5 | 0xFC8C | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/5 | 0xFC8D | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/5 | 0xFC8E | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/5 | 0xFC8F | BY | 1 | LOCAL, RECORD, REGISTER... |

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
        ; Example usage of DECR
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.20
**Title:** Decrement

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
