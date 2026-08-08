# R=: - Store record base

## Overview

**Mnemonic:** `rassignto`
**Function:** Store record base
**Class:** MOVE
**Privilege:** user

**Format:** `<dest> R=: <source>` or `<source> R=: <dest>`

---

## Description

The record base store operator (`R=:`) stores the R (record) register to memory. The R register typically points to the current record structure or object, enabling efficient field access in structured data.

**Operation:**
```
R → <destination>
```

**Key Characteristics:**
- Stores current record base pointer
- Essential for nested record access
- Used in object-oriented patterns
- Enables efficient structure manipulation
- Critical for record-based addressing
- Common in data structure traversal and debugging

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
        R=: B.SAVE
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

**Section:** §10.6
**Title:** Store record base

---

## See Also

- [MOVE](move.md) - Move data
- [SWAP](swap.md) - Swap operands
