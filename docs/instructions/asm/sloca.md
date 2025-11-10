# SLOCA - String Locate Element

## Overview

**Mnemonic:** `sloca`
**Function:** Locate element in string
**Class:** STRING
**Privilege:** user

**Format:** `t SLOCA <source>, <test>`

---

## Description

Searches through a string element by element until finding an element equal to the test value or reaching the end of the string. The I1 register points to the source string and is automatically incremented during the search.

**Operation:**
```
while not end of string and S(I1) ≠ <test> do:
    I1 + 1 → I1
endwhile
```

**Key Characteristics:**
- Searches string for matching element
- I1 register automatically incremented during search
- Stops at first match or end of string
- Sets K and Z flags based on result
- Supports bit and byte element types

**Terminating conditions:**
- **Outside source:** K=0, Z=1, I1 unmodified, DR trap
- **Element = test:** K=0, Z=1, I1 points to found element
- **Source empty:** K=0, Z=0, I1 points to next element

**Common Use Cases:**
- Searching strings for specific characters
- Finding delimiters in text
- Locating special bytes in data streams
- String parsing and tokenization
- Pattern matching in buffers

**Operands:** 2 (source string via I1, test value)
**Variants:** 2 opcodes (BI, BY)

---

## Variants

Total variants: 2

| Variant | Opcode | Data Type | Assembly |
|---------|--------|-----------|----------|
| 1/2 | 0xFDAF | BI | BI SLOCA |
| 2/2 | 0xFDB0 | BY | BY SLOCA |

---

## Operands

### Operand 1 (Source)

**Type:** String (BI or BY elements)
**Access:** Read (via I1 register)

String to search. I1 register must point to start of string. I1 is automatically incremented during search.

### Operand 2 (Test)

**Type:** BI or BY (matching source type)
**Access:** Read

Value to search for in the string.

**Supported modes:**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **CONSTANT** - Immediate value
- **REGISTER** - Register
- **PRE_INDEXED** - Indexed access
- **ABSOLUTE** - Absolute address

---

## Trap Conditions

- **Data range (DR):** I1 points outside source string bounds

---

## Data Status Bits

- **K:** Set to 0
- **Z:** Set to 1 if element found or outside source, cleared if source empty

---

## Examples

### Example 1: Find space character in string

```assembly
        % Search for space in text
        W1 := TEXT_STRING
        BY1 SLOCA W1, ' '
        IF=GO FOUND_SPACE
        % No space found
        GO NO_SPACE
FOUND_SPACE:
        % I1 points to space character
```

**Explanation:** Locating first space character in a text string.

### Example 2: Find newline delimiter

```assembly
        % Search for newline
        W1 := LINE_BUFFER
        BY1 SLOCA W1, 0x0A          % '\n'
        IF><GO NO_NEWLINE
        % Found newline at I1
```

**Explanation:** Finding line terminator in buffer.

### Example 3: Locate bit pattern

```assembly
        % Find specific bit in bit array
        W1 := BIT_ARRAY
        BI1 SLOCA W1, 1
        IF=GO BIT_FOUND
```

**Explanation:** Searching for set bit in bit array.

### Example 4: Parse comma-separated values

```assembly
        % Find next comma
        W1 := CSV_LINE
PARSE_LOOP:
        BY1 SLOCA W1, ','
        IF><GO LAST_FIELD
        % Process field up to I1
        CALL EXTRACT_FIELD
        W1 := I1
        W1 INC                      % Skip comma
        GO PARSE_LOOP
LAST_FIELD:
```

**Explanation:** Parsing CSV by finding comma delimiters.

### Example 5: Search in loop with range check

```assembly
        % Safe string search with bounds
        W2 := START_POS
        W3 := END_POS
SEARCH:
        W1 := BUFFER
        BY1 SLOCA W1, TARGET_BYTE
        W TEST I1
        IF=GO NOT_FOUND
        W1 := I1
        W1 COMP W3
        IF>GO OUT_OF_RANGE
        % Found within range
```

**Explanation:** Search with explicit range checking.

### Example 6: Find null terminator

```assembly
        % Locate end of C-style string
        W1 := C_STRING
        BY1 SLOCA W1, 0
        % I1 now points to null terminator
        W2 := I1
        W2 SUB C_STRING             % Length of string
```

**Explanation:** Finding null terminator to calculate string length.

### Example 7: Pattern search in binary data

```assembly
        % Find marker byte in data stream
        W1 := DATA_STREAM
        BY1 SLOCA W1, MARKER_BYTE
        IF><GO MARKER_NOT_FOUND
        % Process data at I1
        CALL PROCESS_MARKER
```

**Explanation:** Locating marker byte in binary protocol.

---

## Performance Notes

- **Execution:** 5-10 cycles + (2-3 cycles per element examined)
- **Best case:** Match on first element (5-10 cycles)
- **Worst case:** No match, entire string scanned
- **Average:** Depends on string length and match position

**Usage recommendations:**
- Efficient for short to medium strings
- Use for delimiter and pattern finding
- I1 register automatically tracks position
- Check Z flag to determine success
- Handle DR trap for out-of-bounds

**Comparison with related instructions:**
- `SLOCA` vs `SCOMP`: SLOCA finds single element, SCOMP compares strings
- `SLOCA` vs `SSCAN`: SLOCA finds match, SSCAN scans with condition
- Efficient for single-character search
- Part of ND-500 string instruction set

---

## Reference Manual

**Section:** §14.15
**Title:** String locate element

---

## See Also

- [SCOMP](scomp.md) - String compare
- [SSCAN](sscan.md) - String scan
- [SCPUNO](scpuno.md) - String copy until
- [String Operations](../ND500_STRING_OPERATIONS.md)
