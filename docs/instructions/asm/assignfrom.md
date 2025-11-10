# := - Load

## Overview

**Mnemonic:** `assignfrom`
**Function:** Load
**Class:** MOVE
**Privilege:** user

**Format:** `<dest> := <source>` or `<source> := <dest>`

---

## Description

Load operand into register. Fundamental data movement operation in ND-500 assembly.

**Operation:** Transfers data between operands.

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
        I1 := B.VALUE
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

**Section:** §10.1
**Title:** Load

---

## See Also

- [MOVE](move.md) - Move data
- [SWAP](swap.md) - Swap operands
- [Addressing Modes](../AddressingModes.md)
