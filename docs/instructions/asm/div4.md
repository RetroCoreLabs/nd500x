# DIV4 - Divide with Remainder to Register

## Overview

**Mnemonic:** `div4`
**Function:** Divide with remainder to register (modulo operation)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn DIV4 <a>,<b>,<c>`

---

## Description

Divides the `<a>` operand by the `<b>` operand and stores the quotient in the `<c>` operand (destination). The remainder is stored in the specified register Rn.

This instruction provides both quotient and remainder in a single operation, essential for modulo operations and division with remainder. The register content is in compliance with ADA and SIMULA remainder semantics. Separate testing must be done to obtain status.

Division by zero triggers a divide-by-zero (DZ) trap. Integer overflow occurs if and only if the largest possible negative integer is divided by -1.

The operands are assumed to have the same data type (BY, H, or W). Only integer types are supported - no floating point variants.

**Operands:** 3
**Variants:** 12 opcode(s)

---

## Variants

Total variants: 12 (3 data types × 4 registers)

| Variant | Opcode | Data Type | Register | Assembly Notation |
|---------|--------|-----------|----------|-------------------|
| 1/12 | 0xFC2C | BY | 1 | BY1 DIV4 |
| 2/12 | 0xFC2D | BY | 2 | BY2 DIV4 |
| 3/12 | 0xFC2E | BY | 3 | BY3 DIV4 |
| 4/12 | 0xFC2F | BY | 4 | BY4 DIV4 |
| 5/12 | 0xFC30 | H | 1 | H1 DIV4 |
| 6/12 | 0xFC31 | H | 2 | H2 DIV4 |
| 7/12 | 0xFC32 | H | 3 | H3 DIV4 |
| 8/12 | 0xFC33 | H | 4 | H4 DIV4 |
| 9/12 | 0xFC7C | W | 1 | W1 DIV4 |
| 10/12 | 0xFC7D | W | 2 | W2 DIV4 |
| 11/12 | 0xFC7E | W | 3 | W3 DIV4 |
| 12/12 | 0xFC7F | W | 4 | W4 DIV4 |

---

## Operands

### Operand 1 (Dividend)

The first source operand (dividend). This operand is not modified.

**Type:** Byte, Halfword, or Word
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Divisor)

The second source operand (divisor). This operand is not modified.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Quotient/Destination)

The destination operand where the quotient is stored. The remainder goes to register Rn.

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
- **Integer overflow (O):** Largest negative integer divided by -1
- **Divide by zero (DZ):** Operand 2 = 0

---

## Data Status Bits

- **Z (Zero):** Set if quotient = 0, cleared otherwise
- **S (Sign):** Set to sign bit of quotient
- **O (Overflow):** Set if integer overflow
- **DZ (Divide by Zero):** Set if operand 2 = 0

---

## Examples

### Example 1: Divide with remainder

```assembly
        % Divide BYTECOUNT by 4, quotient in WORDCOUNT, remainder in R2
        BY2 DIV4 R.BYTECOUNT, 4, R.WORDCOUNT
```

### Example 2: Modulo operation

```assembly
        % Get remainder of VALUE divided by MODULUS
        W1 DIV4 B.VALUE, B.MODULUS, B.QUOTIENT
        % R1 now contains the remainder (VALUE mod MODULUS)
```

### Example 3: Extract digits

```assembly
        % Extract last digit (mod 10)
        W3 DIV4 B.NUMBER, 10, B.REDUCED
        % R3 contains last digit, REDUCED contains number/10
```

### Example 4: Time conversion

```assembly
        % Convert seconds to minutes and seconds
        W2 DIV4 B.TOTAL_SECONDS, 60, B.MINUTES
        % R2 contains remaining seconds
```

---

## Performance Notes

- **Typical cycles:** 13-21 cycles depending on addressing modes
- **Best case:** 13 cycles (register to register)
- **Worst case:** 21+ cycles (memory to memory with page fault)

**Note:** DIV4 is slightly slower than DIV3 due to the extra register store operation.

---

## Reference Manual

**Section:** §11.14
**Title:** Divide with remainder to register (modulo)

---

## See Also

- [DIV2](div2.md) - Divide two operands (destructive)
- [DIV3](div3.md) - Divide three operands (non-destructive)
- [UDIV](udiv.md) - Unsigned divide with remainder
- [MUL4](mul4.md) - Multiply with overflow to register
- [REM](rem.md) - Remainder operation
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
