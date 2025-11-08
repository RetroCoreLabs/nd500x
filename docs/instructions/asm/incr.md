# INCR - Incr

## Overview

**Mnemonic:** `incr`
**Function:** Incr
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} INCR <operand>`

---

## Description

The `<operand>` is incremented by one. The Carry bit is set if a carry occurs from the sign bit position of the adder, otherwise it is reset. Carry will occur when and only when integer -1 is incremented.

**Operands:** 1
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/5 | 0x004E | BY | 1 | LOCAL, RECORD, REGISTER... |
| 2/5 | 0x004F | BY | 2 | LOCAL, RECORD, REGISTER... |
| 3/5 | 0x0050 | BY | 3 | LOCAL, RECORD, REGISTER... |
| 4/5 | 0xFC8A | BY | 4 | LOCAL, RECORD, REGISTER... |
| 5/5 | 0xFC8B | BY | 1 | LOCAL, RECORD, REGISTER... |

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
        ; Example for incr
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §10.19
**Title:** Increment

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
