# SMOVE - String Move

## Overview

**Mnemonic:** `smove`
**Function:** Move string with descriptor bounds checking
**Class:** STRING
**Privilege:** user

**Format:** `t SMOVE <source>, <dest>`

---

## Description

Moves elements from source string to destination string using descriptor-based bounds checking. Elements are copied one at a time using index registers I1 (source) and I2 (destination) until source is exhausted or destination is full. Handles overlapping regions correctly.

This is a descriptor-aware string operation that provides automatic bounds checking and termination. Unlike BMOVE which requires explicit counts, SMOVE uses descriptor limits to determine when to stop, making it safer for dynamic string operations.

**Operation:**
```
while (not end of source) and (not end of dest):
    source[I1] → dest[I2]
    I1 + 1 → I1
    I2 + 1 → I2
```

**Key Characteristics:**
- Descriptor-aware string copy with automatic bounds checking
- 6 data types supported (BI, BY, H, W, F, D)
- Uses I1/I2 as implicit index registers
- K flag indicates termination reason (0=source empty, 1=dest full)
- Safer than BMOVE (bounds checking vs explicit count)
- Handles overlapping regions correctly
- DR trap on descriptor range violation
- Essential for safe dynamic string operations
- Variable execution time based on string length

**Termination Conditions:**
- Source empty: K=0, I1/I2 point to next element
- Dest full: K=1, I1/I2 point to next element
- Outside source bounds: K=0, trap (DR)
- Outside dest bounds: K=1, trap (DR)

**Common Use Cases:**
- Safe string copying with bounds checking
- Array transfers between descriptors
- Dynamic buffer management
- String manipulation with overflow detection

**Operands:** 2
**Variants:** 6 opcodes (BI, BY, H, W, F, D)

---

## Variants

| Variant | Opcode | Data Type | Assembly Notation |
|---------|--------|-----------|-------------------|
| 1/6 | 0xFD66 | Bit | BI SMOVE |
| 2/6 | 0xFD67 | Byte | BY SMOVE |
| 3/6 | 0xFD68 | Halfword | H SMOVE |
| 4/6 | 0xFD69 | Word | W SMOVE |
| 5/6 | 0xFD6A | Float | F SMOVE |
| 6/6 | 0xFD6B | Double | D SMOVE |

---

## Operands

**Operand 1** (Source, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix
- **Role**: Source string descriptor (indexed by I1)

**Operand 2** (Destination, Write):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Matches instruction prefix
- **Role**: Destination string descriptor (indexed by I2)

**Result**: Elements copied, I1/I2 updated, K flag set based on termination

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Descriptor range (DR)**: Access outside descriptor bounds

---

## Data Status Bits

- **K (User flag)**: 0=source empty, 1=dest full
- **Z, S, C, V**: Unaffected

---

## Examples

### Example 1: Copy string with descriptors

```assembly
% Copy from source to dest descriptor
        W1 CLR          % Start at index 0
        W2 CLR
        BY SMOVE SRC_DESC, DEST_DESC
```

### Example 2: Partial copy with continuation

```assembly
% Copy until destination full
        W1 CLR
        W2 CLR
        BY SMOVE SOURCE, BUFFER
        IFKGO BUFFER_FULL
        % Source exhausted
BUFFER_FULL:
        % Destination filled
```

### Example 3: Array descriptor copy

```assembly
% Copy array via descriptors
        W1 CLR
        W2 CLR
        W SMOVE IND(B.DATABLOCK), B.COPY
```

### Example 4: String concatenation

```assembly
% Append to existing string
        W1 CLR          % Start of source
        W2 MOVE DEST_LEN, I2  % End of dest
        BY SMOVE SOURCE, DEST
```

### Example 5: Safe buffer copy

```assembly
% Copy with overflow detection
        W1 CLR
        W2 CLR
        BY SMOVE INPUT, OUTPUT
        IFKGO OVERFLOW_HANDLER
        % Copied successfully
```

### Example 6: Float array transfer

```assembly
% Copy float descriptor array
        W1 CLR
        W2 CLR
        F SMOVE FLOAT_SRC, FLOAT_DEST
```

### Example 7: Multi-part string processing

```assembly
% Process string in chunks
LOOP:
        BY SMOVE SOURCE, CHUNK_BUFFER
        CALL PROCESS_CHUNK
        IF-KGO LOOP     % Continue if source not empty
```

---

## Performance Notes

- **Size**: 3-4 bytes (opcode + 2 operands)
- **Execution**: Variable (depends on string length)
- **vs BMOVE**: SMOVE slower but provides bounds checking
- **Descriptor Overhead**: Bounds checks add cycles per element
- **Use Case**: When safety/bounds checking required
- **Overlap**: Automatically handled correctly

---

## Reference Manual

**Section:** §14.2
**Title:** String Move

---

## See Also

- [BMOVE](bmove.md) - Block move (explicit count)
- [SMOVN](smovn.md) - String move N elements
- [SFILL](sfill.md) - String fill
- [SCOMP](scomp.md) - String compare
