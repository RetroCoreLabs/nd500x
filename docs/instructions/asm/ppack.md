# PPACK - Ppack

## Overview

**Mnemonic:** `ppack`
**Function:** Ppack
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} PPACK <op1>,<op2>`

---

## Description

The content of the `<source>` operand in ASCII coded decimal is packed into the `<dest>` operand in packed format. If specified, the value is rounded before storing it in the `<dest>` operand. If bit 26 in the descriptor of the `<dest>` operand is set, the value is stored with a sign code equal to 1111 (unsigned). Otherwise, `<dest>` will be given the sign of the `<source>` value. The `<source>` value consists of ASCII digits and a sign according to the SGN code in the `<source>` descriptor only

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFEB5 | - | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for ppack
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §17.7
**Title:** Convert ASCII to packed

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
