# SSPAN - String Span

## Overview

**Mnemonic:** `sspan`
**Function:** Scan while translated byte AND mask is non-zero
**Class:** STRING
**Privilege:** user

**Format:** `BY SSPAN <source/r/BY/I1=>, <mask/r/BY>, <trans table/aa/BY>`

---

## Description

Scans through a string, translating each byte via a translation table and testing the result against a bit mask, advancing while `(translated_byte AND mask) != 0`. The scan continues as long as the masked condition holds true, and terminates when a byte fails the condition or the end of the string is reached.

**Operation:**
```
while (source[I1] not at end):
    translated = trans_table[byte]
    if ((translated AND mask) == 0):
        Z=1, I1 = position
        return
    I1++
I1 = end of string
Z = 0
```

**Key Characteristics:**
- String span (skip while) with translation and mask test
- Uses 256-byte translation table
- Uses I1 as implicit index register (auto-incremented)
- Z=1 if terminating byte found (translated & mask = 0)
- Z=0 if source exhausted (all bytes passed condition)
- K flag always cleared to 0
- Complement of SSCAN (span while vs find first where)
- DR trap on descriptor range violation
- Essential for token scanning and property-based skipping
- O(n) complexity where n = bytes spanned

SSPAN is the "span while" complement to SSCAN's "find first where" operation. While SSCAN searches for the first byte where the masked translation is non-zero, SSPAN skips over all such bytes and stops at the first byte where the masked translation equals zero.

The operation proceeds as follows for each byte:
1. Read byte from source at position I1
2. Translate byte through table: `translated = table[byte]`
3. Test: `translated AND <mask>`
4. If result is non-zero: increment I1, continue
5. If result is zero: stop, set Z=1, I1 points to found element

The span terminates successfully (Z=1) when a byte's translated value ANDed with the mask produces a zero result. If the entire string is spanned without finding such a byte, Z=0 is set and I1 points past the end of the string.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB2 | BY SSPAN |

---

## Operands

### Operand 1: `<source/r/BY/I1=>`

Source string to scan.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the string to scan. The I1 register serves as the index pointer and is incremented while spanning. When a terminating byte is found (masked translation equals zero), I1 points to that element.

### Operand 2: `<mask/r/BY>`

Bit mask applied to translated bytes.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the bit mask ANDed with each translated byte. The span continues while the result is non-zero and terminates when zero. Common values:
- `0xFF`: Check for zero translation (terminator)
- `0x80`: Check high bit
- `0x0F`: Check low nibble
- Custom masks: Check specific property bits

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
| Z | Zero | Set to 1 if terminating byte found (tr(byte) AND mask = 0), cleared to 0 if source exhausted |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Skip directive fragment until terminator

```assembly
% Skip through DIRECTIVE until character translates to zero
% CODETABLE: non-terminators -> non-zero, terminators -> 0
CODETABLE: % 256-byte translation table

DIRECTIVE: "COMMAND ARG1 ARG2\x00"

        W1 CLR
        BY SSPAN DIRECTIVE, 0xFF, B.CODETABLE
        IF<>Z GO NO_TERMINATOR
        % I1 points to null terminator
```

### Example 2: Skip alphanumeric characters

```assembly
% Translation table: alphanumeric -> 1, others -> 0
ISALNUM: % 256-byte table

TEXT: "abc123!@#"

        W1 CLR
        BY SSPAN TEXT, 0xFF, ISALNUM
        IF=Z GO FOUND_NON_ALNUM
        % All characters were alphanumeric
FOUND_NON_ALNUM:
        % I1 points to '!'
```

### Example 3: Skip while property bit set

```assembly
% Skip while high bit set in translation
% PROPS: property table with bit-encoded flags
HIGH_BIT: 0x80

PROPS: % Translation table

DATA: (100 bytes)

        W1 CLR
        BY SSPAN DATA, HIGH_BIT, PROPS
        % I1 points to first byte whose translation has bit 7 = 0
```

### Example 4: Character class spanning

```assembly
% Skip identifier characters (letters, digits, underscore)
% CHARTYPE: identifier chars -> 0x01, others -> 0x00
IDENTIFIER_BIT: 0x01

CHARTYPE: % Character classification table

SOURCE: "variable_name123 = value"

        W1 CLR
        BY SSPAN SOURCE, IDENTIFIER_BIT, CHARTYPE
        % I1 points to ' ' (space, not identifier char)
        % Identifier name is SOURCE[0..I1-1]
```

### Example 5: Record-addressed property span

```assembly
RECORD:
        .FIELD: (40 bytes)
        .LEN:   1

CLASSIFY: % Classification table

        W1 CLR
        BY SSPAN R.FIELD, 0x0F, CLASSIFY
        % Spanned while lower nibble non-zero
        W MOVE I1, R.LEN
```

### Example 6: Skip until null-translated character

```assembly
% Skip until character that translates to 0x00
% Useful for format-specific terminators
FORMAT_TABLE: % Format-specific translation
              % Terminators translate to 0, others to non-zero

INPUT: (200 bytes)

        W1 CLR
        BY SSPAN INPUT, 0xFF, FORMAT_TABLE
        IF<>Z GO UNTERMINATED
        % Found terminator at I1
        W MOVE I1, B.LENGTH
```

### Example 7: Local buffer token spanning

```assembly
SPAN_TOKEN: ENTS 80
        % Span over token characters defined by table
        % TOKEN_TABLE: token chars -> 1, delimiters -> 0
        W1 CLR
        BY SSPAN B.INPUT, 0xFF, TOKEN_TABLE
        IF=Z GO HAS_DELIMITER
        % No delimiter - entire buffer is token
        RET
HAS_DELIMITER:
        % I1 points to delimiter
        W MOVE I1, B.TOKEN_LEN
        RET
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0 Z=0, I1 unmodified, DR trap
  - tr(byte) AND mask = 0: K=0 Z=1, I1 points to found element
  - Source empty: K=0 Z=0, I1 points to next element

- **Translation First**: Byte is translated BEFORE mask test
- **Non-Zero Span**: Continues while `(translation AND mask) != 0`
- **Zero Termination**: Stops when `(translation AND mask) == 0`
- **Complement**: SSCAN finds where non-zero, SSPAN spans over non-zero
- **Table Size**: Must be exactly 256 bytes
- **Performance**: O(n) where n is bytes spanned
- **Typical Use**: Token scanning, property-based skipping, format parsing

---

## Reference Manual

**Section:** §14.17
**Title:** String span

---

## See Also

- [SSCAN](sscan.md) - String scan (find where condition met)
- [SSKIP](sskip.md) - Skip equal elements (no translation)
- [SMVWH](smvwh.md) - String move while condition
