# UDIV - Unsigned Divide with Remainder to Register

## Overview

**Mnemonic:** `udiv`
**Function:** Unsigned divide with remainder to register
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `Wn UDIV <a>,<b>,<c>`

---

## Description

Treats operands as unsigned and divides the `<a>` operand by the `<b>` operand, storing the quotient in the `<c>` operand (destination). The remainder is stored in the specified register Rn.

**Key Characteristics:**
- Unsigned 32-bit division with remainder
- Both quotient and remainder in single operation
- Word-only instruction (no BY, H, F, D variants)
- Essential for hash functions and modulo arithmetic
- Divide-by-zero trap (DZ) when divisor = 0
- 4 register variants (W1-W4)
- Treats all operands as unsigned (0 to 4,294,967,295)
- Common in bit shifting and unsigned modulo

This instruction is specifically designed for unsigned arithmetic. Byte and halfword integer constants are sign extended, and the result of the sign extension is treated as unsigned.

Division by zero triggers a divide-by-zero (DZ) trap. Only word (W) type is supported - no byte, halfword, or floating point variants. This instruction is essential for unsigned division and modulo operations.

**Operands:** 3
**Variants:** 4 opcode(s)

---

## Variants

Total variants: 4 (word type × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/4 | 0xFE48 | W | 1 | W1 UDIV |
| 2/4 | 0xFE49 | W | 2 | W2 UDIV |
| 3/4 | 0xFE4A | W | 3 | W3 UDIV |
| 4/4 | 0xFE4B | W | 4 | W4 UDIV |

---

## Operands

### Operand 1 (Dividend)

The first source operand (unsigned dividend). This operand is not modified.

**Type:** Word (treated as unsigned)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value (sign-extended then treated as unsigned)
- **REGISTER** - Integer register (I1-I4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Divisor)

The second source operand (unsigned divisor). This operand is not modified.

**Type:** Word (treated as unsigned)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value (sign-extended then treated as unsigned)
- **REGISTER** - Integer register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Quotient/Destination)

The destination operand where the quotient is stored. The remainder goes to register Rn.

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
- **Divide by zero (DZ):** Operand 2 = 0

---

## Data Status Bits

- **Z (Zero):** Set if quotient = 0, cleared otherwise
- **S (Sign):** Set to sign bit of quotient
- **DZ (Divide by Zero):** Set if operand 2 = 0

---

## Examples

### Example 1: Unsigned division with remainder

```assembly
        % Divide LONG/FACT on alternative domain, quotient in RES, remainder in R3
        W3 UDIV ALT(B.LONG), ALT(B.FACT), ALT(IND(RES))
```

### Example 2: Unsigned modulo operation

```assembly
        % Get unsigned remainder
        W1 UDIV B.UNSIGNED_VALUE, B.MODULUS, B.QUOTIENT
        % R1 contains unsigned remainder
```

### Example 3: Hash function

```assembly
        % Simple hash: key mod table_size
        W2 UDIV B.KEY, B.TABLE_SIZE, B.TEMP
        % R2 contains hash index (remainder)
```

### Example 4: Bit field extraction

```assembly
        % Divide by power of 2 for unsigned bit shift
        W4 UDIV B.VALUE, 256, B.SHIFTED
        % Effectively shifts right 8 bits for unsigned values
```

---

## Performance Notes

- **Typical cycles:** 13-21 cycles depending on addressing modes
- **Best case:** 13 cycles (register to register)
- **Worst case:** 21+ cycles (memory to memory with page fault)

**Note:** UDIV has similar performance to DIV4 but handles unsigned operands correctly.

---

## Reference Manual

**Section:** §11.16
**Title:** Unsigned divide

---

## See Also

- [DIV4](div4.md) - Signed divide with remainder to register
- [UMUL](umul.md) - Unsigned multiply with overflow
- [DIV2](div2.md) - Divide two operands (signed)
- [DIV3](div3.md) - Divide three operands (signed)
- [REM](rem.md) - Remainder operation
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
