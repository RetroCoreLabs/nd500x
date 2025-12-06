# BMOVE - Block Move

## Overview

**Mnemonic:** `bmove`
**Function:** Move or fill block of memory
**Class:** MOVE
**Privilege:** user

**Format:** `t BMOVE <source>, <dest>, <count>`

---

## Description

Copies a block of elements from source to destination, or fills destination with a constant value. This is the fundamental bulk memory operation in the ND-500 architecture, optimized for efficient data transfer and initialization.

**Key Characteristics:**
- Hardware-accelerated bulk memory operation
- Dual mode: copy (memory to memory) or fill (value to memory)
- Automatic overlap handling (safe for same buffer)
- 5 data types (BY, H, W, F, D) supported
- 10-100x faster than manual loops
- Essential for memcpy/memset functionality
- All status flags cleared after execution
- Optimized for word-aligned transfers

**Two Operating Modes:**

1. **Block Copy** (source is memory): Copies `count` elements from source array to destination array
2. **Block Fill** (source is register/constant): Fills destination with `count` copies of source value

The instruction automatically handles overlapping regions correctly, choosing forward or backward copy direction as needed. This makes BMOVE safe for moving data within the same buffer.

**Operation:**
```
if source is register/constant:
    for i = 0 to count-1:
        source_value -> dest[i]
else:
    for i = 0 to count-1:
        source[i] -> dest[i]  (with overlap handling)
```

**Common Use Cases:**
- Array copying (memcpy)
- Buffer initialization (memset)
- String duplication
- Data structure cloning
- Screen buffer updates
- Zero-filling memory regions

**Operands:** 3 (source, dest, count)
**Variants:** 5 opcodes (BY, H, W, F, D)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/5 | 0xFD20 | Byte | BY BMOVE |
| 2/5 | 0xFE78 | Halfword | H BMOVE |
| 3/5 | 0xFE79 | Word | W BMOVE |
| 4/5 | 0xFE7A | Float | F BMOVE |
| 5/5 | 0xFE7B | Double | D BMOVE |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix
- **Role**: Source array base (or fill value if constant/register)

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, PRE_INDEXED, ABSOLUTE (NOT constant/register)
- **Data type**: Matches instruction prefix
- **Role**: Destination array base

**Operand 3** (Count, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W) - unsigned
- **Role**: Number of elements to copy/fill

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation

---

## Data Status Bits

**All flags cleared** (Z, S, C, V all set to 0)

---

## Examples

### Example 1: Copy array

```assembly
% Copy 100 words from SRC to DEST
        W BMOVE SOURCE_ARRAY, DEST_ARRAY, 100
```

### Example 2: Zero-fill buffer

```assembly
% Clear 256 bytes
        W1 CLR
        BY BMOVE I1, BUFFER, 256
```

### Example 3: Initialize array with value

```assembly
% Fill array with -1 (0xFFFFFFFF)
        W1 MOVE -1, I1
        W BMOVE I1, INIT_ARRAY, 50
```

### Example 4: Copy overlapping regions (safe)

```assembly
% Shift array elements down by 10
        W BMOVE ARRAY+10, ARRAY, 90
        % BMOVE handles overlap correctly
```

### Example 5: String duplication

```assembly
% Copy string (byte array)
        BY BMOVE SOURCE_STR, DEST_STR, STR_LEN
```

### Example 6: Screen buffer update

```assembly
% Copy display buffer to screen memory
        W BMOVE DISPLAY_BUF, SCREEN_ADDR, 2000
```

### Example 7: Float array initialization

```assembly
% Fill float array with 0.0
        F1 CLR
        F BMOVE F1, FLOAT_ARRAY, 100
```

---

## Performance Notes

- **Optimized**: Hardware-accelerated block transfer
- **Overlap Handling**: Automatic direction selection
- **vs Manual Loop**: 10-100x faster than element-by-element copy
- **Large Transfers**: Efficient for any size, but best for >10 elements
- **Fill Mode**: Constant source is faster than memory-to-memory
- **Alignment**: Word-aligned transfers are fastest

---

## Reference Manual

**Section:** 15.1
**Title:** Block Move and Fill

---

## See Also

- [SMOVE](smove.md) - String move
- [SFILL](sfill.md) - String fill
- [:=](assignto.md) - Single element assignment
- [CLR](clr.md) - Clear register

---

# Detailed Syntax Reference

This section provides detailed explanations of the actual nd500-as assembler syntax, with verified examples showing exact machine code output.

## Understanding the Instruction Format

```
w bmove $0xDEADBEEF,r1.(0x0),r2
```

Breaking this down piece by piece:

