# LADDR - Laddr

## Overview

**Mnemonic:** `laddr`
**Function:** Laddr
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} LADDR <operand>`

---

## Description

[Description for LADDR instruction to be written based on Reference Manual §15.4]

**Operands:** 1
**Variants:** 24 opcode(s)

---

## Variants

Total variants: 24

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/24 | 0xFD3C | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 2/24 | 0xFD3C | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 3/24 | 0xFD3D | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 4/24 | 0xFD3D | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |
| 5/24 | 0xFD3E | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 6/24 | 0xFD3E | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 7/24 | 0xFD3F | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 8/24 | 0xFD3F | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |
| 9/24 | 0xFE20 | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 10/24 | 0xFE21 | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 11/24 | 0xFE22 | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 12/24 | 0xFE23 | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |
| 13/24 | 0xFE24 | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 14/24 | 0xFE25 | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 15/24 | 0xFE26 | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 16/24 | 0xFE27 | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |
| 17/24 | 0xFE28 | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 18/24 | 0xFE29 | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 19/24 | 0xFE2A | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 20/24 | 0xFE2B | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |
| 21/24 | 0xFE2C | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 22/24 | 0xFE2D | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 23/24 | 0xFE2E | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 24/24 | 0xFE2F | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
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
        ; Example usage of LADDR
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §15.4
**Title:** Load address

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
