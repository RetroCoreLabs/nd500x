# UMUL - Unsigned Multiply with Overflow to Register

## Overview

**Mnemonic:** `umul`
**Function:** Unsigned multiply with overflow to register
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `Wn UMUL <a>,<b>,<c>`

---

## Description

Treats operands as unsigned and multiplies the `<a>` operand by the `<b>` operand, storing the lower half of the product in the `<c>` operand (destination). The upper half of the double-length result is stored in the specified register Rn.

**Key Characteristics:**
- Unsigned 32-bit multiplication with 64-bit result
- Full double-length product access (no overflow loss)
- Word-only instruction (no BY, H, F, D variants)
- Essential for cryptography and bignum arithmetic
- Overflow when upper half ≠ 0
- 4 register variants (W1-W4)
- Treats all operands as unsigned (0 to 4,294,967,295)
- Critical for multi-precision unsigned operations

This instruction is specifically designed for unsigned arithmetic. Byte and halfword integer constants are sign extended, and the result of the sign extension is treated as unsigned. Integer overflow occurs when the upper part is different from zero.

Only word (W) type is supported - no byte, halfword, or floating point variants. This instruction is essential for multi-precision unsigned arithmetic and cryptographic operations.

**Operands:** 3
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4 (word type × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/4 | 0xFC80 | W | 1 | W1 UMUL |
| 2/4 | 0xFC81 | W | 2 | W2 UMUL |
| 3/4 | 0xFC82 | W | 3 | W3 UMUL |
| 4/4 | 0xFC83 | W | 4 | W4 UMUL |

---

## Operands

### Operand 1 (Multiplicand)

The first source operand (unsigned multiplicand). This operand is not modified.

**Type:** Word (treated as unsigned)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value (sign-extended then treated as unsigned)
- **REGISTER** - Integer register (I1-I4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Multiplier)

The second source operand (unsigned multiplier). This operand is not modified.

**Type:** Word (treated as unsigned)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value (sign-extended then treated as unsigned)
- **REGISTER** - Integer register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Product Lower Half/Destination)

The destination operand where the lower half of the unsigned product is stored. The upper half goes to register Rn.

**Type:** Word
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
- **Integer overflow (O):** Upper part ≠ 0

---

## Data Status Bits

- **Z (Zero):** Set if product = 0, cleared otherwise
- **S (Sign):** Set to sign bit of product
- **O (Overflow):** Set if upper part ≠ 0

---

## Examples

### Example 1: Unsigned multiplication

```assembly
        % Multiply LEASTX by LEASTY, result in R2, upper half in R1
        W1 UMUL B.LEASTX, B.LEASTY, R2
```

### Example 2: Large number multiplication

```assembly
        % Multiply two unsigned 32-bit values
        W2 UMUL B.UNSIGNED_A, B.UNSIGNED_B, B.PROD_LOW
        % R2 contains upper 32 bits, PROD_LOW contains lower 32 bits
```

### Example 3: Multi-precision arithmetic

```assembly
        % Part of 64-bit × 64-bit multiplication
        W3 UMUL B.LOW_A, B.LOW_B, B.PARTIAL_PROD
        % R3 contains carry for next part
```

### Example 4: Overflow detection

```assembly
        % Check if unsigned multiply overflows 32 bits
        W1 UMUL B.VALUE_A, B.VALUE_B, B.RESULT
        W COMP R1, 0
        IF><GO OVERFLOW_OCCURRED
```

---

## Performance Notes

- **Typical cycles:** 6-9 cycles depending on addressing modes
- **Best case:** 6 cycles (register to register)
- **Worst case:** 9+ cycles (memory to memory with page fault)

**Note:** UMUL has similar performance to MUL4 but handles unsigned operands correctly.

---

## Reference Manual

**Section:** §11.15
**Title:** Unsigned multiply with overflow to register

---

## See Also

- [MUL4](mul4.md) - Signed multiply with overflow to register
- [UDIV](udiv.md) - Unsigned divide with remainder
- [MUL2](mul2.md) - Multiply two operands (signed)
- [MUL3](mul3.md) - Multiply three operands (signed)
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
