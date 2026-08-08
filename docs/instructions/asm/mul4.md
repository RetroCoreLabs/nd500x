# MUL4 - Multiply with Overflow to Register

## Overview

**Mnemonic:** `mul4`
**Function:** Multiply with overflow to register (full double-length product)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn MUL4 <a>,<b>,<c>`

---

## Description

Multiplies the `<a>` operand by the `<b>` operand and stores the lower half of the product in the `<c>` operand (destination). The upper half of the double-length result is stored in the specified register Rn.

**Operation:**
```
<c> = lower_half(<a> * <b>)
Rn = upper_half(<a> * <b>)
```

**Key Characteristics:**
- Four-operand multiplication (includes implicit register)
- Full double-length product access (no precision loss)
- Integer-only (BY, H, W - no float/double support)
- Essential for multi-precision arithmetic
- Upper half enables overflow detection
- 12 variants (3 types × 4 registers)
- Slightly slower than MUL3 (extra register store)
- Critical for cryptography and bignum operations

This instruction provides access to the full double-length product of a multiplication, which is essential for multi-precision arithmetic and overflow detection. Integer overflow occurs if the upper half is not equal to the sign extension of the lower half.

The operands are assumed to have the same data type (BY, H, or W). Only integer types are supported - no floating point variants.

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/12 | 0xFC20 | BY | 1 | BY1 MUL4 |
| 2/12 | 0xFC21 | BY | 2 | BY2 MUL4 |
| 3/12 | 0xFC22 | BY | 3 | BY3 MUL4 |
| 4/12 | 0xFC23 | BY | 4 | BY4 MUL4 |
| 5/12 | 0xFC24 | H | 1 | H1 MUL4 |
| 6/12 | 0xFC25 | H | 2 | H2 MUL4 |
| 7/12 | 0xFC26 | H | 3 | H3 MUL4 |
| 8/12 | 0xFC27 | H | 4 | H4 MUL4 |
| 9/12 | 0xFC28 | W | 1 | W1 MUL4 |
| 10/12 | 0xFC29 | W | 2 | W2 MUL4 |
| 11/12 | 0xFC2A | W | 3 | W3 MUL4 |
| 12/12 | 0xFC2B | W | 4 | W4 MUL4 |

---

## Operands

### Operand 1 (Multiplicand)

The first source operand (multiplicand). This operand is not modified.

**Type:** Byte, Halfword, or Word
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Multiplier)

The second source operand (multiplier). This operand is not modified.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Product Lower Half/Destination)

The destination operand where the lower half of the product is stored. The upper half goes to register Rn.

**Type:** Same data type as operands 1 and 2
**Access:** Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Integer overflow (O):** Upper half ≠ sign extension of lower half

---

## Data Status Bits

- **Z (Zero):** Set if lower part of result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of lower part of result
- **O (Overflow):** Set if integer overflow

---

## Examples

### Example 1: Multiply word arguments

```assembly
        % Multiply M and N, store product in TEMP, overflow in R1
        W1 MUL4 IND(B.M), IND(B.N), B.TEMP
```

### Example 2: Double-precision multiplication

```assembly
        % Multiply two words, get full 64-bit result
        % Lower 32 bits in PROD_LO, upper 32 bits in R2
        W2 MUL4 B.FACTOR_A, B.FACTOR_B, B.PROD_LO
```

### Example 3: Overflow detection

```assembly
        % Multiply with overflow check
        W1 MUL4 B.A, B.B, B.RESULT
        % R1 now contains upper half
        % If R1 = 0 (or -1 for negative), no overflow occurred
```

### Example 4: Multi-precision arithmetic

```assembly
        % First part of multi-precision multiply
        W3 MUL4 B.LOW_A, B.LOW_B, B.LOW_RESULT
        % R3 contains partial carry
```

---

## Performance Notes

- **Typical cycles:** 6-9 cycles depending on addressing modes
- **Best case:** 6 cycles (register to register)
- **Worst case:** 9+ cycles (memory to memory with page fault)

**Note:** MUL4 is slightly slower than MUL3 due to the extra register store operation.

---

## Reference Manual

**Section:** §11.13
**Title:** Multiply with overflow to register

---

## See Also

- [MUL2](mul2.md) - Multiply two operands (destructive)
- [MUL3](mul3.md) - Multiply three operands (non-destructive)
- [UMUL](umul.md) - Unsigned multiply with overflow
- [DIV4](div4.md) - Divide with remainder to register
- [Trap System](../../ND-500-TRAPS.md)
