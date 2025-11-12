# LOOPI - Loop with Increment

## Overview

**Mnemonic:** `loopi`
**Function:** Loop control with fixed increment (+1)
**Class:** BRANCH
**Privilege:** user
**Format:** `t LOOPI <index>, <limit>, <displacement>`

---

## Description

Performs a counted loop operation with automatic increment by one. This is a specialized, optimized version of the LOOP instruction with a fixed step value of +1. The instruction increments the index variable and tests if the loop should continue or exit based on comparison with the limit value.

LOOPI is the most common loop instruction for simple counting loops that increment by 1 each iteration.

**Operation:**
```
index = index + 1
if (index - limit) > 0:
    PC = next instruction (exit loop)
else:
    PC = PC + displacement (continue loop)
```

**Loop Termination Logic:**
- Continue loop while: index ≤ limit
- Exit loop when: index > limit

**Key Characteristics:**
- Fixed increment step (+1) for optimized forward iteration
- Single-instruction loop control (test-and-branch combined)
- Supports 5 data types (BY, H, W, F, D) with automatic scaling
- Two displacement sizes (byte: ±127, halfword: ±32767)
- Atomic index increment and comparison (no race conditions)
- 3-5 cycle execution depending on branch taken
- More efficient than LOOP instruction when step is +1
- Most common loop instruction (typical "for i = 0 to N" pattern)

**Common Use Cases:**
- Simple counting loops (for i = 0 to N)
- Sequential array/buffer traversal
- Iteration over consecutive memory locations
- String processing (character by character)
- Range-based operations
- Most common loop pattern in programming

**Operands:** 3 (index, limit, displacement)
**Variants:** 10 opcodes (5 data types × 2 displacement sizes)

---

## Variants

| Variant | Opcode | Index Type | Displacement | Assembly |
|---------|--------|------------|--------------|----------|
| 1/10 | 0xFCDE | BY | Byte | BY LOOPI:B |
| 2/10 | 0xFD1E | BY | Halfword | BY LOOPI:H |
| 3/10 | 0xFCDF | H | Byte | H LOOPI:B |
| 4/10 | 0xFD1F | H | Halfword | H LOOPI:H |
| 5/10 | 0x00BF | W | Byte | W LOOPI:B |
| 6/10 | 0x00E1 | W | Halfword | W LOOPI:H |
| 7/10 | 0xFD1C | F | Byte | F LOOPI:B |
| 8/10 | 0xFD21 | F | Halfword | F LOOPI:H |
| 9/10 | 0xFD1D | D | Byte | D LOOPI:B |
| 10/10 | 0xFD22 | D | Halfword | D LOOPI:H |

**Displacement size selection:**
- **:B suffix**: Byte displacement (-128 to +127 bytes)
- **:H suffix**: Halfword displacement (-32768 to +32767 bytes)

---

## Operands

