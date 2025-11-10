# ADD3 - Add Three Operands

## Overview

**Mnemonic:** `add3`
**Function:** Add three operands (non-destructive add)
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t ADD3 <a>,<b>,<c>`

---

## Description

Adds the `<a>` operand to the `<b>` operand and stores the result in the `<c>` operand (destination). This is a non-destructive operation - the original values of `<a>` and `<b>` are preserved.

The operands are assumed to have the same data type (BY, H, W, F, or D). For integer types, carry and overflow flags are set appropriately. For floating point types, overflow and underflow traps may occur.

This three-operand form is useful when you need to preserve the original values of both source operands, such as in complex mathematical expressions or when implementing algorithms that require non-destructive arithmetic.

**Operands:** 3
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5 (one per data type)

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFC67 | BY | Byte |
| 2/5 | 0xFC68 | H | Halfword |
| 3/5 | 0xFC69 | W | Word |
| 4/5 | 0xFC6A | F | Float |
| 5/5 | 0xFC6B | D | Double Float |

---

## Operands

### Operand 1 (First Source)

The first source operand.

**Type:** Byte, Halfword, Word, Float, or Double Float
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer register (I1-I4) or float register (A1-A4/D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Second Source)

The second source operand.

**Type:** Same data type as operand 1
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 3 (Destination)

The destination operand where the sum is stored.

**Type:** Same data type as operands 1 and 2
**Access:** Write

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **REGISTER** - Integer or float register
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

**Note:** CONSTANT mode not allowed for destination operands.

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Integer overflow (O):** Signed integer addition overflow (for BY, H, W types)
- **Floating overflow (FO):** Result too large to represent (for F, D types)
- **Floating underflow (FU):** Result too small to represent (for F, D types)

---

## Data Status Bits

- **Z (Zero):** Set if sum = 0, cleared otherwise
- **S (Sign):** Set to sign bit of sum
- **O (Overflow):** Set if integer overflow (BY, H, W types)
- **C (Carry):** Set if carry from most significant bit (integer types only)
- **FO (Floating Overflow):** Set if floating overflow (F, D types)
- **FU (Floating Underflow):** Set if floating underflow (F, D types)

---

## Examples

### Example 1: Add registers non-destructively

```assembly
        % Add R1 and R2, leave result in R3
        W ADD3 R1, R2, R3
```

### Example 2: Complex expression

```assembly
        % Calculate: result = (a + b) * c
        F ADD3 B.A, B.B, F1      % F1 = a + b
        F MUL2 F1, B.C           % F1 = F1 * c
        F1 =: B.RESULT
```

### Example 3: Vector addition

```assembly
        % Add two vectors element by element
        W1 := 0                  % Loop counter
LOOP:
        W ADD3 B.VEC_A(I1), B.VEC_B(I1), B.VEC_RESULT(I1)
        W ADD2 I1, 4             % Next element (word size)
        W COMP I1, B.VECTOR_SIZE
        IF<GO LOOP
```

### Example 4: Preserve operands

```assembly
        % Calculate sum while preserving originals
        D ADD3 B.ORIGINAL_A, B.ORIGINAL_B, D1
        D1 =: B.SUM
        % B.ORIGINAL_A and B.ORIGINAL_B unchanged
```

---

## Performance Notes

- **Typical cycles:** 4-6 cycles depending on addressing modes
- **Best case:** 4 cycles (register to register to register)
- **Worst case:** 6+ cycles (memory to memory with page fault)
- **Float/Double:** Additional cycles for floating point normalization

**Note:** Slightly slower than ADD2 due to third operand handling.

---

## Reference Manual

**Section:** §11.9
**Title:** Add three operands

---

## See Also

- [ADD2](add2.md) - Add two operands (destructive)
- [SUB3](sub3.md) - Subtract three operands
- [MUL3](mul3.md) - Multiply three operands
- [DIV3](div3.md) - Divide three operands
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
