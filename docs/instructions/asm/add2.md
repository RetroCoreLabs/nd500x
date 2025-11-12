# ADD2 - Add Two Operands

## Overview

**Mnemonic:** `add2`
**Function:** Add two operands (destructive add)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t ADD2 <a>,<b>`

---

## Description

Adds the `<b>` operand to the `<a>` operand and stores the result in the `<a>` operand (destination). This is a destructive operation - the original value of `<a>` is overwritten.

**Operation:**
```
<a> = <a> + <b>
```

**Key Characteristics:**
- Destructive two-operand addition (first operand overwritten)
- 5 data types supported (BY, H, W, F, D)
- Integer types set Z, S, O, C flags
- Float types may trap on overflow/underflow
- Essential for accumulators and counters
- Faster than ADD3 (fewer operand encodings, 3-5 cycles)
- Common in loop iteration and running totals
- Carry flag set for multi-precision arithmetic
- First operand must be writeable (not constant)

The operands are assumed to have the same data type (BY, H, W, F, or D). For integer types, carry and overflow flags are set appropriately. For floating point types, overflow and underflow traps may occur.

This instruction is commonly used for accumulation operations, counters, and general arithmetic where the destination operand can be modified.

**Operands:** 2
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC17 | BY | Byte |
| 2/5 | 0xFC54 | H | Halfword |
| 3/5 | 0x0053 | W | Word |
| 4/5 | 0xFC56 | F | Float |
| 5/5 | 0xFC57 | D | Double Float |

---

## Operands

### Operand 1 (Destination)

The first operand - serves as both source and destination. Result is stored here.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read/Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

### Operand 2 (Source)

The second operand - value to add to operand 1.

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
- **Integer overflow (O):** Signed integer addition overflow (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of result
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **C (Carry):** Set if carry from most significant bit (integer types only)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)

---

## Examples

### Example 1: Add float arguments

```assembly
        % Add float argument X2 to argument X1
        F ADD2 IND(B.X1), IND(B.X2)
```

### Example 2: Increment counter

```assembly
        % Increment word counter by 1
        W ADD2 B.COUNTER, 1
```

### Example 3: Accumulate array sum

```assembly
        % Sum array elements into accumulator
        W1 := 0                  % Initialize accumulator
        W2 := 0                  % Loop counter
LOOP:
        W ADD2 I1, B.ARRAY(I2)   % Add array[i] to accumulator
        W ADD2 I2, 1             % Increment counter
        W COMP I2, B.ARRAY_SIZE
        IF<GO LOOP
        W1 =: B.TOTAL_SUM
```

### Example 4: Double precision floating addition

```assembly
        % Add two double precision values
        D1 := B.VALUE_A
        D ADD2 D1, B.VALUE_B     % D1 = D1 + VALUE_B
        D1 =: B.RESULT
```

---

## Performance Notes

- **Typical cycles:** 3-5 cycles depending on addressing modes
- **Best case:** 3 cycles (register to register)
- **Worst case:** 5+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization

**Note:** Integer addition is significantly faster than floating point addition.

---

## Reference Manual

**Section:** §11.5
**Title:** Add two operands

---

## See Also

- [ADD3](add3.md) - Add three operands (non-destructive)
- [ADDC](addc.md) - Add with carry (multi-precision)
- [SUB2](sub2.md) - Subtract two operands
- [INCR](incr.md) - Increment by 1
- [+](+.md) - Add operator (synonym)
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
