# INIT - Init

## Overview

**Mnemonic:** `init`
**Function:** Init
**Class:** CONTROL
**Privilege:** user

**Format:** `{prefix}{register} INIT <operands>`

---

## Description

The stack is initialized according to the instruction operands: The direct operand <<bottom of stack>> is a 4 byte absolute address, which is loaded into the B register. The B.SP location, the stack pointer, is loaded with the sum of <<bottom of stack>> and &lt;stack demand of main program>. <<bottom of stack>> and &lt;total system stack demand> are added and the result is loaded into the top of stack register, TOS. PREVB and RETA are cleared. A value of &lt;stack demand of main program> greater

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0x00DC | - | 1 | ALL |

---

## Operands

### Operand 1

[Description for operand 1]

### Operand 2

[Description for operand 2]

**Supported modes:**
- **LOCAL**
- **RECORD**
- **CONSTANT**
- **REGISTER**
- **PRE_INDEXED**
- **ABSOLUTE**

### Operand 3

[Description for operand 3]

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
        ; Example for init
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.9
**Title:** Initialize stack

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
