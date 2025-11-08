# CALL - Call

## Overview

**Mnemonic:** `call`
**Function:** Call
**Class:** CALL
**Privilege:** user

**Format:** `{prefix}{register} CALL <op1>,<op2>`

---

## Description

Call the subroutine specified by `<subr. addr.>`. This is a general operand and it *must* refer to an entry point instruction. Otherwise an instruction-sequence error-trap condition occurs. The `<no of arg>` operand must be a constant byte integer less than 256. Other data types which are not constants will cause an illegal operand specifier trap condition. The effective addresses of the arguments in the instruction are calculated and stored for use by the entry point instruction. The arguments 

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0x00C3 | - | 1 | ALL |

---

## Operands

### Operand 1

[Description for operand 1]

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
        ; Example for call
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.7
**Title:** Call subroutine general

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
