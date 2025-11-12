# SMATCH - String Match

## Overview

**Mnemonic:** `smatch`
**Function:** Find substring within string
**Class:** STRING
**Privilege:** user

**Format:** `BY SMATCH <substring/r/BY/I1=>, <string/r/BY/I2=>`

---

## Description

Searches for the first occurrence of a substring within a string. The instruction scans through the `<string>` operand byte by byte, comparing each position against the `<substring>` pattern, until a match is found or the end of the string is reached.

**Operation:**
```
for each position in string:
    if (substring matches at position):
        I2 = position
        Z = 1
        return
I2 = end of string
Z = 0
```

**Key Characteristics:**
- Substring search within string (byte-by-byte comparison)
- Uses I1 (substring index) and I2 (string index) implicitly
- Z=1 when substring found, Z=0 when not found
- I2 points to match position or end of string
- K flag always cleared to 0
- DR trap on descriptor range violation
- O(n×m) worst-case complexity
- Essential for text parsing and pattern matching
- Used for delimiter searching and token recognition

SMATCH is designed for efficient text processing and pattern searching tasks common in string manipulation, parsing, and data validation. The instruction uses the I1 and I2 registers as implicit index pointers, with I1 indexing the substring and I2 indexing the string being searched.

The substring comparison is performed sequentially starting from the current I2 position. If a match is found, the Z flag is set to 1 and I2 points to the first matching byte position. If no match is found before the end of the string, Z is cleared to 0 and I2 points to the next element position (end of string).

The I1 register remains unmodified throughout the operation, as it serves only as a base pointer for the substring operand.

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB3 | BY SMATCH |

---

## Operands

### Operand 1: `<substring/r/BY/I1=>`

Source substring pattern to search for.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

This operand specifies the pattern to search for within the string. The I1 register serves as the base index pointer for accessing substring bytes. The length of the substring is determined by its descriptor.

### Operand 2: `<string/r/BY/I2=>`

String to search within.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

This operand specifies the string to be searched. The I2 register is used as an index pointer and is updated during the search operation. When a match is found, I2 points to the first byte of the match within the string.

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the substring operand is addressed outside its valid bounds, or when the string operand is addressed outside its valid bounds. Both operands outside bounds is treated as substring found (Z=1). String outside with valid substring is treated as not found (Z=0).

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Always cleared to 0 |
| Z | Zero | Set to 1 if substring found, cleared to 0 if not found |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Find comma delimiter

```assembly
% Search for comma delimiter in parameter string
        W1 CLR                  % Reset substring index
        W2 CLR                  % Start from beginning
        BY SMATCH COMMA, PARAMETERS
        IF=Z GO FOUND           % Branch if comma found
        % Not found - handle error
        GO ERROR
FOUND:
        % I2 now points to comma position
        % Extract substring before comma
```

### Example 2: Find keyword in buffer

```assembly
% Search for "END" keyword in input buffer
KEYWORD: "END"
BUFFER:  % Input buffer with descriptor

SEARCH:
        W1 CLR
        W2 CLR
        BY SMATCH KEYWORD, BUFFER
        IF<>Z GO NOT_FOUND
        % Keyword found at position I2
        W MOVE I2, B.POSITION
```

### Example 3: Multiple occurrence search

```assembly
% Find all occurrences of pattern in text
PATTERN: "ERROR"
TEXT:    % Text buffer to search

LOOP:   W1 CLR
        BY SMATCH PATTERN, TEXT
        IF<>Z GO DONE           % No more matches
        % Found occurrence
        W INCR B.COUNT          % Increment counter
        W INCR I2               % Move past this match
        GO LOOP                 % Continue searching
DONE:
```

### Example 4: Record-addressed string search

```assembly
% Search within record-addressed string field
RECORD:
        .NAME:   (80 bytes)
        .VALUE:  (40 bytes)

        W1 CLR
        W2 CLR
        BY SMATCH SEARCH_KEY, R.VALUE
        IF=Z GO MATCH_FOUND
```

### Example 5: Local variable string comparison

```assembly
SEARCH: ENTS 100
        % Search pattern in local buffer
        W1 CLR
        W2 CLR
        BY SMATCH B.PATTERN, B.LINE
        IF<>Z GO NO_MATCH
        % Match found - get position
        W MOVE I2, I3           % Save position
        RET
```

---

## Performance Notes

- **Termination Conditions**:
  - Substring outside bounds: K=0 Z=1, I2 unmodified, DR trap
  - String outside bounds: K=0 Z=0, I2 unmodified, DR trap
  - Substring found: K=0 Z=1, I2 points to first matching byte
  - String exhausted: K=0 Z=0, I2 points to next element (end)

- **Complexity**: O(n×m) worst case, where n is string length and m is substring length
- **Typical Use**: Command line parsing, delimiter searching, token recognition

---

## Reference Manual

**Section:** §14.18
**Title:** String match

---

## See Also

- [SSKIP](sskip.md) - Skip matching elements
- [SSCAN](sscan.md) - Scan string with translation
- [SSPAN](sspan.md) - Span while condition met
