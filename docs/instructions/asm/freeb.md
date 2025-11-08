# FREEB - Freeb

## Overview

**Mnemonic:** `freeb`
**Function:** Freeb
**Class:** SYSTEM
**Privilege:** user

**Format:** `{prefix}{register} FREEB <op1>,<op2>`

---

## Description

The specified `<element>` is appended to the appropriate freelist of the heap. Elements are not combined; this may be done by a trap handler for the stack overflow condition. The administration of the heap is described in section 3.3. When executing the FREEB instruction, the TOS register must point to the variables describing the heap. Write access to the `<element>` is required, but if `<element>` is addressed with a DESC prefix, the index register is not updated.

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFDB6 | - | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for freeb
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §15.14
**Title:** Free buddy element

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
