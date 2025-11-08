# BLADDR - Bladdr

## Overview

**Mnemonic:** `bladdr`
**Function:** Bladdr
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} BLADDR <operand>`

---

## Description

The address of the operand is loaded into the local base register. Registers and constants have no address in memory and are illegal as operands.

**Operands:** 1
**Variants:** 6 opcode(s)

---

## Variants

Total variants: 6

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/6 | 0xFCB3 | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 2/6 | 0xFCBC | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |
| 3/6 | 0xFD37 | BI | 3 | LOCAL, RECORD, PRE_INDEXED... |
| 4/6 | 0xFD38 | BI | 4 | LOCAL, RECORD, PRE_INDEXED... |
| 5/6 | 0xFD63 | BI | 1 | LOCAL, RECORD, PRE_INDEXED... |
| 6/6 | 0xFD63 | BI | 2 | LOCAL, RECORD, PRE_INDEXED... |

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
        ; Example for bladdr
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §15.6
**Title:** Load address into base register

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
