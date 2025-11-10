# ADDC - Add with Carry

## Overview

**Mnemonic:** `addc`
**Function:** Add with carry (multi-precision arithmetic)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `Wn ADDC <addend>`

---

## Description

Adds the `<addend>` operand, the carry bit from the status register (treated as 0 or 1), and the contents of the specified word register, storing the result back in the register.

The operation is: `Rn = Rn + C + <addend>`

This instruction is specifically designed for multi-precision arithmetic, allowing addition of numbers larger than 32 bits by chaining multiple ADDC operations together. The carry bit from one operation automatically propagates to the next, enabling addition of 64-bit, 128-bit, or larger integers.

**Important:** Only available for word (W) data type and operates on registers I1-I4.

**Operands:** 1
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4 (one per register, word-only)

| Variant | Opcode | Register | Data Type |
|---------|--------|----------|-----------|
| 1/4 | 0xFE40 | 1 | Word (32-bit) |
| 2/4 | 0xFE41 | 2 | Word (32-bit) |
| 3/4 | 0xFE42 | 3 | Word (32-bit) |
| 4/4 | 0xFE43 | 4 | Word (32-bit) |

---

## Operands

### Operand 1 (Addend)

The value to add to the register (along with carry bit).

**Type:** Word (32-bit)
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
- **Integer overflow (O):** Signed integer addition overflow

---

## Data Status Bits

- **Z (Zero):** Set if sum = 0, cleared otherwise
- **S (Sign):** Set to sign bit of sum (bit 31)
- **O (Overflow):** Set if integer overflow occurs
- **C (Carry):** Set if carry from most significant bit (bit 31)
- **K (Flag):** Unaffected

**Note:** The carry bit is both an input (added to the sum) and an output (set by the result).

---

## Examples

### Example 1: Simple add with carry

```assembly
        % Add variable MOST to W2 with carry
        W2 ADDC MOST
```

### Example 2: 64-bit addition

```assembly
        % Add two 64-bit integers: (A_HI:A_LO) + (B_HI:B_LO) -> (RESULT_HI:RESULT_LO)

        % Load first 64-bit value
        I1 := B.A_LO             % Low 32 bits
        I2 := B.A_HI             % High 32 bits

        % Add low words (clears carry initially)
        W ADD2 I1, B.B_LO        % I1 = A_LO + B_LO

        % Add high words with carry from low addition
        W2 ADDC B.B_HI           % I2 = A_HI + B_HI + carry

        % Store result
        I1 =: B.RESULT_LO
        I2 =: B.RESULT_HI
```

### Example 3: 128-bit addition chain

```assembly
        % Add two 128-bit integers (4 words each)

        % Load first value
        I1 := B.VALUE_A_WORD0
        I2 := B.VALUE_A_WORD1
        I3 := B.VALUE_A_WORD2
        I4 := B.VALUE_A_WORD3

        % Add word 0 (least significant)
        W ADD2 I1, B.VALUE_B_WORD0

        % Chain additions with carry propagation
        W2 ADDC B.VALUE_B_WORD1
        W3 ADDC B.VALUE_B_WORD2
        W4 ADDC B.VALUE_B_WORD3  % Most significant word

        % Store 128-bit result
        I1 =: B.RESULT_WORD0
        I2 =: B.RESULT_WORD1
        I3 =: B.RESULT_WORD2
        I4 =: B.RESULT_WORD3
```

### Example 4: Multi-precision array addition

```assembly
        % Add two multi-precision numbers stored as arrays
        % ARRAY_A + ARRAY_B -> ARRAY_RESULT (each N words long)

        W1 := 0                  % Initialize index

        % Add first word (no carry in)
        W2 := B.ARRAY_A(I1)
        W ADD2 I2, B.ARRAY_B(I1)
        I2 =: B.ARRAY_RESULT(I1)

        W ADD2 I1, 4             % Next word

LOOP:
        % Add subsequent words with carry
        W2 := B.ARRAY_A(I1)
        W2 ADDC B.ARRAY_B(I1)    % Includes carry from previous
        I2 =: B.ARRAY_RESULT(I1)

        W ADD2 I1, 4
        W COMP I1, B.ARRAY_SIZE
        IF<GO LOOP
```

---

## Performance Notes

- **Typical cycles:** 4-5 cycles depending on addressing mode
- **Best case:** 4 cycles (register to register with carry)
- **Worst case:** 5+ cycles (memory access with page fault)

**Note:** Essential for implementing bignum arithmetic libraries and cryptographic operations requiring >32-bit precision.

---

## Reference Manual

**Section:** §11.17
**Title:** Add with carry

---

## See Also

- [SUBC](subc.md) - Subtract with carry (multi-precision)
- [ADD2](add2.md) - Add two operands (normal addition)
- [ADD3](add3.md) - Add three operands
- [UMUL](umul.md) - Unsigned multiply (produces 64-bit result)
- [UDIV](udiv.md) - Unsigned divide (multi-precision)
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
