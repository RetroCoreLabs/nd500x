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

**Two Operating Modes:**

1. **Block Copy** (source is memory): Copies `count` elements from source array to destination array
2. **Block Fill** (source is register/constant): Fills destination with `count` copies of source value

The instruction automatically handles overlapping regions correctly, choosing forward or backward copy direction as needed. This makes BMOVE safe for moving data within the same buffer.

**Operation:**
```
if source is register/constant:
    for i = 0 to count-1:
        source_value → dest[i]
else:
    for i = 0 to count-1:
        source[i] → dest[i]  (with overlap handling)
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

**All flags cleared** (Z, S, C, V all → 0)

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

**Section:** §15.1
**Title:** Block Move and Fill

---

## See Also

- [SMOVE](smove.md) - String move
- [SFILL](sfill.md) - String fill
- [:=](assignto.md) - Single element assignment
- [CLR](clr.md) - Clear register
