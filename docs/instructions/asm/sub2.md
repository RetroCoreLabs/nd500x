# SUB2 - Sub2

## Overview

**Mnemonic:** `sub2`
**Function:** Sub2
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} SUB2 <op1>,<op2>`

---

## Description

[Description for SUB2 instruction to be written based on Reference Manual §11.6]

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0x00E0 | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/5 | 0xFC58 | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/5 | 0xFC59 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/5 | 0xFC5B | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/5 | 0xFC5C | BY | 1 | LOCAL, RECORD, REGISTER... |

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
        ; Example usage of SUB2
        ; [To be written]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §11.6
**Title:** Subtract two operands

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
