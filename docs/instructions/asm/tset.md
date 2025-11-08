# TSET - Tset

## Overview

**Mnemonic:** `tset`
**Function:** Tset
**Class:** CONTROL
**Privilege:** user

**Format:** `{prefix}{register} TSET <operand>`

---

## Description

The TSET instruction performs the two necessary memory accesses uninterruptible by other processors or by channels connected to the memory system. It may therefore be used to implement processor synchronization. The TSET instruction always reads the contents of main memory, even if the addressed data are present in cache memory. The cache is updated for later references by ordinary load instructions. The TSET instruction is valid in the MPM-IV and later memory systems. In installations using MPM

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

Total variants: 1

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/1 | 0xFD40 | BY | 1 | LOCAL, RECORD, REGISTER... |

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
        ; Example for tset
        ; [Variant data not available]
```

---

## Performance Notes

- **Typical cycles:** [To be determined]
- **Best case:** [To be determined]
- **Worst case:** [To be determined]

---

## Reference Manual

**Section:** §16.3
**Title:** Test and set

---

## See Also

- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
