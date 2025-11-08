# POLY - Poly

## Overview

**Mnemonic:** `poly`
**Function:** Poly
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `{prefix}{register} POLY <op1>,<op2>`

---

## Description

This instruction calculates a polynomial of degree <m>. The result is loaded into the specified float or double float register. The instruction requires <m>+1 coefficients. <m> must always be a positive constant less than 256, otherwise an illegal operand specifier trap condition occurs. If floating overflow or underflow occurs, the trap will not have any effect until the instruction has completed execution, even if the trap condition occurred at an intermediate step. The Z and S bits reflect th

**Operands:** 2
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/8 | 0xFCE0 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 2/8 | 0xFCE1 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 3/8 | 0xFCE2 | F | 3 | LOCAL, RECORD, CONSTANT... |
| 4/8 | 0xFCE3 | F | 4 | LOCAL, RECORD, CONSTANT... |
| 5/8 | 0xFCE4 | F | 1 | LOCAL, RECORD, CONSTANT... |
| 6/8 | 0xFCE5 | F | 2 | LOCAL, RECORD, CONSTANT... |
| 7/8 | 0xFCE6 | F | 3 | LOCAL, RECORD, CONSTANT... |
| 8/8 | 0xFCE7 | F | 4 | LOCAL, RECORD, CONSTANT... |

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
- **CONSTANT**

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
        ; Example for poly
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §12.3
**Title:** Polynomial

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
