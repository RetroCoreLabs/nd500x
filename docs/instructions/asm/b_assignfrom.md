# B:= - Load local base

## Overview

**Mnemonic:** `bassignfrom`
**Function:** Load local base
**Class:** MOVE
**Privilege:** user

**Format:** `<dest> B:= <source>` or `<source> B:= <dest>`

---

## Description

The local base load operator (`B:=`) loads a value into the B (local/base) register from memory. The B register typically points to the current stack frame or data segment, and loading it is essential for context restoration and frame switching.

**Operation:**
```
<source> → B
```

**Key Characteristics:**
- Loads new local base frame pointer
- Essential for context restoration
- Used in function returns and context switches
- Enables dynamic frame switching
- Critical for stack frame management
- Common in function epilogues and exception handlers

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
        B:= B.FRAME
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

**Section:** §10.2
**Title:** Load local base

---

## See Also

- [MOVE](move.md) - Move data
- [SWAP](swap.md) - Swap operands
- [Addressing Modes](../AddressingModes.md)
