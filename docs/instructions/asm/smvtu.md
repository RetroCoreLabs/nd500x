# SMVTU - String Move Translated Until

## Overview

**Mnemonic:** `smvtu`
**Function:** Copy string with translation until escape character
**Class:** STRING
**Privilege:** user

**Format:** `BY SMVTU <source/r/BY/I1=>, <dest/w/BY/I2=>, <trans table/aa/BY>`

---

## Description

Copies bytes from a source string to a destination string with character translation, stopping when an escape character (ASCII ESC = 0x1B = 27 decimal) is encountered in the translated output. Zero bytes in the translated output are skipped (not copied) but do not terminate the operation.

**Operation:**
```
for each byte in source[I1..end]:
    translated = trans_table[byte]
    if (translated == 0x1B): Z=1, stop  // ESC found
    if (translated == 0x00): I1++, continue  // Skip zero
    dest[I2] = translated
    I1++, I2++
K = (dest full) ? 1 : 0
```

**Key Characteristics:**
- String copy with translation, ESC termination, and null filtering
- Uses 256-byte translation table
- Stops when translated byte = ESC (0x1B), sets Z=1
- Skips when translated byte = 0x00 (advance I1 only)
- K=1 if destination full, K=0 otherwise
- ESC character not copied to destination
- Zero translations allow character deletion
- Does NOT handle overlap (requires separate buffers)
- Essential for escape-delimited record parsing

SMVTU combines translation, filtering, and termination detection in a single operation, making it ideal for processing escaped strings, removing null characters, and performing conditional character transformations.

The operation proceeds as follows for each source byte:
1. Read source byte at I1
2. Translate through table to get output byte
3. If translated byte equals ASCII ESC (0x1B): stop, set Z=1
4. If translated byte equals zero (0x00): skip to next source byte (increment I1 only)
5. Otherwise: write translated byte to destination, increment both I1 and I2

The escape character itself is never copied to the destination. Zero-valued translations cause the source pointer to advance without copying, allowing for character deletion during translation.

This instruction does NOT handle overlap correctly - source and destination regions must not overlap.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD75 | BY SMVTU |

---

## Operands

### Operand 1: `<source/r/BY/I1=>`

Source string to copy and translate.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the source string. The I1 register is used as index pointer and is incremented for each byte processed (whether copied or skipped).

### Operand 2: `<dest/w/BY/I2=>`

Destination buffer for translated bytes.

**Role:** Destination
**Access:** Write
**Addressing modes:** LOCAL, RECORD, PRE_INDEXED, ABSOLUTE

Specifies the destination buffer. The I2 register serves as index pointer and is incremented only when bytes are actually written (non-zero, non-escape translations).

### Operand 3: `<trans table/aa/BY>`

Address of 256-byte translation table.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, PRE_INDEXED, ABSOLUTE

Provides the absolute address of the translation table. Source byte values index into this table to retrieve translated values.

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the source operand is addressed outside its valid bounds, or when the destination operand is addressed outside its valid bounds.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Set to 1 if destination full, cleared to 0 otherwise |
| Z | Zero | Set to 1 if escape character found, cleared to 0 otherwise |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Remove NULs and convert to uppercase

```assembly
% Remove ASCII NULs and translate to uppercase
UPPERCASETABLE: % 'a'-'z' -> 'A'-'Z', NUL -> 0, ESC -> 0x1B

TEXT:   "hello\0\0world\x1B"  % String with embedded NULs
TEXT2:  (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVTU TEXT, TEXT2, UPPERCASETABLE
        % TEXT2 = "HELLOWORLD", stopped at ESC
        % I1 points to ESC position
```

### Example 2: Parse escape-terminated field

```assembly
% Extract field terminated by ESC, filtering control chars
FILTER_ESC: % Control chars (0x00-0x1F) -> 0, ESC -> 0x1B

RECORD:
        .NAME:  (40 bytes)

INPUT:  "John\x03\x05Smith\x1BAge:30"

        W1 CLR
        W2 CLR
        BY SMVTU INPUT, R.NAME, FILTER_ESC
        IF=Z GO VALID           % ESC found - valid field
        % No ESC - malformed input
VALID:
        % R.NAME = "JohnSmith" (control chars removed)
```

### Example 3: Convert with character deletion

```assembly
% Translate and delete unwanted characters
DELETE_PUNCT: % Punctuation -> 0 (delete), ESC -> 0x1B

SOURCE: "Hello, World! Done.\x1B"
DEST:   (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVTU SOURCE, DEST, DELETE_PUNCT
        % DEST = "Hello World Done"
        % Punctuation deleted, stopped at ESC
```

### Example 4: Escape-delimited record parsing

```assembly
% Parse escape-delimited fields from input stream
FIELDS:
        .FIELD1: (20 bytes)
        .FIELD2: (20 bytes)
        .FIELD3: (20 bytes)

STREAM: "First\x1BSecond\x1BThird\x1B"

PARSE:
        W1 CLR
        W2 CLR
        BY SMVTU STREAM, FIELDS.FIELD1, IDENTITY
        W INCR I1               % Skip ESC
        W2 CLR
        BY SMVTU STREAM, FIELDS.FIELD2, IDENTITY
        W INCR I1
        W2 CLR
        BY SMVTU STREAM, FIELDS.FIELD3, IDENTITY
```

### Example 5: Local buffer with null filtering

```assembly
PROCESS: ENTS 100
        % Remove embedded nulls, uppercase, stop at ESC
        W1 CLR
        W2 CLR
        BY SMVTU B.INPUT, B.OUTPUT, TO_UPPER
        IF<>Z GO NO_TERMINATOR
        % Normal termination at ESC
        % B.OUTPUT contains cleaned string
        RET
```

### Example 6: Translation with completion detection

```assembly
% Translate record field until escape or buffer full
CONVERT_TABLE: % Application-specific translation

        W1 CLR
        W2 CLR
        BY SMVTU R.SOURCE, B.BUFFER, CONVERT_TABLE
        IF=Z GO ESC_FOUND       % Escape terminated
        IF=K GO BUFFER_FULL     % Destination full
        % Source exhausted normally
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0 Z=0, I1 and I2 unmodified, DR trap
  - Outside dest: K=1 Z=0, I1 and I2 unmodified, DR trap
  - Escape found: K=0 Z=1, I1 and I2 point to escape position
  - Source empty: K=0 Z=0, I1 and I2 point to next element
  - Dest full: K=1 Z=0, I1 and I2 point to next element

- **Zero Handling**: Translated zeros advance I1 but not I2 (character deletion)
- **Escape Character**: ASCII ESC = 0x1B (27 decimal, 033 octal)
- **Overlap**: Does NOT handle overlap - use separate source and destination
- **Performance**: O(n) where n is source bytes processed
- **Typical Use**: Parsing delimited records, filtering with translation, escape sequence processing

---

## Reference Manual

**Section:** §14.6
**Title:** String move translated until

---

## See Also

- [SMVTR](smvtr.md) - String move translated (no escape handling)
- [SMVUN](smvun.md) - String move until mask condition
- [SMVWH](smvwh.md) - String move while mask condition
