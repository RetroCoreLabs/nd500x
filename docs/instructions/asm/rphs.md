# RPHS - Rphs

## Overview

**Mnemonic:** `rphs`
**Function:** Rphs
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} RPHS <operand>`

---

## Description

Privileged instruction Copy a number of bytes from logical address on physical segment to logical address on the domain. - I1 : Number of bytes to be moved. - I2 : Logical address on the domain. - I3 : Address on the physical segment. - I4 : Physical segment number. Operand : domain number. The copy operation is continued until the number of bytes left is equal to 0 (I1 = 0) or a page boundary is reached on the physical segment. Number of bytes to be moved is counted down and will be zero when t

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFFF5 | - | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for rphs
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.31
**Title:** RPHS - Read from physical segment ('87 extension)

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
