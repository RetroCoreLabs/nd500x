# ENTB - Entb

## Overview

**Mnemonic:** `entb`
**Function:** Entb
**Class:** CALL
**Privilege:** user

**Format:** `{prefix}{register} ENTB <operand>`

---

## Description

When the ENTM entry point is used, a new stack is initialized. A value of <stack demand of main program> greater than or equal to <total system stack demand> will cause a stack overflow trap condition. If ENTM is entered from another domain, TOS is not saved on the old stack, but is stored in the domain information table. Also THA, LL and HL are stored and new contents for these registers are fetched from the new domain information table. ENTM is the only entry point that may be called from anot

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0x00BD | - | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for entb
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.10
**Title:** Enter block

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
