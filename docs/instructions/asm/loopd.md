# LOOPD - Loop with Decrement

## Overview

**Mnemonic:** `loopd`
**Function:** Loop control with fixed decrement (-1)
**Class:** BRANCH
**Privilege:** user
**Format:** `t LOOPD <index>, <limit>, <displacement>`

---

## Description

Performs a counted loop operation with automatic decrement by one. This is a specialized, optimized version of the LOOP instruction with a fixed step value of -1. The instruction decrements the index variable and tests if the loop should continue or exit based on comparison with the limit value.

LOOPD is commonly used for reverse iteration and countdown loops.

**Operation:**
```
index = index - 1
if (index - limit) < 0:
    PC = next instruction (exit loop)
else:
    PC = PC + displacement (continue loop)
```

**Loop Termination Logic:**
- Continue loop while: index ≥ limit
- Exit loop when: index < limit

**Key Characteristics:**
- Fixed decrement step (-1) for optimized reverse iteration
- Single-instruction loop control (test-and-branch combined)
- Supports 5 data types (BY, H, W, F, D) with automatic scaling
- Two displacement sizes (byte: ±127, halfword: ±32767)
- Atomic index decrement and comparison (no race conditions)
- 3-5 cycle execution depending on branch taken
- More efficient than LOOP instruction when step is -1
- Common in reverse array traversal and countdown scenarios

**Common Use Cases:**
- Reverse array/buffer traversal
- Countdown loops
- Stack unwinding
- Backward iteration over data structures
- LIFO (Last In First Out) processing
- Reverse string processing

**Operands:** 3 (index, limit, displacement)
**Variants:** 10 opcodes (5 data types × 2 displacement sizes)

---

## Variants

| Variant | Opcode | Index Type | Displacement | Assembly |
|---------|--------|------------|--------------|----------|
| 1/10 | 0xFD23 | BY | Byte | BY LOOPD:B |
| 2/10 | 0xFD28 | BY | Halfword | BY LOOPD:H |
| 3/10 | 0xFD24 | H | Byte | H LOOPD:B |
| 4/10 | 0xFD29 | H | Halfword | H LOOPD:H |
| 5/10 | 0xFD25 | W | Byte | W LOOPD:B |
| 6/10 | 0xFD2A | W | Halfword | W LOOPD:H |
| 7/10 | 0xFD26 | F | Byte | F LOOPD:B |
| 8/10 | 0xFD2B | F | Halfword | F LOOPD:H |
| 9/10 | 0xFD27 | D | Byte | D LOOPD:B |
| 10/10 | 0xFD2C | D | Halfword | D LOOPD:H |

**Displacement size selection:**
- **:B suffix**: Byte displacement (-128 to +127 bytes)
- **:H suffix**: Halfword displacement (-32768 to +32767 bytes)

---

## Operands

**Operand 1** (Index, Read-Modify-Write):
- **Addressing modes**: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: BY/H/W/F/D (matches instruction prefix)
- **Role**: Loop counter/index variable
- **Access**: Read, decrement by 1, write back

