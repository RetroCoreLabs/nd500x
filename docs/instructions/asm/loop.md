# LOOP - Loop General

## Overview

**Mnemonic:** `loop`
**Function:** General loop control with step and limit
**Class:** BRANCH
**Privilege:** user
**Format:** `t LOOP <index>, <step>, <limit>, <displacement>`

---

## Description

Performs a general-purpose counted loop operation with an index variable, step value, limit test, and conditional branch. This instruction combines index increment/decrement with a limit comparison and conditional branch in a single atomic operation.

The LOOP instruction adds the step value to the index, then tests if the loop should continue or exit based on the relationship between the index and limit values.

**Operation:**
```
index = index + step
if (step > 0 and index - limit > 0) or (step < 0 and index - limit < 0):
    PC = next instruction (exit loop)
else:
    PC = PC + displacement (continue loop)

% If step = 0: Illegal operand value trap
```

**Loop Termination Logic:**
- **Positive step** (counting up): Exit when index > limit
- **Negative step** (counting down): Exit when index < limit
- **Zero step**: Trap (infinite loop prevention)

**Key Characteristics:**
- Four-operand general-purpose loop control
- Variable step value (positive, negative, or arbitrary)
- Single-instruction loop operation (atomic index update and test)
- Supports 5 data types (BY, H, W, F, D) for index and step
- Two displacement sizes (byte: ±127, halfword: ±32767)
- Automatic direction detection (step sign determines comparison)
- Infinite loop prevention (zero step causes trap)
- 4-6 cycle execution depending on branch taken
- Most flexible loop instruction (LOOPI/LOOPD are optimized variants)

**Common Use Cases:**
- Counted loops with arbitrary step values
- Array/buffer iteration with stride
- Numeric range processing
- Multi-dimensional array traversal
- DSP operations with custom increments
- Reverse iteration (negative step)

**Operands:** 4 (index, step, limit, displacement)
**Variants:** 10 opcodes (5 data types × 2 displacement sizes)

---

## Variants

| Variant | Opcode | Index Type | Displacement | Assembly |
|---------|--------|------------|--------------|----------|
| 1/10 | 0xFD2D | BY | Byte | BY LOOP:B |
| 2/10 | 0xFD32 | H | Byte | H LOOP:B |
| 3/10 | 0xFD2E | BY | Halfword | BY LOOP:H |
| 4/10 | 0xFD33 | H | Halfword | H LOOP:H |
| 5/10 | 0xFD2F | W | Byte | W LOOP:B |
| 6/10 | 0xFD34 | W | Halfword | W LOOP:H |
| 7/10 | 0xFD30 | F | Byte | F LOOP:B |
| 8/10 | 0xFD35 | F | Halfword | F LOOP:H |
| 9/10 | 0xFD31 | D | Byte | D LOOP:B |
| 10/10 | 0xFD36 | D | Halfword | D LOOP:H |

**Displacement size selection:**
- **:B suffix**: Byte displacement (-128 to +127 bytes)
- **:H suffix**: Halfword displacement (-32768 to +32767 bytes)

---

## Operands

