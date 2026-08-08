# SUBC - Subtract with Carry

## Overview

**Mnemonic:** `subc`
**Function:** Subtract with carry (multi-precision arithmetic)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `Wn SUBC <subtrahend>`

---

## Description

Adds the carry bit in the status register (treated as 0 or 1) and the one's complement of `<subtrahend>` to the contents of the specified register Rn. The result is then stored in the specified register.

**Operation:**
```
Rn = Rn + C + ~<subtrahend>
```

**Key Characteristics:**
- Multi-precision subtraction with carry propagation
- Only word (W) type supported (4 register variants)
- Essential for 64-bit, 96-bit, 128-bit arithmetic
- Carry flag propagates borrow across word boundaries
- Follows initial SUB2 for low-order word
- 4-6 cycles execution time
- Sets Z, S, C, O flags for chaining
- Common in bignum and cryptographic operations
- Similar performance to SUB2

Adds the carry bit in the status register (treated as 0 or 1) and the one's complement of `<subtrahend>` to the contents of the specified register Rn. The result is then stored in the specified register.

Operation: `Rn = Rn + C + ~<subtrahend>` (where ~ is one's complement)

This instruction is specifically designed for multi-precision (extended precision) subtraction operations. It allows chaining multiple subtract operations to handle numbers larger than 32 bits.

Only word (W) type is supported - no byte, halfword, or floating point variants.

**Operands:** 1
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4 (word type × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/4 | 0xFE44 | W | 1 | W1 SUBC |
| 2/4 | 0xFE45 | W | 2 | W2 SUBC |
| 3/4 | 0xFE46 | W | 3 | W3 SUBC |
| 4/4 | 0xFE47 | W | 4 | W4 SUBC |

---

## Operands

### Operand 1 (Subtrahend)

The operand to subtract from the specified register (with carry).

**Type:** Word
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Integer overflow (O):** Signed integer subtraction overflow

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of result
- **C (Carry):** Set if carry from most significant bit
- **O (Overflow):** Set if integer overflow

---

## Examples

### Example 1: Subtract with carry

```assembly
        % Subtract 400H from W2 with carry
        W2 SUBC 0400H
```

### Example 2: Multi-precision subtraction (64-bit)

```assembly
        % Subtract 64-bit value (B.HIGH_B:B.LOW_B) from (R2:R1)
        W SUB2 R1, B.LOW_B        % Subtract low parts
        W2 SUBC B.HIGH_B          % Subtract high parts with carry
        % Result: 64-bit difference in R2:R1
```

### Example 3: Triple-precision subtraction

```assembly
        % Subtract 96-bit values
        W SUB2 R1, B.PART0        % Low word
        W2 SUBC B.PART1           % Middle word with carry
        W3 SUBC B.PART2           % High word with carry
        % Result: 96-bit difference in R3:R2:R1
```

### Example 4: Extended precision decrement

```assembly
        % Decrement multi-word counter
        W SUB2 B.COUNTER_LOW, 1
        W2 SUBC 0                 % Propagate borrow to high word
```

---

## Performance Notes

- **Typical cycles:** 4-6 cycles depending on addressing modes
- **Best case:** 4 cycles (immediate constant)
- **Worst case:** 6+ cycles (memory operand with page fault)

**Note:** SUBC is essential for multi-precision arithmetic and has similar performance to SUB2.

---

## Reference Manual

**Section:** §11.18
**Title:** Subtract with carry

---

## See Also

- [ADDC](addc.md) - Add with carry (multi-precision)
- [SUB2](sub2.md) - Subtract two operands
- [SUB3](sub3.md) - Subtract three operands
- [Trap System](../../ND-500-TRAPS.md)
