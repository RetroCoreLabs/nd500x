# SCOMP - String Compare

## Overview

**Mnemonic:** `scomp`
**Function:** Compare byte strings with descriptor bounds checking
**Class:** STRING
**Privilege:** user

**Format:** `BY SCOMP <source-1>, <source-2>`

---

## Description

Compares two byte strings element-by-element using descriptor-based bounds checking. Comparison continues until unequal bytes are found or the end of either string is reached. Index registers I1 and I2 track current positions and are automatically incremented. The byte elements are treated as unsigned values.

**Operation:**
```
while not end of strings and S(I1) = D(I2) do:
    I1 + 1 → I1
    I2 + 1 → I2
endwhile
```

**Key Characteristics:**
- Descriptor-based bounds checking
- I1 register points to source-1 string
- I2 register points to source-2 string
- Bytes treated as unsigned values
- Auto-increment of index registers
- Sets K, Z, S flags for termination condition

**Common Use Cases:**
- String equality testing
- Password verification
- Input validation against expected values
- Lexicographic string comparison
- Database key comparison
- Command parsing

**Operands:** 2 (both via index registers I1, I2)
**Variants:** 1 opcode

---

## Examples

### Example 1: Simple string comparison

```assembly
        % Compare user input against command
        W1 := INPUTLINE
        W2 := B.COMMAND
        BY SCOMP INPUTLINE, B.COMMAND
        IF><GO MATCH
```

**Explanation:** Compare input buffer with expected command string.

### Example 2: Password verification

```assembly
        % Verify password
        W1 := USER_INPUT
        W2 := STORED_HASH
        BY SCOMP USER_INPUT, STORED_HASH
        IF><GO AUTH_FAILED
        % Password matches
```

**Explanation:** Compare entered password with stored value.

### Example 3: Search loop with comparison

```assembly
        % Search for matching string in table
        W2 CLR
SEARCH: W1 := SEARCH_KEY
        W2 := TABLE_ENTRIES(W2)
        BY SCOMP SEARCH_KEY, TABLE_ENTRIES(W2)
        IF=GO FOUND
        W2 ADD 1
        W2 COMP TABLE_SIZE
        IF<GO SEARCH
```

**Explanation:** Linear search through string table.

### Example 4: Validate expected data format

```assembly
        % Check file header signature
        W1 := FILE_HEADER
        W2 := MAGIC_BYTES
        BY SCOMP FILE_HEADER, MAGIC_BYTES
        IF><GO INVALID_FORMAT
```

**Explanation:** Verify file begins with correct magic bytes.

### Example 5: Lexicographic ordering

```assembly
        % Sort strings: compare NAME1 vs NAME2
        W1 := NAME1
        W2 := NAME2
        BY SCOMP NAME1, NAME2
        IF<GO NAME1_FIRST    % NAME1 < NAME2
        IF=GO EQUAL          % NAME1 = NAME2
        GO NAME2_FIRST       % NAME1 > NAME2
```

**Explanation:** Determine alphabetical order of two strings.

---

## Terminating Conditions

| Condition | K | Z | S | I1, I2 | Result |
|-----------|---|---|---|--------|--------|
| Both operands outside string | 0 | 1 | 0 | Unmodified | DR trap |
| Exact match | 0 | 1 | 0 | Next element | Strings equal |
| source-1 longer | 0 | 0 | 0 | Next element | source-1 > source-2 |
| source-2 longer (greater byte) | 0 | 0 | 1 | Next element | source-2 > source-1 |
| Smaller byte in source-1 | 1 | 0 | 0 | Differing element | source-1 < source-2 |
| Greater byte in source-1 | 1 | 0 | 1 | Differing element | source-1 > source-2 |

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **Descriptor range (DR)**: Both operands outside string bounds

---

## Data Status Bits

- **K (Termination)**: 0=length mismatch, 1=byte difference found
- **Z (Zero)**: 1=exact match, 0=difference found
- **S (Sign)**: Indicates comparison result (see table above)

---

## Reference Manual

**Section:** §14.10
**Title:** String compare

---

## See Also

- [SCOTR](scotr.md) - String compare translated
- [SCOPT](scopt.md) - String compare translated with pad
- [SCOPA](scopa.md) - String compare with pad
- [SMOVE](smove.md) - String move
- [COMP2](comp2.md) - Compare two operands