**Operand 2** (Limit, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Same as index
- **Role**: Loop termination test value

**Operand 3** (Displacement, Immediate):
- **Addressing modes**: NONE (immediate/direct operand)
- **Data type**: Signed byte or halfword (based on variant)
- **Role**: Branch offset (typically negative to loop back)
- **Value**: Bytes from loop start to LOOPD instruction

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation

---

## Data Status Bits

- **Z (Zero)**: Unaffected
- **S (Sign)**: Unaffected
- **C (Carry)**: Unaffected
- **V (Overflow)**: Unaffected

**Note:** LOOPD does not modify status flags.

---

## Examples

### Example 1: Countdown loop (10 to 1)
```assembly
        % Loop with index = 10, 9, 8, ..., 1
        W1 := 10
LOOP1:
        % Loop body - I1 available as countdown
        W LOOPD I1, 1, LOOP1
        % Falls through when I1 = 0
```

### Example 2: Reverse array traversal
```assembly
        % Process array elements in reverse (99 to 0)
        W1 := 99
REVERSE:
        W2 MOVE ARRAY(I1)   % Get element
        % Process W2
        W LOOPD I1, 0, REVERSE
```

### Example 3: Countdown to zero
```assembly
        % Loop N times, counting down to 0
        W1 := N
COUNTDOWN:
        % Use I1 as remaining count
        W LOOPD I1, 0, COUNTDOWN
```

### Example 4: Stack unwinding
```assembly
        % Pop N items from stack
        W1 := N
UNWIND:
        W2 POP
        % Process popped value
        W LOOPD I1, 1, UNWIND
```

### Example 5: Reverse string processing
```assembly
        % Process string backwards
        W1 := LENGTH
        W1 DEC              % Start at last char
SCAN:
        BY2 MOVE STRING(I1)
        % Process character in I2
        W LOOPD I1, 0, SCAN
```

### Example 6: Byte countdown
```assembly
        % Countdown from 255
        BY1 := 255
BYTE_DOWN:
        % Process byte value
        BY LOOPD I1, 0, BYTE_DOWN
```

### Example 7: Buffer clear (backwards)
```assembly
        % Clear buffer in reverse
        W1 := SIZE
        W1 DEC
CLEAR:
        W MOVE 0, BUFFER(I1)
        W LOOPD I1, 0, CLEAR
```

### Example 8: Nested reverse loops
```assembly
        % Nested reverse iteration
        W1 := 5
OUTER:
        W2 := 10
INNER:
        % Process I1, I2 (both counting down)
        W LOOPD I2, 0, INNER
        W LOOPD I1, 0, OUTER
```

### Example 9: LIFO queue processing
```assembly
        % Process queue from end
        W1 := QUEUE_SIZE
        W1 DEC
PROCESS:
        W2 MOVE QUEUE(I1)
        % Process item
        W LOOPD I1, 0, PROCESS
```

### Example 10: Reverse memory copy
```assembly
        % Copy in reverse order
        W1 := COUNT
        W1 DEC
RCOPY:
        W2 MOVE SRC(I1)
        W2 MOVE DST(I1)
        W LOOPD I1, 0, RCOPY
```

---

## Performance Notes

- **Execution**: 3-5 cycles
  - Index in register: ~3 cycles
  - Index in memory: ~5 cycles
- **Optimization**: Faster than LOOP with step=-1 (no step operand fetch)
- **Reverse iteration**: Natural choice for processing data backwards
- **Atomic operation**: All operations complete without interruption

**Comparison with LOOP:**
```assembly
% LOOPD (3 operands):
W LOOPD I1, 0, LOOP     % Faster - implicit step=-1

% LOOP (4 operands):
W LOOP I1, -1, 0, LOOP  % Slower - explicit step
```

**Comparison with manual loop:**
```assembly
% Manual loop (4 instructions):
LOOP:
    % ... body ...
    W1 DEC              % Decrement
    W1 COMP LIMIT       % Compare
    IF>=GO LOOP         % Branch

% LOOPD (1 instruction):
LOOP:
    % ... body ...
    W LOOPD I1, LIMIT, LOOP
```

**Loop patterns:**
```assembly
% Pattern 1: Countdown from N to 1
W1 := N
LOOP: ... ; W LOOPD I1, 1, LOOP

% Pattern 2: Countdown from N to 0 (inclusive)
W1 := N
LOOP: ... ; W LOOPD I1, 0, LOOP

% Pattern 3: Reverse range (END-1 to START)
W1 := END
W1 DEC
LOOP: ... ; W LOOPD I1, START, LOOP
```

**Index value in loop body:**
```assembly
% Index is decremented BEFORE comparison but AFTER body
W1 := 10
LOOP:
    % First iteration: I1 = 10
    % Second iteration: I1 = 9
    % ...
    % Last iteration: I1 = LIMIT
    W LOOPD I1, LIMIT, LOOP
    % After loop: I1 = LIMIT-1
```

**Termination boundary:**
```assembly
% Example: LIMIT = 0
W1 := 10
LOOP:
    % Body executes 10 times with I1 = 10,9,8,...,1
    W LOOPD I1, 0, LOOP
% After loop: I1 = -1 (one before limit)

% To include zero:
W1 := 10
LOOP:
    % Body executes 11 times with I1 = 10,9,8,...,0
    W LOOPD I1, -1, LOOP
```

**Common use case comparison:**
```assembly
% Forward iteration (LOOPI):
W1 := 0
FORWARD:
    W2 MOVE ARRAY(I1)   % Access 0, 1, 2, ...
    W LOOPI I1, N, FORWARD

% Reverse iteration (LOOPD):
W1 := N
W1 DEC
BACKWARD:
    W2 MOVE ARRAY(I1)   % Access N-1, N-2, ..., 0
    W LOOPD I1, 0, BACKWARD
```

**When to use LOOPD vs LOOPI vs LOOP:**
- **LOOPD**: Counting down by 1, reverse iteration (~15% of loops)
- **LOOPI**: Counting up by 1, forward iteration (~80% of loops)
- **LOOP**: Custom step value, non-unit stride (~5% of loops)

**Advantages of LOOPD:**
- Simpler reverse iteration than manual decrements
- Single instruction vs separate DEC/COMP/branch
- Efficient for processing data structures from end to start
- Natural for stack/LIFO operations

---

## Reference Manual

**Section:** §13.5
**Title:** Loop with decrement

---

## See Also

- [LOOPI](loopi.md) - Loop increment (step=+1)
- [LOOP](loop.md) - Loop general (arbitrary step)
- [DEC](dec.md) - Decrement instruction
- [COMP](comp.md) - Compare instruction
