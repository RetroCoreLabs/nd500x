# SUB2 - Subtract Two Operands

## Overview

**Mnemonic:** `sub2`
**Function:** Subtract two operands (destructive subtract)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t SUB2 <a>,<b>`

---

## Description

Subtracts the `<b>` operand from the `<a>` operand and stores the difference in the `<a>` operand (destination). This is a destructive operation - the original value of `<a>` is overwritten.

**Operation:**
```
<a> = <a> - <b>
```

**Key Characteristics:**
- Destructive two-operand subtraction (first operand overwritten)
- 5 data types supported (BY, H, W, F, D)
- Integer types set Z, S, O, C flags
- Float types may trap on overflow/underflow
- Essential for in-place decrement and accumulation
- Faster than SUB3 (fewer operand encodings, 4-7 cycles)
- Common in loop counters and running totals
- Carry flag set for borrow (multi-precision subtraction)
- First operand must be writeable (not constant)

For integer types (BY, H, W), carry and overflow flags are set appropriately. For floating point types (F, D), overflow and underflow traps may occur.

The operands are assumed to have the same data type. This instruction is commonly used for in-place subtraction operations where the minuend can be modified.

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC58 | BY | Byte |
| 2/5 | 0xFC59 | H | Halfword |
| 3/5 | 0x00E0 | W | Word |
| 4/5 | 0xFC5B | F | Float |
| 5/5 | 0xFC5C | D | Double Float |

---

## Operands

### Operand 1 (Minuend/Destination)

The first operand - serves as both source (minuend) and destination. Result is stored here.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read/Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

### Operand 2 (Subtrahend)

The second operand (subtrahend) - value to subtract from operand 1.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Integer overflow (O):** Signed integer subtraction overflow (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)

---

## Data Status Bits

- **Z (Zero):** Set if difference = 0, cleared otherwise
- **S (Sign):** Set to sign bit of difference
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **C (Carry):** Set if carry from most significant bit (integer types only)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)

---

## Examples

### Example 1: Subtract from array element

```assembly
        % Subtract 4 from R3rd element of byte array VALUES
        BY SUB2 DESC(VALUES)(R3), 4
```

### Example 2: Subtract from local variable

```assembly
        % Subtract constant from local variable
        W SUB2 B.COUNT, 1
```

### Example 3: Register subtraction

```assembly
        % Subtract memory value from register
        W SUB2 I1, B.DECREMENT
```

### Example 4: Float subtraction

```assembly
        % Subtract coefficient from float variable
        F SUB2 B.VALUE, B.OFFSET
```

### Example 5: Decrement array element

```assembly
        % Decrement array element
        W SUB2 B.ARRAY(I2), I3
```

---

## Performance Notes

- **Typical cycles:** 4-7 cycles depending on addressing modes and data type
- **Best case:** 4 cycles (register to register, integer)
- **Worst case:** 7+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization

**Note:** Integer subtraction is significantly faster than floating point subtraction.

---

## Reference Manual

**Section:** §11.6
**Title:** Subtract two operands

---

## See Also

- [SUB3](sub3.md) - Subtract three operands (non-destructive)
- [SUBC](subc.md) - Subtract with carry (multi-precision)
- [ADD2](add2.md) - Add two operands
- [NEG](neg.md) - Negate
- [DECR](decr.md) - Decrement by 1
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
