# COMP2 - Compare Two Operands

## Overview

**Mnemonic:** `comp2`
**Function:** Compare two operands
**Class:** COMPARE
**Privilege:** user

**Format:** `t COMP2 <op1>, <op2>`

---

## Description

Compares two operands by subtracting the second from the first and setting flags based on the result. Unlike COMP (which uses an implicit accumulator), COMP2 allows comparing any two operands directly. The subtraction result is discarded - only flags are affected.

This is the general-purpose comparison instruction for symmetric comparisons where neither operand is special. It supports all data types (BI, BY, H, W, F, D) and sets flags according to signed/unsigned comparison semantics.

**Operation:**
```
FLAGS = (op1 - op2)  // Result discarded, only flags set
```

**Key Characteristics:**
- Symmetric two-operand comparison (neither privileged)
- Non-destructive (result discarded, only flags set)
- Supports all 6 data types (BI, BY, H, W, F, D)
- More flexible than COMP (explicit operands vs accumulator)
- Essential for sorting, range checking, validation
- Sets Z, S, C, V flags for conditional branching
- Slightly slower than COMP (2 operand encodings)
- Common in comparison-heavy algorithms

**Flag Interpretation:**
- **Equal**: Z=1 (op1 == op2)
- **Not Equal**: Z=0 (op1 != op2)
- **Less Than**: S=1, Z=0 (op1 < op2 signed)
- **Greater Than**: S=0, Z=0 (op1 > op2 signed)
- **Less/Equal**: S=1 OR Z=1 (op1 <= op2 signed)
- **Greater/Equal**: S=0 OR Z=1 (op1 >= op2 signed)

**Common Use Cases:**
- Symmetric comparisons (neither operand preferred)
- Comparing two memory locations
- Comparing two registers
- Validating against constants
- Range checking
- Sorting algorithms

**Operands:** 2
**Variants:** 6 opcodes (BI, BY, H, W, F, D)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFC15 | Bit | BI COMP2 |
| 2/6 | 0x002D | Byte | BY COMP2 |
| 3/6 | 0xFC16 | Halfword | H COMP2 |
| 4/6 | 0x002E | Word | W COMP2 |
| 5/6 | 0x002F | Float | F COMP2 |
| 6/6 | 0x0040 | Double | D COMP2 |

---

## Operands

**Operand 1** (First Value, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix
- **Role**: Left side of comparison (minuend)

**Operand 2** (Second Value, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix
- **Role**: Right side of comparison (subtrahend)

**Result**: No result - only flags modified

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Floating underflow (FU)**: For F/D types
- **Floating overflow (FO)**: For F/D types

---

## Data Status Bits

- **Z (Zero)**: Set if op1 == op2
- **S (Sign)**: Set if op1 < op2 (signed)
- **C (Carry)**: Set from MSB (unsigned comparison)
- **V (Overflow)**: Set if signed overflow (but S still correct)

---

## Examples

### Example 1: Compare two variables

```assembly
% Compare two memory locations
        W COMP2 VALUE_A, VALUE_B
        IF=GO EQUAL_HANDLER
        IF<GO A_LESS_THAN_B
        % A > B
```

### Example 2: Compare registers

```assembly
% Compare two general registers
        W COMP2 I1, I2
        IF>GO I1_GREATER
```

### Example 3: Validate against constant

```assembly
% Check if within range
        W COMP2 INPUT, MIN_VALUE
        IF<GO TOO_SMALL
        W COMP2 INPUT, MAX_VALUE
        IF>GO TOO_LARGE
        % Valid range
```

### Example 4: Floating-point comparison

```assembly
% Compare temperatures
        F COMP2 TEMP1, TEMP2
        IF>GO TEMP1_HOTTER
```

### Example 5: Array element comparison

```assembly
% Compare array elements for sorting
        W COMP2 ARRAY(I1), ARRAY(I2)
        IF<=GO NO_SWAP
        % Swap needed
        CALL SWAP_ELEMENTS
NO_SWAP:
```

### Example 6: Multi-criteria comparison

```assembly
% Two-level sort: primary then secondary
        W COMP2 PRIMARY_A, PRIMARY_B
        IF<>GO DONE         % Different primary values
        % Equal primary, check secondary
        W COMP2 SECONDARY_A, SECONDARY_B
DONE:
```

### Example 7: Bounds checking

```assembly
% Validate index
        W COMP2 INDEX, 0
        IF<GO INDEX_NEGATIVE
        W COMP2 INDEX, ARRAY_SIZE
        IF>=GO INDEX_TOO_LARGE
        % Index valid
```

---

## Performance Notes

- **Size**: 2-4 bytes (opcode + 2 operand encodings)
- **Execution**: 2-3 cycles
- **vs COMP**: COMP2 more flexible (any 2 operands), COMP faster (1 operand implicit)
- **Symmetric**: Neither operand is privileged
- **Use Case**: When comparing two equally-important values

---

## Reference Manual

**Section:** §10.10
**Title:** Compare two operands

---

## See Also

- [COMP](comp.md) - Compare with accumulator (single operand)
- [IF=GO](if=go.md) - Conditional branch if equal
- [IF<GO](iflessthango.md) - Conditional branch if less than
- [SUB](sub.md) - Subtraction with result
- [TEST](test.md) - Bitwise test
