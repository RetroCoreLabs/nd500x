# SMOVN - String Move N Elements

## Overview

**Mnemonic:** `smovn`
**Function:** Move M elements with bounds checking
**Class:** STRING
**Privilege:** user

**Format:** `t SMOVN <source>, <dest>, <m>`

---

## Description

Moves a specified number (M) of elements from source to destination with descriptor-based bounds checking. Unlike SMOVE which moves until descriptor exhaustion, SMOVN moves exactly M elements or until bounds are exceeded. Handles overlapping regions correctly. The I1 and I2 registers track positions and auto-increment.

**Operation:**
```
0 → i
while not end of strings and i < m do:
    S(I1) → D(I2)
    I1 + 1 → I1
    I2 + 1 → I2
    i + 1 → i
endwhile
```

**Key Characteristics:**
- Explicit element count (M operand)
- I1 register points to source
- I2 register points to destination
- Descriptor bounds checking on both operands
- Handles overlapping memory regions
- Supports all data types: BI, BY, H, W, F, D

**Common Use Cases:**
- Fixed-size block copy
- Partial buffer transfer
- Array segment copying
- Safe memory copy with length limit
- Copying structures of known size

**Operands:** 3 (source via I1, dest via I2, count)
**Variants:** 6 opcodes (one per data type)

---

## Examples

### Example 1: Copy 64 bits

```assembly
        % Copy next 64 bits from S1 to start of S2
        W1 := S1
        W2 CLR
        W2 := S2
        BI SMOVN S1, S2, 64
```

**Explanation:** Bit-level copy of 64 bits between descriptors.

### Example 2: Copy fixed-size record

```assembly
        % Copy 100-byte record
        W1 := SOURCE_REC
        W2 := DEST_REC
        BY SMOVN SOURCE_REC, DEST_REC, 100
```

**Explanation:** Copy exactly 100 bytes regardless of descriptor size.

### Example 3: Partial array copy

```assembly
        % Copy first N elements
        W1 := ARRAY_SRC
        W2 := ARRAY_DST
        W3 := N
        W SMOVN ARRAY_SRC, ARRAY_DST, N
```

**Explanation:** Copy variable number of words.

### Example 4: Float buffer transfer

```assembly
        % Move 10 floats
        W1 := FLOAT_SRC
        W2 := FLOAT_DST
        F SMOVN FLOAT_SRC, FLOAT_DST, 10
```

**Explanation:** Copy specific number of floating-point values.

### Example 5: Safe bounded copy

```assembly
        % Copy with length limit
        W1 := INPUT
        W2 := BUFFER
        W3 := MAX_LEN
        BY SMOVN INPUT, BUFFER, MAX_LEN
        IF<>GO OVERFLOW        % Check if all copied
```

**Explanation:** Prevent buffer overflow with explicit limit.

---

## Terminating Conditions

| Condition | K | Z | I1, I2 | Result |
|-----------|---|---|--------|--------|
| Outside source (before move) | 0 | 0 | Unmodified | DR trap |
| Outside dest (before move) | 1 | 0 | Unmodified | DR trap |
| M items moved | 0 | 1 | Next element | Normal completion |
| Source empty (< M items) | 0 | 0 | Next element | Partial move |
| Dest full (< M items) | 1 | 0 | Next element | Partial move |

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **Descriptor range (DR)**: Operand outside descriptor bounds

---

## Data Status Bits

- **K (Termination)**: 0=source condition, 1=dest condition
- **Z (Zero)**: 1=M items moved successfully, 0=incomplete

---

## Reference Manual

**Section:** §14.7
**Title:** String move m elements

---

## See Also

- [SMOVE](smove.md) - String move (descriptor-bounded)
- [BMOVE](bmove.md) - Block move
- [SFILL](sfill.md) - String fill
- [SCOMP](scomp.md) - String compare