| Part | Meaning |
|------|---------|
| `w` | Word operation (4 bytes per element) |
| `bmove` | Block move instruction |
| `$0xDEADBEEF` | The VALUE to fill with (a constant) |
| `r1.(0x0)` | Destination address = contents of register I1 + offset 0 |
| `r2` | Count = contents of register I2 |

**In plain English:** "Fill memory starting at the address in I1 with the value 0xDEADBEEF, repeat I2 times, each element is 4 bytes (word)."

---

## Addressing Modes Explained

The ND-500 has several ways to specify operands:

### Constants (Immediate Values)

| Syntax | Example | Meaning |
|--------|---------|---------|
| `$value` | `$0xDEADBEEF` | Use this exact value |
| `number` | `4` | Short constant (0-63), use this exact value |

**Example:** `$0xAB` means "the value 0xAB" (not an address, the actual number)

### Registers

| Syntax | Example | Meaning |
|--------|---------|---------|
| `r1` | `r1` | Use the value stored in register I1 |
| `r2` | `r2` | Use the value stored in register I2 |
| `r3` | `r3` | Use the value stored in register I3 |
| `r4` | `r4` | Use the value stored in register I4 |

**Example:** If I2 contains 100, then `r2` means "the value 100"

### Pre-Indexed (Register + Offset)

| Syntax | Example | Meaning |
|--------|---------|---------|
| `rN.(offset)` | `r1.(0)` | Address = contents of IN + offset |
| `rN.(offset)` | `r1.(0x10)` | Address = contents of IN + 16 |

**Example:** If I1 contains 0x1000, then `r1.(0x10)` means "address 0x1010"

This is like array indexing: `array[offset]` where `array` is the base address in the register.

---

## Verified Assembler Examples

All examples below have been tested with nd500-as and nd500-dis.

### Verified Example 1: Fill memory with a constant value

**nd500-as input:**
```assembly
w bmove $0xDEADBEEF,r1.(0),r2
```

**Machine code:** `FE 79 CF DE AD BE EF F4 00 D1`

**nd500-dis output:**
```
00000000: FE 79 CF DE AD BE EF F4 00 D1 w bmove      $0xDEADBEEF,r1.(0x0),r2
```

**Setup and execution:**
- Set I1 = 0x1000 (destination address)
- Set I2 = 4 (count: fill 4 words)

**What happens:**
1. Source = constant value 0xDEADBEEF
2. Destination = address 0x1000 (from I1 + 0)
3. Count = 4 (from I2)
4. Result: Writes 0xDEADBEEF to addresses 0x1000, 0x1004, 0x1008, 0x100C

**Memory before:**
```
0x1000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

**Memory after:**
```
0x1000: DE AD BE EF DE AD BE EF DE AD BE EF DE AD BE EF
```

---

### Verified Example 2: Fill bytes with a value

**nd500-as input:**
```assembly
by bmove $0xAB,r1.(0),r2
```

**Machine code:** `FD 20 CD AB F4 00 D1`

**nd500-dis output:**
```
00000000: FD 20 CD AB F4 00 D1          by bmove     $0xAB,r1.(0x0),r2
```

**Setup and execution:**
- Set I1 = 0x1000 (destination address)
- Set I2 = 8 (count: fill 8 bytes)

**What happens:**
1. Source = constant value 0xAB (byte)
2. Destination = address 0x1000
3. Count = 8
4. Result: Writes 0xAB to 8 consecutive bytes

**Memory after:**
```
0x1000: AB AB AB AB AB AB AB AB 00 00 00 00 00 00 00 00
```

---

### Verified Example 3: Copy memory block

**nd500-as input:**
```assembly
w bmove r1.(0),r2.(0),r3
```

**Machine code:** `FE 79 F4 00 F5 00 D2`

**nd500-dis output:**
```
00000000: FE 79 F4 00 F5 00 D2          w bmove      r1.(0x0),r2.(0x0),r3
```

**Setup:**
- Set I1 = 0x2000 (source address)
- Set I2 = 0x3000 (destination address)
- Set I3 = 4 (count: copy 4 words)

**What happens:**
1. Source = memory starting at address 0x2000 (from I1)
2. Destination = memory starting at address 0x3000 (from I2)
3. Count = 4 (from I3)
4. Result: Copies 16 bytes (4 words) from 0x2000 to 0x3000

This is equivalent to C's `memcpy(dest, src, count * sizeof(word))`.

---

### Verified Example 4: Zero-fill a buffer

**nd500-as input:**
```assembly
w bmove 0,r1.(0),r2
```

**Machine code:** `FE 79 C4 00 00 00 00 F4 00 D1`

**nd500-dis output:**
```
00000000: FE 79 C4 00 00 00 00 F4 00 D1 w bmove      $0x0,r1.(0x0),r2
```

**What happens:**
1. Source = constant 0
2. Destination = address from I1
3. Count = from I2
4. Result: Fills memory with zeros (like `memset(ptr, 0, size)`)



#### Real world example

  What This Instruction Will Do

  w bmove $0xF0F0F0F0,r1.(0x0),r2

  Given your register state:
  - I1 = 0x00000000 (destination address)
  - I2 = 0x00050000 = 327,680 (count)

  The Operation:

  | Operand     | Value               | Meaning                   |
  |-------------|---------------------|---------------------------|
  | $0xF0F0F0F0 | Constant            | Fill value                |
  | r1.(0x0)    | I1 + 0 = 0x00000000 | Destination start address |
  | r2          | I2 = 0x00050000     | Count = 327,680 elements  |

  What Happens:

  Fill memory at address 0x00000000 with 327,680 copies of 0xF0F0F0F0

  - Each element is 4 bytes (word)
  - Total bytes written: 327,680 × 4 = 1,310,720 bytes (1.25 MB)
  - Memory range: 0x00000000 to 0x0013FFFF

  Memory After Execution:
```
  0x00000000: F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0
  0x00000010: F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0
  ... (repeating for 1.25 MB)
  0x0013FFF0: F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0 F0
