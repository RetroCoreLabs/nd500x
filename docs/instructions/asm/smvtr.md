# SMVTR - String Move Translated

## Overview

**Mnemonic:** `smvtr`
**Function:** Copy string with character translation
**Class:** STRING
**Privilege:** user

**Format:** `BY SMVTR <source/r/BY/I1=>, <dest/w/BY/I2=>, <trans table/aa/BY>`

---

## Description

Copies bytes from a source string to a destination string while translating each byte through a 256-byte translation table. This instruction is essential for character set conversions (e.g., EBCDIC to ASCII), case conversions, and character mapping operations.

SMVTR reads bytes sequentially from the `<source>` operand using I1 as the index pointer. Each source byte value is used as an index into the translation table to retrieve the translated byte value. The translated value is then written to the `<dest>` operand at the position indicated by I2. Both I1 and I2 are automatically incremented after each byte transfer.

The operation continues until either the source string is exhausted or the destination buffer becomes full. The instruction handles overlapping source and destination regions correctly, ensuring data integrity during in-place transformations.

The translation table must be a 256-byte array where the source byte value serves as the index. For example, to convert byte value 0x41 ('A'), the instruction reads the translation value from address `<trans table>` + 0x41.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD74 | BY SMVTR |

---

## Operands

### Operand 1: `<source/r/BY/I1=>`

Source string to copy and translate.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the source string whose bytes will be read and translated. The I1 register acts as the index pointer and is incremented for each byte read.

### Operand 2: `<dest/w/BY/I2=>`

Destination buffer for translated bytes.

**Role:** Destination
**Access:** Write
**Addressing modes:** LOCAL, RECORD, PRE_INDEXED, ABSOLUTE

Specifies the destination buffer where translated bytes are written. The I2 register serves as the index pointer and is incremented for each byte written.

### Operand 3: `<trans table/aa/BY>`

Address of 256-byte translation table.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, PRE_INDEXED, ABSOLUTE

Provides the absolute address of a 256-byte translation table. Each source byte value (0-255) is used as an offset into this table to fetch the corresponding translated value.

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the source operand is addressed outside its valid bounds, or when the destination operand is addressed outside its valid bounds.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Set to 1 if destination full, cleared to 0 if source empty |
| Z | Zero | Undefined after operation |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: EBCDIC to ASCII conversion

```assembly
% Convert EBCDIC string to ASCII
CHARACTERS: % EBCDIC encoded string
EBCDIC2ASCII: % 256-byte translation table

        W1 CLR
        W2 CLR
        BY SMVTR CHARACTERS, CHARACTERS, EBCDIC2ASCII
        % CHARACTERS now contains ASCII
```

### Example 2: Uppercase to lowercase conversion

```assembly
% Convert string to lowercase using translation table
TO_LOWER:   % 256-byte table: 'A'-'Z' -> 'a'-'z', others unchanged

INPUT:      "HELLO WORLD"
OUTPUT:     (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVTR INPUT, OUTPUT, TO_LOWER
        % OUTPUT now contains "hello world"
```

### Example 3: Character substitution cipher

```assembly
% Simple substitution cipher using translation table
CIPHER_TABLE: % 256-byte Caesar cipher mapping

PLAINTEXT:  "SECRET MESSAGE"
CIPHERTEXT: (80 bytes)

ENCRYPT:
        W1 CLR
        W2 CLR
        BY SMVTR PLAINTEXT, CIPHERTEXT, CIPHER_TABLE
```

### Example 4: Control character filtering

```assembly
% Replace control characters with spaces
FILTER_TABLE: % 0x00-0x1F -> 0x20, others unchanged

        W1 CLR
        W2 CLR
        BY SMVTR B.INPUT, B.OUTPUT, FILTER_TABLE
        % Control chars now converted to spaces
```

### Example 5: In-place translation with overlap handling

```assembly
% Translate string in place (source = dest)
NORMALIZE:  % Translation table for normalization

BUFFER:     (100 bytes)

        W1 CLR
        W2 CLR
        BY SMVTR BUFFER, BUFFER, NORMALIZE
        % BUFFER translated in place - overlap handled
```

### Example 6: Hex digit conversion

```assembly
% Convert ASCII hex digits to binary values
HEX2BIN:    % '0'-'9' -> 0-9, 'A'-'F' -> 10-15

        W1 CLR
        W2 CLR
        BY SMVTR B.HEX_STRING, B.BIN_VALUES, HEX2BIN
```

### Example 7: Character class translation

```assembly
% Map characters to character classes
% Letters -> 1, Digits -> 2, Other -> 0
CLASSIFY:   % 256-byte classification table

TEXT:       "abc123!@#"
CLASSES:    (20 bytes)

        W1 CLR
        W2 CLR
        BY SMVTR TEXT, CLASSES, CLASSIFY
        % CLASSES = [1,1,1,2,2,2,0,0,0]
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0, I1 and I2 unmodified, DR trap
  - Outside dest: K=1, I1 and I2 unmodified, DR trap
  - Source empty: K=0, I1 and I2 point to next element
  - Dest full: K=1, I1 and I2 point to next element

- **Overlap Handling**: Source and destination can overlap - the instruction ensures correct byte-by-byte copying
- **Translation Table Size**: Must be exactly 256 bytes (one entry per possible byte value)
- **Performance**: O(n) where n is the number of bytes transferred
- **Typical Use**: Character set conversion, case mapping, encoding translation

---

## Reference Manual

**Section:** §14.5
**Title:** String move translated

---

## See Also

- [SMVTU](smvtu.md) - String move translated until escape
- [SMVUN](smvun.md) - String move until condition
- [SMVWH](smvwh.md) - String move while condition
