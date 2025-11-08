# GO - Go

## Overview

**Mnemonic:** `go`
**Function:** Go
**Class:** BRANCH
**Privilege:** user

**Format:** `{prefix}{register} GO <operand>`

---

## Description

Perform a jump relative to the current program counter value. GO uses a direct operand and has three formats, with a byte, halfword, or word displacement part. The displacement is signed and is found in the 1, 2 or 4 bytes following the instruction code.

**Operands:** 1
**Variants:** 3 opcode(s)

---

## Variants

Total variants: 3

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/3 | 0x00C0 | - | 1 | ALL |
| 2/3 | 0x00C1 | - | 2 | ALL |
| 3/3 | 0x00C2 | - | 3 | ALL |

---

## Operands

### Operand 1

[Description for operand 1]

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
        ; Example for go
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §13.1
**Title:** Unconditional jump

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
