# B=: - Store local base

## Overview

**Mnemonic:** `bassignto`
**Function:** Store local base
**Class:** MOVE
**Privilege:** user

**Format:** `<dest> B=: <source>` or `<source> B=: <dest>`

---

## Description

The local base store operator (`B=:`) stores the B (local/base) register to memory. The B register typically points to the current stack frame or data segment, and storing it is essential for context switching and debugging.

**Operation:**
```
B → <destination>
```

**Key Characteristics:**
- Stores current local base frame pointer
- Essential for context switching and unwinding
- Used in debugging and profiling
- Enables nested function calls
- Critical for stack frame management
- Common in function prologues and exception handlers

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
        B=: B.SAVE
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

**Section:** §10.5
**Title:** Store local base

---

## See Also

- [MOVE](move.md) - Move data
- [SWAP](swap.md) - Swap operands
