# DIV2 - Div2

## Overview

**Mnemonic:** `div2`
**Function:** Div2
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} DIV2 <op1>,<op2>`

---

## Description

[Description for DIV2 instruction to be written based on Reference Manual §11.8]

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0xFC62 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/5 | 0xFC63 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/5 | 0xFC64 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/5 | 0xFC65 | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/5 | 0xFC66 | BY | 1 | LOCAL, RECORD, REGISTER... |

---

## Operands

### Operand 1

[Description for operand 1]

**Supported modes:**
- **LOCAL**
- **RECORD**
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
        ; Example usage of DIV2
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.8
**Title:** Divide two operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
