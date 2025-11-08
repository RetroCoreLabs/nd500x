# CLTE - Clte

## Overview

**Mnemonic:** `clte`
**Function:** Clte
**Class:** CONTROL
**Privilege:** user

**Format:** `{prefix}{register} CLTE <operand>`

---

## Description

The specified bit in the Own Trap Enable register is cleared. An ignorable trap condition will be ignored and no trap handler invoked unless the corresponding MTE bit is set. A non-ignorable trap condition will be propagated to the mother domain. The `<bit no>` operand is compared with a modify mask (TEMM) found in the domain description table. If a bit in this mask is set, the corresponding bit in the local trap-enable register is modifiable. An attempt to modify a non-modifiable bit will cause

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFD3A | - | 1 | LOCAL, RECORD, CONSTANT... |

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
        ; Example for clte
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.6
**Title:** Clear bit in trap enable register

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
