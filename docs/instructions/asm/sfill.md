# SFILL - String Fill

## Overview

**Mnemonic:** `sfill`
**Function:** Fill string with register value
**Class:** STRING
**Privilege:** user

**Format:** `tn SFILL <dest>`

---

## Description

Fills every element of the destination string with the contents of the specified register (Rn). The I2 register points to the destination and is automatically incremented for each element written. Filling continues until the end of the destination descriptor is reached.

**Operation:**
```
while not end of string do:
    Rn → D(I2)
    I2 + 1 → I2
endwhile
```

**Key Characteristics:**
- Fills with register value (not source operand)
- I2 register points to destination
- Auto-increment of I2
- Descriptor-based bounds checking
- Supports all data types: BI, BY, H, W, F, D
- Efficient for array initialization

**Common Use Cases:**
- Zero-fill buffers
- Initialize arrays with default values
- Clear string/array to spaces
- Set memory blocks to constant value
- Prepare data structures

**Operands:** 1 (destination via I2, source is Rn)
**Variants:** 24 opcodes (4 per data type)

---

## Examples

### Example 1: Fill string with spaces

```assembly
        % Clear line buffer to spaces
        BY3 := 40B                % ASCII space
        W2 := STRING
        BY3 SFILL STRING
```

**Explanation:** Fill remaining characters of STRING with ASCII spaces.

### Example 2: Zero-fill array

```assembly
        % Initialize array to zero
        W1 CLR
        W2 := BUFFER
        W1 SFILL BUFFER
```

**Explanation:** Fill entire buffer with zeros.

### Example 3: Initialize float array

```assembly
        % Set all elements to 1.0
        F2 := 1.0
        W2 := FLOAT_ARRAY
        F2 SFILL FLOAT_ARRAY
```

**Explanation:** Initialize floating-point array with 1.0.

### Example 4: Clear bit array

```assembly
        % Set all bits to 0
        BI1 CLR
        W2 := BIT_BUFFER
        BI1 SFILL BIT_BUFFER
```

**Explanation:** Clear bit string to all zeros.

### Example 5: Pattern fill with constant

```assembly
        % Fill with pattern byte 0xAA
        BY4 := 0AAH
        W2 := TEST_BUFFER
        BY4 SFILL TEST_BUFFER
```

**Explanation:** Memory test pattern fill.

---

## Terminating Conditions

| Condition | K | I2 | Result |
|-----------|---|-----|--------|
| Outside dest (before fill) | 1 | Unmodified | DR trap |
| String filled | 1 | Next element | Normal completion |

---

## Trap Conditions

- **Addressing traps**: Invalid destination address
- **Descriptor range (DR)**: I2 outside destination descriptor bounds

---

## Data Status Bits

- **K (Termination)**: 1 when operation completes (dest full or outside bounds)

---

## Reference Manual

**Section:** §14.8
**Title:** String Fill

---

## See Also

- [SFILLN](sfilln.md) - String fill N elements
- [SMOVE](smove.md) - String move
- [SMOVN](smovn.md) - String move N elements
- [CLR](clr.md) - Clear register
