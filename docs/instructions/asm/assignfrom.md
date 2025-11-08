# =: - AssignFrom

## Overview

**Mnemonic:** `=:`
**Function:** AssignFrom
**Class:** MOVE
**Privilege:** user

**Format:** `{prefix}{register} =: <operand>`

---

## Description

[Description for =: instruction to be written based on Reference Manual §10.4]

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0x001C | BI | 1 | LOCAL, RECORD, REGISTER... |
| 2/24 | 0x001D | BI | 2 | LOCAL, RECORD, REGISTER... |
| 3/24 | 0x001E | BI | 3 | LOCAL, RECORD, REGISTER... |
| 4/24 | 0x001F | BI | 4 | LOCAL, RECORD, REGISTER... |
| 5/24 | 0x0020 | BI | 1 | LOCAL, RECORD, REGISTER... |
| 6/24 | 0x0021 | BI | 2 | LOCAL, RECORD, REGISTER... |
| 7/24 | 0x0022 | BI | 3 | LOCAL, RECORD, REGISTER... |
| 8/24 | 0x0023 | BI | 4 | LOCAL, RECORD, REGISTER... |
| 9/24 | 0x0024 | BI | 1 | LOCAL, RECORD, REGISTER... |
| 10/24 | 0x0025 | BI | 2 | LOCAL, RECORD, REGISTER... |
| 11/24 | 0x0026 | BI | 3 | LOCAL, RECORD, REGISTER... |
| 12/24 | 0x0027 | BI | 4 | LOCAL, RECORD, REGISTER... |
| 13/24 | 0x0028 | BI | 1 | LOCAL, RECORD, REGISTER... |
| 14/24 | 0x0029 | BI | 2 | LOCAL, RECORD, REGISTER... |
| 15/24 | 0x002A | BI | 3 | LOCAL, RECORD, REGISTER... |
| 16/24 | 0x002B | BI | 4 | LOCAL, RECORD, REGISTER... |
| 17/24 | 0xFC0C | BI | 1 | LOCAL, RECORD, REGISTER... |
| 18/24 | 0xFC0D | BI | 2 | LOCAL, RECORD, REGISTER... |
| 19/24 | 0xFC0E | BI | 3 | LOCAL, RECORD, REGISTER... |
| 20/24 | 0xFC0F | BI | 4 | LOCAL, RECORD, REGISTER... |
| 21/24 | 0xFC10 | BI | 1 | LOCAL, RECORD, REGISTER... |
| 22/24 | 0xFC11 | BI | 2 | LOCAL, RECORD, REGISTER... |
| 23/24 | 0xFC12 | BI | 3 | LOCAL, RECORD, REGISTER... |
| 24/24 | 0xFC13 | BI | 4 | LOCAL, RECORD, REGISTER... |

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
        ; Example usage of =:
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.4
**Title:** Store

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
