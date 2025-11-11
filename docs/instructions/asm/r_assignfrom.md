# R:= - Load record base

## Overview

**Mnemonic:** `rassignfrom`
**Function:** Load record base
**Class:** MOVE
**Privilege:** user

**Format:** `<dest> R:= <source>` or `<source> R:= <dest>`

---

## Description

The record base load operator (`R:=`) loads a value into the R (record) register from memory. The R register typically points to the current record structure or object, enabling efficient field access in structured data.

**Operation:**
```
<source> → R
```

**Key Characteristics:**
- Loads new record base pointer
- Essential for record context switching
- Used in object-oriented patterns
- Enables efficient structure manipulation
- Critical for record-based addressing
- Common in data structure traversal and field access

**Operands:** 2
**Variants:** Multiple (all data types)

---

## Variants

Supports all data types (BY, H, W, F, D) and addressing modes.

---

## Operands

### Operands

Source and destination as specified by operator direction.

---

## Trap Conditions

- **Addressing traps:** Invalid address

---

## Data Status Bits

Varies by data type.

---

## Examples

### Example 1: Basic usage

```assembly
        R:= B.RECORD
```

### Example 2: Array access

```assembly
        I1 := B.ARRAY(I2)
```

### Example 3: Record field

```assembly
        I1 := R.FIELD
```

---

## Performance Notes

- **Cycles:** 2-4

---

## Reference Manual

**Section:** §10.3
**Title:** Load record base

---

## See Also

- [MOVE](move.md) - Move data
- [SWAP](swap.md) - Swap operands
- [Addressing Modes](../AddressingModes.md)
