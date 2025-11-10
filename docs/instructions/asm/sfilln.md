# SFILLN - String Fill N Elements

## Overview

**Mnemonic:** `sfilln`
**Function:** Fill n string elements with register value
**Class:** STRING
**Privilege:** user

**Format:** `tn SFILLN <dest>, <m>`

---

## Description

Fills up to m elements of a destination string with the contents of a specified register. Starting at the position indicated by register I2, the register value is stored repeatedly into consecutive string elements until either m elements have been filled or the end of the destination string is reached.

**Operation:**
```
0 → i
while not end of string and i < m do:
    Rn → D(I2)
    I2 + 1 → I2
    i + 1 → i
endwhile
```

**Key Characteristics:**
- Fills destination string with register value
- I2 register points to destination and auto-increments
- Stops at m elements or end of string
- m (count) is unsigned
- Supports all data types (BI, BY, H, W, F, D)

**Common Use Cases:**
- Initializing arrays with constant values
- Clearing buffers (fill with 0)
- Setting memory patterns for testing
- Initializing data structures
- Padding strings with specific values

**Operands:** 2 (destination string via I2, count m)
**Variants:** 24 opcodes (6 data types × 4 registers)

---

## Variants

| Data Type | Opcodes | Assembly |
|-----------|---------|----------|
| BI | 0xFD94-0xFD97 | BIn SFILLN |
| BY | 0xFD98-0xFD9B | BYn SFILLN |
| H | 0xFD9C-0xFD9F | Hn SFILLN |
| W | 0xFDA0-0xFDA3 | Wn SFILLN |
| F | 0xFDA4-0xFDA7 | Fn SFILLN |
| D | 0xFDA8-0xFDAB | Dn SFILLN |

---

## Examples

### Example 1: Clear 100 bytes

```assembly
        % Clear buffer to zero
        W1 := 0
        W2 := BUFFER
        BY1 SFILLN W2, 100
```

**Explanation:** Initialize buffer with zeros.

### Example 2: Fill array with value

```assembly
        % Fill array with pattern
        W3 := 0xFFFF
        W2 := ARRAY
        W3 SFILLN W2, ARRAY_SIZE
```

**Explanation:** Set all array elements to specific value.

### Example 3: Initialize structure

```assembly
        % Init struct fields to -1
        W4 := -1
        W2 := STRUCT_BASE
        W4 SFILLN W2, FIELD_COUNT
```

**Explanation:** Initialize structure fields.

### Example 4: Memory pattern test

```assembly
        % Fill test buffer with pattern
        W1 := 0xA5A5
        W2 := TEST_BUF
        H1 SFILLN W2, 1000
```

**Explanation:** Fill memory with test pattern for diagnostics.

### Example 5: Padding string

```assembly
        % Pad string with spaces
        BY2 := ' '
        W2 := TEXT_END
        BY2 SFILLN W2, PAD_LENGTH
```

**Explanation:** Pad text with space characters.

---

## Performance Notes

- **Execution:** 5-10 cycles + (1-2 cycles per element filled)
- **Optimization:** More efficient than loop of individual moves

**Usage recommendations:**
- Use for bulk initialization
- Efficient for clearing/setting arrays
- Check termination conditions via flags
- Useful for memory testing

---

## Reference Manual

**Section:** §14.9
**Title:** String fill n elements

---

## See Also

- [SFILL](sfill.md) - String fill (all elements)
- [SMOVE](smove.md) - String move
- [CLR](clr.md) - Clear register
