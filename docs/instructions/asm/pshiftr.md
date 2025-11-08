# PSHIFTR - Pshiftr

## Overview

**Mnemonic:** `pshiftr`
**Function:** Pshiftr
**Class:** SHIFT
**Privilege:** user

**Format:** `{prefix}{register} PSHIFTR <op1>,<op2>`

---

## Description

The content of the `<source>` operand is shifted to the scaling factor of the `<dest>` operand and, if specified, rounded before storing it in the `<dest>` operand. The destination string is extended with zeroes if necessary. With the exception of rounding, the value is not modified, but the number of decimal positions may be changed. If the `<source>` and `<dest>` operands have the same scaling factor, a move is performed. If bit 26 in the descriptor of the `<dest>` operand is set, the value is

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFE87 | - | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for pshiftr
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §17.6
**Title:** Packed shift rounded

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