**Operand 1** (Index, Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: BY/H/W/F/D (matches instruction prefix)
- **Role**: Loop counter/index variable
- **Access**: Read, increment by 1, write back

**Operand 2** (Limit, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as index
- **Role**: Loop termination test value

**Operand 3** (Displacement, Immediate):
- **Addressing modes**: NONE (immediate/direct operand)
- **Data type**: Signed byte or halfword (based on variant)
- **Role**: Branch offset (typically negative to loop back)
- **Value**: Bytes from loop start to LOOPI instruction

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation

---

## Data Status Bits

- **Z (Zero)**: Unaffected
- **S (Sign)**: Unaffected
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

**Note:** LOOPI does not modify status flags.

---

## Examples

### Example 1: Loop 10 times (0 to 9)
```assembly
        % Loop with index = 0, 1, 2, ..., 9
        W1 := 0
LOOP1:
        % Loop body - I1 available as index
        W LOOPI I1, 10, LOOP1
        % Falls through when I1 = 10
```

### Example 2: Loop N times (1 to N)
```assembly
        % Start at 1, loop until N
        W1 := 1
LOOP2:
        % Use I1 as 1-based index
        W LOOPI I1, N, LOOP2
```

### Example 3: Array processing
```assembly
        % Process 100 array elements
        W1 := 0
PROCESS:
        W2 MOVE ARRAY(I1)   % Get element
        % Process W2
        W LOOPI I1, 100, PROCESS
```

### Example 4: String iteration
```assembly
        % Iterate over null-terminated string
        W1 := 0
SCAN:
        BY2 MOVE STRING(I1)
        BY TEST I2
        IF=GO DONE          % Exit if null
        % Process character
        W LOOPI I1, 1000, SCAN  % Safety limit
DONE:
```

### Example 5: Byte loop
```assembly
        % Loop through 256 bytes
        BY1 := 0
BYTE_LOOP:
        % Process byte index
        BY LOOPI I1, 255, BYTE_LOOP
```

### Example 6: Memory fill
```assembly
        % Fill memory with value
        W1 := 0
FILL:
        W MOVE FILL_VALUE, BUFFER(I1)
        W LOOPI I1, SIZE, FILL
```

### Example 7: Floating-point sequence
```assembly
        % Generate sequence: 0.0, 1.0, 2.0, ..., 9.0
        F1 := 0.0
FLOOP:
        % Process F1
        F LOOPI I1, 10.0, FLOOP
```

### Example 8: Nested loops
```assembly
        % Nested iteration: i from 0-4, j from 0-9
        W1 := 0
OUTER:
        W2 := 0
INNER:
        % Process I1, I2
        W LOOPI I2, 10, INNER
        W LOOPI I1, 5, OUTER
```

### Example 9: Buffer copy
```assembly
        % Copy COUNT bytes
        W1 := 0
COPY:
        BY2 MOVE SRC(I1)
        BY2 MOVE DST(I1)
        W LOOPI I1, COUNT, COPY
```

### Example 10: Sum array elements
```assembly
        % Sum array of N words
        W1 := 0             % Index
        W2 := 0             % Sum accumulator
SUM:
        W3 MOVE ARRAY(I1)
        W2 ADD I3
        W LOOPI I1, N, SUM
        % Result in I2
```

---

## Performance Notes

- **Execution**: 3-5 cycles
  - Index in register: ~3 cycles
  - Index in memory: ~5 cycles
- **Optimization**: Faster than LOOP with step=1 (no step operand fetch)
- **Common case**: Most loops use LOOPI (increment by 1)
- **Atomic operation**: All operations complete without interruption

**Comparison with LOOP:**
```assembly
% LOOPI (3 operands):
W LOOPI I1, 10, LOOP    % Faster - implicit step=1

% LOOP (4 operands):
W LOOP I1, 1, 10, LOOP  % Slower - explicit step
```

**Comparison with manual loop:**
```assembly
% Manual loop (4 instructions):
LOOP:
    % ... body ...
    W1 INC              % Increment
    W1 COMP LIMIT       % Compare
    IF<=GO LOOP         % Branch

% LOOPI (1 instruction):
LOOP:
    % ... body ...
    W LOOPI I1, LIMIT, LOOP
```

**Loop patterns:**
```assembly
% Pattern 1: 0-based, N iterations (0 to N-1)
W1 := 0
LOOP: ... ; W LOOPI I1, N, LOOP

% Pattern 2: 1-based, N iterations (1 to N)
W1 := 1
LOOP: ... ; W LOOPI I1, N+1, LOOP

% Pattern 3: Explicit range (START to END-1)
W1 := START
LOOP: ... ; W LOOPI I1, END, LOOP
```

**Index value in loop body:**
```assembly
% Index is incremented BEFORE comparison but AFTER body
W1 := 0
LOOP:
    % First iteration: I1 = 0
    % Second iteration: I1 = 1
    % ...
    % Last iteration: I1 = LIMIT-1
    W LOOPI I1, LIMIT, LOOP
    % After loop: I1 = LIMIT
```

**Termination boundary:**
```assembly
% Example: LIMIT = 10
W1 := 0
LOOP:
    % Body executes 10 times with I1 = 0,1,2,...,9
    W LOOPI I1, 10, LOOP
% After loop: I1 = 10 (one past limit)

% If you want to include limit:
W1 := 0
LOOP:
    % Body executes 11 times with I1 = 0,1,2,...,10
    W LOOPI I1, 11, LOOP
```

**When to use LOOPI vs LOOP vs LOOPD:**
- **LOOPI**: Counting up by 1 (most common - ~80% of loops)
- **LOOPD**: Counting down by 1 (reverse iteration)
- **LOOP**: Custom step value (stride access, skip elements)

---

## Reference Manual

**Section:** §13.4
**Title:** Loop with increment

---

## See Also

- [LOOPD](loopd.md) - Loop decrement (step=-1)
- [LOOP](loop.md) - Loop general (arbitrary step)
- [INC](inc.md) - Increment instruction
- [COMP](comp.md) - Compare instruction