**Operand 1** (Index, Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: BY/H/W/F/D (matches instruction prefix)
- **Role**: Loop counter/index variable
- **Access**: Read, add step, write back

**Operand 2** (Step, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as index
- **Role**: Increment/decrement value
- **Restriction**: Must not be zero (causes trap)

**Operand 3** (Limit, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as index
- **Role**: Loop termination test value

**Operand 4** (Displacement, Immediate):
- **Addressing modes**: NONE (immediate/direct operand)
- **Data type**: Signed byte or halfword (based on variant)
- **Role**: Branch offset (typically negative to loop back)
- **Value**: Bytes from loop start to LOOP instruction

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Illegal operand value (IOV)**: Step = 0

---

## Data Status Bits

- **Z (Zero)**: Unaffected
- **S (Sign)**: Unaffected
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

**Note:** LOOP does not modify status flags (unlike explicit comparison operations).

---

## Examples

### Example 1: Simple count-up loop (0 to 9)
```assembly
        % Loop 10 times: index = 0, 1, 2, ..., 9
        W1 := 0             % Initialize index
LOOP1:
        % Loop body here
        W LOOP I1, 1, 10, LOOP1
        % Falls through when I1 reaches 10
```

### Example 2: Count-down loop (10 to 1)
```assembly
        % Loop 10 times: index = 10, 9, 8, ..., 1
        W1 := 10
LOOP2:
        % Loop body
        W LOOP I1, -1, 0, LOOP2
        % Exits when I1 reaches 0
```

### Example 3: Array processing with stride
```assembly
        % Process every 4th element
        W1 := 0
LOOP3:
        W2 MOVE ARRAY(I1)   % Access array element
        % Process W2
        W LOOP I1, 4, 100, LOOP3
        % Processes indices: 0, 4, 8, ..., 96
```

### Example 4: Range iteration (start to end)
```assembly
        % Loop from START to END-1
        W1 MOVE START
LOOP4:
        % Process current index in I1
        W LOOP I1, 1, END, LOOP4
```

### Example 5: Byte loop with step 2
```assembly
        % Byte loop: 0, 2, 4, 6, 8
        BY1 := 0
LOOP5:
        % Process byte value
        BY LOOP I1, 2, 10, LOOP5
```

### Example 6: Floating-point loop
```assembly
        % Float loop: 0.0, 0.1, 0.2, ..., 0.9
        F1 := 0.0
LOOP6:
        % Process float value
        F LOOP I1, 0.1, 1.0, LOOP6
```

### Example 7: Double-precision loop
```assembly
        % Double loop with small step
        D1 := 0.0
LOOP7:
        % Process double value
        D LOOP I1, 0.01, 1.0, LOOP7
```

### Example 8: Nested loops
```assembly
        % Outer loop
        W1 := 0
OUTER:
        % Inner loop
        W2 := 0
INNER:
        % Process I1, I2
        W LOOP I2, 1, 10, INNER
        W LOOP I1, 1, 5, OUTER
```

### Example 9: Reverse array iteration
```assembly
        % Process array backwards
        W1 := 99            % Last index
LOOP9:
        W2 MOVE ARRAY(I1)
        % Process element
        W LOOP I1, -1, -1, LOOP9
        % Exits when I1 reaches -1
```

### Example 10: Memory copy with loop
```assembly
        % Copy N words
        W1 := 0
COPY:
        W2 MOVE SRC(I1)
        W2 MOVE DST(I1)
        W LOOP I1, 1, COUNT, COPY
```

---

## Performance Notes

- **Execution**: 4-6 cycles
  - Index in register: ~4 cycles
  - Index in memory: ~6 cycles
- **Atomic operation**: All operations complete without interruption
- **Displacement**: Use :B for short loops (< 128 bytes), :H for large loops
- **Step = 0**: Always traps (prevents accidental infinite loops)

**Loop placement:**
- LOOP instruction typically placed at end of loop body
- Displacement is typically negative (branches backward)
- Calculate displacement: (loop_start_address - loop_instruction_address)

**Comparison with manual loop:**
```assembly
% Manual loop (5 instructions):
LOOP:
    % ... body ...
    W1 ADD 1            % Increment
    W1 COMP LIMIT       % Compare
    IF<GO LOOP          % Branch

% LOOP instruction (1 instruction):
LOOP:
    % ... body ...
    W LOOP I1, 1, LIMIT, LOOP
```

**Step value considerations:**
```assembly
% Positive step (count up):
W LOOP I1, 1, 10, LOOP     % Exits when I1 > 10
W LOOP I1, 2, 10, LOOP     % Exits when I1 > 10 (odd or even start)

% Negative step (count down):
W LOOP I1, -1, 0, LOOP     % Exits when I1 < 0
W LOOP I1, -5, -10, LOOP   % Exits when I1 < -10

% Zero step:
W LOOP I1, 0, 10, LOOP     % TRAP - IOV
```

**Termination test details:**
```assembly
% For step > 0: Exit when (index - limit) > 0
% Example: index=10, limit=10, step=1
%   After step: index=11
%   Test: 11 - 10 = 1 > 0 → EXIT

% For step < 0: Exit when (index - limit) < 0
% Example: index=0, limit=0, step=-1
%   After step: index=-1
%   Test: -1 - 0 = -1 < 0 → EXIT

% This means:
% - Positive step: Loop while index <= limit
% - Negative step: Loop while index >= limit
```

**Common patterns:**
```assembly
% Pattern 1: Count N times (0 to N-1)
W1 := 0
LOOP: ... ; W LOOP I1, 1, N, LOOP

% Pattern 2: Count N times (1 to N)
W1 := 1
LOOP: ... ; W LOOP I1, 1, N+1, LOOP

% Pattern 3: Reverse count (N-1 to 0)
W1 := N-1
LOOP: ... ; W LOOP I1, -1, -1, LOOP

% Pattern 4: Stride access
W1 := 0
LOOP: ... ; W LOOP I1, STRIDE, SIZE, LOOP
```

---

## Reference Manual

**Section:** §13.6
**Title:** Loop General

---

## See Also

- [LOOPI](loopi.md) - Loop increment (simpler, step=+1)
- [LOOPD](loopd.md) - Loop decrement (simpler, step=-1)
- [IF](if.md) - Conditional branch instructions
- [COMP](comp.md) - Compare instruction (for manual loops)
