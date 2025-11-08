# AXI - Axi

## Overview

**Mnemonic:** `axi`
**Function:** Axi
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} AXI <op1>,<op2>`

---

## Description

AXI instruction

**Operands:** 2
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/8 | 0xFCC0 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 2/8 | 0xFCC1 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 3/8 | 0xFCC2 | F | 3 | LOCAL, RECORD, CONSTANT... |
| 4/8 | 0xFCC3 | F | 4 | LOCAL, RECORD, CONSTANT... |
| 5/8 | 0xFCC4 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 6/8 | 0xFCC5 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 7/8 | 0xFCC6 | F | 3 | LOCAL, RECORD, CONSTANT... |
| 8/8 | 0xFCC7 | F | 4 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for axi
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.21
**Title:** Algebraic multiply index

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