```

  This looks like a memory initialization routine - filling a large block with a pattern (possibly to detect uninitialized memory later, since 0xF0F0F0F0 is a distinctive pattern).

---

### Verified Example 5: Halfword fill

**nd500-as input:**
```assembly
h bmove $0x1234,r1.(0),r2
```

**Machine code:** `FE 78 CE 12 34 F4 00 D1`

**nd500-dis output:**
```
00000000: FE 78 CE 12 34 F4 00 D1       h bmove      $0x1234,r1.(0x0),r2
```

**What happens:**
1. Source = constant value 0x1234 (halfword = 2 bytes)
2. Destination = address from I1
3. Count = from I2
4. Each element is 2 bytes, so count=10 fills 20 bytes

---

### Verified Example 6: Short constant count

**nd500-as input:**
```assembly
w bmove $0xFF,r1.(0),10
```

**Machine code:** `FE 79 CE 00 FF F4 00 C4 00 00 00 0A`

**nd500-dis output:**
```
00000000: FE 79 CE 00 FF F4 00 C4 00 00 00 0A       w bmove      $0xFF,r1.(0x0),$0xA
```

**Note:** The count `10` is encoded as `$0xA` (address 0x0A). For counts larger than 63, the assembler uses ABSOLUTE addressing which reads from memory. Use a register for the count instead.

---

## Operand Encoding Details

### Address Code Summary

| Address Code | Mode | Example | Meaning |
|--------------|------|---------|---------|
| 0x00-0x3F | Short constant | `4` | Value 0-63 embedded in opcode |
| 0xC4-0xC7 | Absolute | `$addr` | Read value from memory address |
| 0xCC | Double constant | `$value` | 8-byte immediate value |
| 0xCD | Byte constant | `$0xAB` | 1-byte immediate value |
| 0xCE | Half constant | `$0x1234` | 2-byte immediate value |
| 0xCF | Word constant | `$0xDEADBEEF` | 4-byte immediate value |
| 0xD0-0xD3 | Register | `r1`-`r4` | Value in I1-I4 |
| 0xF4-0xF7 | Pre-indexed (byte) | `r1.(0)` | IN + byte offset |
| 0xF8-0xFB | Pre-indexed (half) | `r1.(0x100)` | IN + halfword offset |
| 0xFC-0xFF | Pre-indexed (word) | `r1.(0x10000)` | IN + word offset |

---

## Common Mistakes

### Wrong: Using $value for large count
```assembly
w bmove $0xAB,r1.(0),$100    ; WRONG! Reads count from address 100
```

### Correct: Using register for count
```assembly
w bmove $0xAB,r1.(0),r2    ; Correct: count from I2
```

### Correct: Using short constant for count (0-63)
```assembly
w bmove $0xAB,r1.(0),10     ; Note: assembler may encode as $0xA
```

---

## C Equivalent

```c
// Fill mode (source is constant)
void bmove_fill(void* dest, uint32_t value, size_t count) {
    uint32_t* p = (uint32_t*)dest;
    for (size_t i = 0; i < count; i++) {
        p[i] = value;
    }
}

// Copy mode (source is memory)
void bmove_copy(void* dest, void* src, size_t count) {
    memcpy(dest, src, count * sizeof(uint32_t));
}
```
