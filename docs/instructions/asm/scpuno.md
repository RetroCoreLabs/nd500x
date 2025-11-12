# SCPUNO - String Copy Until

## Overview

**Mnemonic:** `scpuno`
**Function:** Copy string until specified element found
**Class:** STRING
**Privilege:** user

**Format:** `BY SCPUNO <src>, <dest>, <test>`

---

## Description

Copies bytes from source string to destination string, stopping when a byte matching the test value is encountered or end of string is reached. The I1 and I2 registers track source and destination positions and auto-increment during the copy.

**Operation:**
```
while not end of strings and S(I1) ≠ <test> do:
    S(I1) → D(I2)
    I1 + 1 → I1
    I2 + 1 → I2
endwhile
```

**Key Characteristics:**
- Copies until delimiter found or end reached
- I1 register points to source
- I2 register points to destination
- Both I1 and I2 auto-increment
- Stops at test byte (delimiter not copied)

**Common Use Cases:**
- Copying strings up to delimiter
- Tokenizing text by delimiter
- Extracting fields from delimited data
- Parsing structured text formats

**Operands:** 3 (source via I1, dest via I2, test value)
**Variants:** 1 opcode (BY only)

---

## Examples

### Example 1: Copy until space

```assembly
        % Copy word until space
        W1 := TEXT_IN
        W2 := TEXT_OUT
        BY SCPUNO W1, W2, ' '
```

**Explanation:** Extract first word from text.

### Example 2: Copy until delimiter

```assembly
        % Extract CSV field
        W1 := CSV_LINE
        W2 := FIELD_BUFFER
        BY SCPUNO W1, W2, ','
```

**Explanation:** Parse comma-separated values.

### Example 3: Copy filename until extension

```assembly
        % Get basename from filename
        W1 := FILENAME
        W2 := BASENAME
        BY SCPUNO W1, W2, '.'
```

**Explanation:** Extract filename without extension.

### Example 4: Copy until null terminator

```assembly
        % Copy C-string
        W1 := C_STR_SRC
        W2 := C_STR_DEST
        BY SCPUNO W1, W2, 0
```

**Explanation:** String copy stopping at null.

### Example 5: Extract path component

```assembly
        % Copy until path separator
        W1 := FULL_PATH
        W2 := COMPONENT
        BY SCPUNO W1, W2, '/'
```

**Explanation:** Parse file path components.

---

## Reference Manual

**Section:** §14 (String operations)

---

## See Also

- [SCOPY](scopy.md) - String copy
- [SLOCA](sloca.md) - String locate element
- [SCOMP](scomp.md) - String compare
