# SSCAN - String Scan

## Overview

**Mnemonic:** `sscan`
**Function:** Scan string with translation and mask test
**Class:** STRING
**Privilege:** user

**Format:** `BY SSCAN <source/r/BY/I1=>, <mask/r/BY>, <trans table/aa/BY>`

---

## Description

Scans through a string, translating each byte via a translation table and testing the result against a bit mask, searching for the first byte where `(translated_byte AND mask) != 0`. The source string is examined byte by byte until the masked condition is satisfied or the end of the string is reached.

SSCAN combines character translation with conditional searching, enabling efficient searches based on character classes, properties, or categories defined in the translation table. The translation step allows mapping characters to property values (e.g., character class codes) before testing.

The operation proceeds as follows for each byte:
1. Read byte from source at position I1
2. Translate byte through table: `translated = table[byte]`
3. Test: `translated AND <mask>`
4. If result is zero: increment I1, continue scanning
5. If result is non-zero: stop, set Z=0, I1 points to found element

The scan terminates successfully (Z=0) when a byte's translated value ANDed with the mask produces a non-zero result. If the entire string is scanned without finding such a byte, Z=1 is set and I1 points past the end of the string.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB1 | BY SSCAN |

---

## Operands

### Operand 1: `<source/r/BY/I1=>`

Source string to scan.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the string to scan. The I1 register serves as the index pointer and is incremented during scanning. When a matching byte is found, I1 points to that element.

### Operand 2: `<mask/r/BY>`

Bit mask applied to translated bytes.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the bit mask ANDed with each translated byte. The scan succeeds when the result is non-zero. Common patterns:
- `0xFF`: Any non-zero translation
- `0x80`: Check high bit
- `0x0F`: Check low nibble
- Specific bits: Check character properties encoded in table

### Operand 3: `<trans table/aa/BY>`

Address of 256-byte translation table.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, PRE_INDEXED, ABSOLUTE

Provides the absolute address of a 256-byte translation table. Each source byte value is used as an index to retrieve a translated value, which is then tested against the mask.

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the source operand is addressed outside its valid bounds. I1 is left unmodified.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Always cleared to 0 |
| Z | Zero | Set to 1 if source empty without finding match, cleared to 0 if match found |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Find character with property

```assembly
% Skip through argument until byte with ACTIVE bit set
% Translation table FNTAB maps characters to property bits
ACTIVE: 0x01            % Property bit to search for

FUNCTION: % String descriptor

        W1 CLR
        BY SSCAN IND(B.FUNCTION), ACTIVE, ALT(FNTAB)
        IF=Z GO NOT_FOUND
        % Found character with ACTIVE bit set
        % I1 points to the character
```

### Example 2: Find first non-whitespace

```assembly
% Translation table: whitespace -> 0, others -> 1
ISWHITE: % 256-byte table: ' ', '\t', '\n', '\r' -> 0, all else -> 1

LINE: (80 bytes)

        W1 CLR
        BY SSCAN LINE, 0xFF, ISWHITE
        IF<>Z GO FOUND_CHAR
        % Entire line is whitespace
        GO EMPTY_LINE
FOUND_CHAR:
        % I1 points to first non-whitespace
```

### Example 3: Character class search

```assembly
% Find first letter using character class table
% CHARCLASS: digits -> 0x01, letters -> 0x02, other -> 0x00
LETTER_BIT: 0x02

CHARCLASS: % Translation table

TEXT: "123abc"

        W1 CLR
        BY SSCAN TEXT, LETTER_BIT, CHARCLASS
        IF=Z GO NO_LETTERS
        % I1 points to 'a' (first letter)
```

### Example 4: Punctuation detection

```assembly
% Find first punctuation using property table
% PROPS: punctuation chars -> 0x04, others -> 0x00
PUNCT_MASK: 0x04

PROPS: % 256-byte property table

SENTENCE: "Hello world!"

        W1 CLR
        BY SSCAN SENTENCE, PUNCT_MASK, PROPS
        % I1 points to '!'
```

### Example 5: Record-addressed scan

```assembly
RECORD:
        .NAME:  (40 bytes)
        .FLAGS: 1

CLASSIFY: % Translation table

        W1 CLR
        BY SSCAN R.NAME, 0x80, CLASSIFY
        IF=Z GO NO_MATCH
        % Found character with high bit set in translation
        W MOVE I1, R.FLAGS
```

### Example 6: Multi-property search

```assembly
% Search for character with any of several properties
% PROPS table encodes multiple properties in bit fields
% Bit 0: digit, Bit 1: letter, Bit 2: punct, etc.
DIGIT_OR_PUNCT: 0x05    % Bits 0 and 2

PROPS: % Property encoding table

INPUT: (100 bytes)

        W1 CLR
        BY SSCAN INPUT, DIGIT_OR_PUNCT, PROPS
        % Finds first digit or punctuation
```

### Example 7: Local buffer property scan

```assembly
SCAN_FN: ENTS 50
        % Find first control character
        % CTRL_TABLE: control chars (0x00-0x1F) -> 1, others -> 0
        W1 CLR
        BY SSCAN B.BUFFER, 0xFF, CTRL_TABLE
        IF=Z GO NO_CONTROL
        % Found control character at I1
        W MOVE I1, B.POSITION
        RET
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0 Z=1, I1 unmodified, DR trap
  - Byte AND mask > 0: K=0 Z=0, I1 points to found element
  - Source empty: K=0 Z=1, I1 points to next element

- **Translation First**: Byte is translated BEFORE mask test
- **Zero Detection**: Scan continues while `(translation AND mask) == 0`
- **Non-Zero Detection**: Scan stops when `(translation AND mask) != 0`
- **Table Size**: Must be exactly 256 bytes
- **Performance**: O(n) where n is bytes scanned
- **Typical Use**: Character class search, property detection, delimiter finding

---

## Reference Manual

**Section:** §14.16
**Title:** String Scan

---

## See Also

- [SSKIP](sskip.md) - Skip matching elements (no translation)
- [SSPAN](sspan.md) - Span while masked condition (with translation)
- [SMVTR](smvtr.md) - String move translated
