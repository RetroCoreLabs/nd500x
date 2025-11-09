# COMP - Compare

## Overview

**Mnemonic:** `comp`
**Function:** Compare with accumulator (register)
**Class:** COMPARE
**Privilege:** user

**Format:** `t COMP <operand>` (implicitly compares accumulator register with operand)

---

## Description

Compares the implicit accumulator register (determined by register number embedded in opcode) with the specified operand by performing a subtraction without storing the result. Sets status flags Z, S, C, V based on the comparison result, enabling subsequent conditional branches.

COMP is the fundamental comparison instruction in the ND-500 architecture. Unlike SUB, it discards the arithmetic result and only updates flags. This "true comparison" correctly handles signed overflow - the sign bit accurately reflects the comparison result even when subtraction would overflow.

**Comparison Semantics:**
```
FLAGS = (Accumulator - Operand)  % Result discarded, only flags set
```

**Status Flag Interpretation:**
- **Equal**: `Z=1` (accumulator == operand)
- **Not Equal**: `Z=0` (accumulator != operand)
- **Less Than (signed)**: `S=1, Z=0` (accumulator < operand)
- **Greater Than (signed)**: `S=0, Z=0` (accumulator > operand)
- **Less/Equal (signed)**: `S=1 OR Z=1` (accumulator <= operand)
- **Greater/Equal (signed)**: `S=0 OR Z=1` (accumulator >= operand)

**Key Characteristics:**

1. **Implicit Accumulator**: Register number (1-24) encoded in opcode - no explicit destination register
2. **Non-Destructive**: Accumulator and operand unchanged; only flags modified
3. **True Comparison**: Sign flag correctly reflects signed comparison even on overflow
4. **All Data Types**: Supports BI, BY, H, W, F, D types with appropriate comparison semantics
5. **Conditional Branch Integration**: Designed to precede IF=GO, IF<GO, IF>GO, etc.

**Common Use Cases:**
- Loop termination conditions (`COMP counter, limit`)
- Bounds checking (`COMP index, array_size`)
- Null/zero detection (`COMP value, 0`)
- Range validation (`COMP input, MIN` / `COMP input, MAX`)
- Sorting comparisons
- Search termination
- Conditional execution guards

COMP is typically followed by conditional branch instructions that test the flags it sets. The combination `COMP + IFxGO` is one of the most common instruction pairs in ND-500 code.

**Operands:** 1 (plus implicit accumulator register)
**Variants:** 24 opcode(s)

---

## Variants

| Variant | Opcode Range | Register | Assembly Notation | Note |
|---------|--------------|----------|-------------------|------|
| 1-24 | 0x0030-0x003F<br>0xFC18-0xFC1F | R1-R24 | BI/BY/H/W/F/D COMP | 24 register variants |

**Note**: Opcode encodes both the register number and operation. Different opcode ranges for different register groups.

---

## Operands

**Operand 1** (Comparand, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as instruction prefix (BI/BY/H/W/F/D)
- **Role**: Value to compare against accumulator

**Implicit Accumulator** (Source):
- **Register**: Encoded in opcode (R1-R24)
- **Role**: Left-hand side of comparison

**Result**: No result register - only status flags updated

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Floating-point exceptions**: For F/D types (invalid operation, denormal)

**Note**: COMP does NOT trap on signed overflow (unlike some arithmetic ops)

---

## Data Status Bits

- **Z (Zero)**: Set if accumulator == operand
- **S (Sign)**: Set if accumulator < operand (signed comparison)
- **C (Carry)**: Set based on unsigned comparison (accumulator < operand unsigned)
- **V (Overflow)**: Set if signed comparison would overflow (but S still correct!)
- **K (Interrupt bits)**: Unaffected

---

## Examples

### Example 1: Loop termination

```assembly
% Loop from 0 to LIMIT-1
        W1 CLR
LOOP:
        % Loop body
        W1 ADD 1, I1
        W1 COMP I1, LIMIT
        IF<GO LOOP        % Continue if I1 < LIMIT
```

### Example 2: Bounds checking

```assembly
% Check if index within array bounds
        W1 COMP INDEX, 0
        IF<GO UNDERFLOW   % Index < 0 (invalid)
        W1 COMP INDEX, ARRAY_SIZE
        IF>=GO OVERFLOW   % Index >= SIZE (invalid)
        % Index is valid
```

### Example 3: Null pointer check

```assembly
% Check if pointer is null
        W1 COMP PTR, 0
        IF=GO NULL_PTR_HANDLER
        % Pointer is valid, continue
```

### Example 4: Floating-point comparison

```assembly
% Compare temperature against threshold
        F1 COMP TEMPERATURE, THRESHOLD
        IF>GO OVERHEAT_ALARM
        IF<GO UNDERHEAT_ALARM
        % Within normal range
```

### Example 5: Search loop

```assembly
% Linear search for value
        W1 CLR
SEARCH:
        W2 COMP ARRAY(I1), TARGET
        IF=GO FOUND
        W1 ADD 1, I1
        W1 COMP I1, COUNT
        IF<GO SEARCH
        % Not found
        GO NOT_FOUND
FOUND:
        % I1 contains index of found element
```

### Example 6: Three-way comparison (sorting)

```assembly
% Compare A and B for sorting
        W1 COMP A, B
        IF<GO A_LESS_THAN_B
        IF>GO A_GREATER_THAN_B
        % A == B (equal)
        GO EQUAL_HANDLER

A_LESS_THAN_B:
        % A < B
        GO SORT_ASCENDING

A_GREATER_THAN_B:
        % A > B
        GO SORT_DESCENDING
```

### Example 7: Range validation

```assembly
% Validate input is in range [MIN, MAX]
        W1 COMP INPUT, MIN
        IF<GO OUT_OF_RANGE    % INPUT < MIN
        W1 COMP INPUT, MAX
        IF>GO OUT_OF_RANGE    % INPUT > MAX
        % Input is valid
        GO PROCESS_INPUT
```

---

## Performance Notes

- **Size**: 2+ bytes (opcode + operand encoding)
- **Execution Time**: Typically 1-2 cycles (register comparison)
- **vs SUB**: COMP is semantically better for comparisons (no result to discard)
- **Branch Pairing**: Modern implementations may fuse COMP+IFxGO into single µop
- **Overflow Safety**: Sign flag remains correct even on signed overflow
- **Floating-Point**: F/D comparisons follow IEEE 754 semantics
- **Optimization**: Compilers emit COMP instead of SUB when result unused

---

## Reference Manual

**Section:** §10.9
**Title:** Compare

---

## See Also

- [IF=GO](if=go.md) - Conditional branch if equal
- [IF<GO](if<go.md) - Conditional branch if less than
- [IF>GO](if>go.md) - Conditional branch if greater than
- [IF>=GO](if>=go.md) - Conditional branch if greater or equal
- [IF<=GO](if<=go.md) - Conditional branch if less or equal
- [TEST](test.md) - Bitwise test operation
- [COMP2](comp2.md) - Two-operand compare
- [SUB](sub.md) - Subtraction with result
