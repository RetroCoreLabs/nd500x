# SWAP - Swap Two Operands

## Overview

**Mnemonic:** `swap`
**Function:** Exchange contents of two operands
**Class:** MOVE
**Privilege:** user
**Format:** `t SWAP <op1>, <op2>`

---

## Description

Exchanges the contents of two operands in a single atomic operation. The value from the first operand is moved to the second operand, and simultaneously the original value from the second operand is moved to the first operand. This operation does not require a temporary storage location.

Both operands must be of the same data type as specified by the instruction prefix.

**Operation:**
```
temp = op1
op1 = op2
op2 = temp

% Flags set based on original op1 value:
original op1 = 0 → Z flag
original op1.signbit → S flag
```

**Key Characteristics:**
- Atomic two-operand exchange (single instruction)
- No temporary storage required (hardware-managed)
- Supports all 6 data types (BI, BY, H, W, F, D)
- Both operands must be read-modify-write capable
- Cannot use CONSTANT addressing (immutable)
- Sets flags based on original op1 value
- More efficient than three-instruction swap sequence
- Common in sorting algorithms and data structure manipulation

**Common Use Cases:**
- Swapping variables without temporary storage
- Register exchange operations
- Data structure manipulation (linked lists, trees)
- Algorithm implementations (sorting, partitioning)
- Parameter reordering
- Context switching

**Operands:** 2 (both read-modify-write)
**Variants:** 6 opcodes (6 data types)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFCBD | Bit | BI SWAP |
| 2/6 | 0xFCBE | Byte | BY SWAP |
| 3/6 | 0xFCBF | Halfword | H SWAP |
| 4/6 | 0x0052 | Word | W SWAP |
| 5/6 | 0xFCDC | Float | F SWAP |
| 6/6 | 0xFCDD | Double | D SWAP |

---

## Operands

**Operand 1** (Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix (BI/BY/H/W/F/D)
- **Role**: First value to exchange
- **Access**: Read original value, write op2's value
- **Restriction**: CONSTANT mode illegal (cannot write to constant)

**Operand 2** (Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Must match operand 1 data type
- **Role**: Second value to exchange
- **Access**: Read original value, write op1's value
- **Restriction**: CONSTANT mode illegal (cannot write to constant)

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Illegal addressing mode**: CONSTANT used for either operand

---

## Data Status Bits

- **Z (Zero)**: Set if original op1 value = 0, cleared otherwise
- **S (Sign)**: Set to original op1 value's sign bit
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

**Note:** Flags are set based on the **original** value of operand 1 (before the swap).

---

## Examples

### Example 1: Swap two variables
```assembly
        % Exchange contents of word variables EAST and WEST
        W SWAP EAST, WEST
```

### Example 2: Swap register with memory
```assembly
        % Exchange register with memory location
        W SWAP I1, B.TEMP
```

### Example 3: Exchange record fields
```assembly
        % Swap two fields in a record
        W SWAP R.X, R.Y
```

### Example 4: Swap array elements
```assembly
        % Exchange two array elements (sorting)
        W SWAP ARRAY(W1), ARRAY(W2)
```

### Example 5: Byte swap
```assembly
        % Exchange byte values
        BY SWAP FLAGS1, FLAGS2
```

### Example 6: Register swap
```assembly
        % Exchange two registers
        W SWAP I1, I2
```

### Example 7: Swap in sorting algorithm
```assembly
        % Bubble sort inner loop
        W COMP ARRAY(W1), ARRAY(W2)
        IF<=GO NO_SWAP
        W SWAP ARRAY(W1), ARRAY(W2)
NO_SWAP:
```

### Example 8: Linked list manipulation
```assembly
        % Swap node pointers
        W SWAP NODE1.NEXT, NODE2.NEXT
```

### Example 9: Double precision swap
```assembly
        % Exchange double-precision floats
        D SWAP VAR1, VAR2
```

### Example 10: Conditional swap
```assembly
        % Swap if condition met
        W TEST B.CONDITION
        IF=GO SKIP
        W SWAP A, B
SKIP:
```

---

## Performance Notes

- **Execution**: 3-4 cycles
  - Register-register: 2 cycles
  - Register-memory: 3 cycles
  - Memory-memory: 4 cycles
- **Atomic operation**: Completes without interruption
- **No temporary needed**: More efficient than three-move sequence
- **Memory bandwidth**: Memory-memory swap requires two reads and two writes

**Comparison with manual swap:**
```assembly
% Manual swap (requires temporary):
W MOVE A, TEMP      % 2 cycles
W MOVE B, A         % 2 cycles
W MOVE TEMP, B      % 2 cycles
% Total: 6 cycles, needs temporary storage

% SWAP instruction:
W SWAP A, B         % 4 cycles, no temporary
% More efficient, atomic
```

**Flag behavior note:**
```assembly
% Example showing flag behavior
W MOVE 0, A
W MOVE 100, B
W SWAP A, B         % Z flag SET (original A was 0)
                    % After: A=100, B=0

W MOVE -5, A
W MOVE 10, B
W SWAP A, B         % S flag SET (original A was negative)
                    % After: A=10, B=-5
```

**Common patterns:**
```assembly
% Pattern 1: Simple variable exchange
W SWAP VAR1, VAR2

% Pattern 2: Register-memory exchange
W SWAP I1, STORAGE

% Pattern 3: Indexed swap (sorting/partitioning)
W SWAP ARRAY(I1), ARRAY(I2)

% Pattern 4: Conditional swap (selection)
W COMP A, B
IF>GO SKIP
W SWAP A, B         % Ensure A <= B
SKIP:
```

**When to use SWAP vs MOVE:**
- Use SWAP when both values need to be preserved and exchanged
- Use three MOVEs if only one value needs to move
- SWAP is atomic, MOVEs are not (important for interrupts)

---

## Reference Manual

**Section:** §10.8
**Title:** Swap

---

## See Also

- [MOVE](move.md) - Copy data from source to destination
- [XCHG](xchg.md) - Exchange (if available on platform variant)
- [CLR](clr.md) - Clear register (when swap with zero not needed)
- [BMOVE](bmove.md) - Block move (for bulk transfers)
