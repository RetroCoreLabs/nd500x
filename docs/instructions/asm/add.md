# ADD - Add

## Overview

**Mnemonic:** `add`
**Function:** Add
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `{prefix}{register} ADD <operand>`

---

## Description

Add a source operand to the destination register and store the result in the destination register. This is the fundamental addition instruction, supporting integer and floating-point addition with automatic overflow detection.

**Operation:**
```
destination_register ← destination_register + source_operand
```

The ADD instruction sets appropriate status flags for signed arithmetic, including overflow detection for two's complement addition.

**Operands:** 1
**Variants:** 20 opcode(s)

---

## Variants

Total variants: 20

| Variant | Opcode | Prefix | Register | Addressing Modes |
|---------|--------|--------|----------|------------------|
| 1/20 | 0x3000 | BY | 1 | ALL |
| 2/20 | 0x3001 | BY | 2 | ALL |
| 3/20 | 0x3002 | BY | 3 | ALL |
| 4/20 | 0x3003 | BY | 4 | ALL |
| 5/20 | 0x3004 | H | 1 | ALL |
| 6/20 | 0x3005 | H | 2 | ALL |
| 7/20 | 0x3006 | H | 3 | ALL |
| 8/20 | 0x3007 | H | 4 | ALL |
| 9/20 | 0x3008 | W | 1 | ALL |
| 10/20 | 0x3009 | W | 2 | ALL |
| 11/20 | 0x300A | W | 3 | ALL |
| 12/20 | 0x300B | W | 4 | ALL |
| 13/20 | 0x300C | F | 1 | ALL |
| 14/20 | 0x300D | F | 2 | ALL |
| 15/20 | 0x300E | F | 3 | ALL |
| 16/20 | 0x300F | F | 4 | ALL |
| 17/20 | 0x3010 | D | 1 | ALL |
| 18/20 | 0x3011 | D | 2 | ALL |
| 19/20 | 0x3012 | D | 3 | ALL |
| 20/20 | 0x3013 | D | 4 | ALL |

---

## Operands

### Operand 1: Source Value
The value to add to the destination register.

**Supported modes (ALL):**
- **CONSTANT:** `value` - Add immediate constant
- **REGISTER:** `Rn` - Add value from another register
- **LOCAL:** `B.displacement` - Add local variable value
- **RECORD:** `R.displacement` - Add record field value
- **ABSOLUTE:** `address` - Add value from absolute address
- **PRE_INDEXED:** `B.array(Rn)` - Add indexed array element
- And all other addressing modes

---

## Trap Conditions

- **ARITHMETIC_OVERFLOW (Bit 3):** Signed overflow occurred (if overflow trap enabled)
- **FLOATING_OVERFLOW (Bit 2):** Floating-point overflow (F/D prefixes only)
- **FLOATING_UNDERFLOW (Bit 1):** Floating-point underflow (F/D prefixes only)
- **OPERAND_ERROR (Bit 5):** Invalid addressing mode or alignment

---

## Data Status Bits

- **Z (Zero):** Set if result is zero
- **S (Sign):** Set if result is negative (sign bit set)
- **O (Overflow):** Set if signed arithmetic overflow occurred
- **K (Flag):** Preserved

**Overflow Detection:**
- **Integer:** Two's complement overflow (positive + positive = negative, or negative + negative = positive)
- **Float:** IEEE 754 overflow detection

---

## Examples

### Example 1: Add Immediate Constant

```assembly
        W1 := 10                ; Initialize W1 to 10
        W1 ADD 5                ; W1 ← 10 + 5 = 15
        W1 ADD 100              ; W1 ← 15 + 100 = 115
```

**Explanation:** Adding immediate constants to a register.

### Example 2: Add Two Registers

```assembly
        W1 := 20                ; W1 ← 20
        W2 := 30                ; W2 ← 30
        W1 ADD W2               ; W1 ← 20 + 30 = 50
```

**Explanation:** Adding values from two registers.

### Example 3: Accumulator Pattern

```assembly
        W1 := 0                 ; Initialize accumulator
        W1 ADD B.VALUE1         ; Add first value
        W1 ADD B.VALUE2         ; Add second value
        W1 ADD B.VALUE3         ; Add third value
        W1 =: B.TOTAL           ; Store sum
```

**Explanation:** Accumulating multiple values into a running total.

### Example 4: Floating-Point Addition

```assembly
        F1 := 3.14              ; F1 ← π (approximation)
        F1 ADD 2.71828          ; F1 ← π + e ≈ 5.85828
        F1 =: B.RESULT          ; Store result
```

**Explanation:** Floating-point arithmetic using F prefix.

### Example 5: Array Summation Loop

```assembly
        W1 := 0                 ; sum = 0
        W2 := 0                 ; index = 0
loop:
        W1 ADD B.ARRAY(W2)      ; sum += array[index]
        W2 ADD 1                ; index++
        W2 COMP 10              ; Compare with array size
        IF < GO loop            ; Continue if index < 10
        W1 =: B.SUM             ; Store final sum
```

**Explanation:** Summing array elements in a loop.

### Example 6: Overflow Handling

```assembly
        W1 := 0x7FFFFFFF        ; Maximum positive 32-bit int
        W1 ADD 1                ; Overflow! (0x7FFFFFFF + 1 = 0x80000000)
                                ; O flag set, result is negative
        IFSTGO overflow_handler ; Jump if overflow occurred
```

**Explanation:** Detecting arithmetic overflow.

---

## Performance Notes

- **Typical cycles:** 2-6 cycles (varies by addressing mode and data type)
- **Best case:** 2 cycles (add immediate constant to register)
- **Worst case:** 8+ cycles (complex addressing + floating-point operation)

**Cycle counts by data type:**
- BY/H/W (integer): 2-6 cycles
- F (single float): 3-8 cycles
- D (double float): 4-10 cycles

**Optimization tips:**
- Use immediate constants when possible (fastest)
- Keep operands in registers for tight loops
- For multi-precision arithmetic, use ADDC (add with carry) for higher words
- Consider ADD2/ADD3 for adding multiple operands in one instruction

---

## Reference Manual

**Section:** §11.1
**Title:** Add
**Page:** 161

---

## See Also

- [SUB instruction](sub.md) - Subtract operation
- [ADDC instruction](addc.md) - Add with carry (for multi-precision)
- [ADD2 instruction](add2.md) - Add two operands (three-address form)
- [ADD3 instruction](add3.md) - Add three operands
- [INCR instruction](incr.md) - Increment by 1 (optimized)
- [Addressing Modes](../AddressingModes.md)
- [Data Type Prefixes](../Prefixes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
