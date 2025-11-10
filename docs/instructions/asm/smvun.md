# SMVUN - String Move Until

## Overview

**Mnemonic:** `smvun`
**Function:** Copy bytes until masked test condition
**Class:** STRING
**Privilege:** user

**Format:** `BY SMVUN <source/r/BY/I1>, <dest/w/BY/I2>, <mask/r/BY>, <test/r/BY>`

---

## Description

Copies bytes from source to destination until a byte satisfying a masked test condition is encountered, or until the source is exhausted or destination is full. The condition is evaluated as: `(source_byte AND mask) == test`.

SMVUN provides flexible conditional copying with bitwise masking, enabling character class detection, range checking, and selective termination. The mask and test operands allow detection of specific characters, character ranges, or bit patterns without requiring translation tables.

The operation proceeds byte by byte:
1. Read byte from source at position I1
2. Compute: `byte AND <mask>`
3. Compare result against `<test>` value
4. If equal: stop, set Z=1, leave I1/I2 pointing to the matching byte (not copied)
5. If not equal: copy byte to destination, increment I1 and I2, continue

The terminating byte (the one that satisfies the condition) is NOT copied to the destination. Both index pointers are left pointing to this byte when the condition is met.

This instruction does NOT handle overlap - source and destination must be separate memory regions.

**Operands:** 4
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD73 | BY SMVUN |

---

## Operands

### Operand 1: `<source/r/BY/I1>`

Source string to copy from.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the source string. The I1 register indexes into this string and is incremented for each byte copied. When the terminating condition is met, I1 points to the triggering byte.

### Operand 2: `<dest/w/BY/I2>`

Destination buffer to copy into.

**Role:** Destination
**Access:** Write
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the destination buffer. The I2 register indexes into this buffer and is incremented for each byte written. When termination occurs, I2 points to where the terminating byte would have been written.

### Operand 3: `<mask/r/BY>`

Bit mask applied to source bytes.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the bit mask ANDed with each source byte before comparison. Common values:
- `0xFF` (255): Test full byte value
- `0x7F` (127): Ignore high bit (ASCII 7-bit)
- `0x80` (128): Test only high bit
- `0xE0` (224): Test character class bits

### Operand 4: `<test/r/BY>`

Test value compared against masked result.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the value compared against the masked source byte. When `(source AND mask) == test`, copying terminates.

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the source operand is addressed outside its valid bounds, or when the destination operand is addressed outside its valid bounds.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Set to 1 if destination full, cleared to 0 otherwise |
| Z | Zero | Set to 1 if terminating byte found, cleared to 0 otherwise |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Copy until apostrophe (single quote)

```assembly
% Copy from ARG to LINE, stop at apostrophe (ASCII 0x27 = 39 = 047 octal)
ARG:    "hello'world"
LINE:   (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVUN ARG, LINE, 0xFF, 0x27    % Full byte match for apostrophe
        % LINE = "hello", I1/I2 point to apostrophe
```

### Example 2: Copy until any control character

```assembly
% Copy until control character (0x00-0x1F)
% Mask 0xE0 (11100000), test 0x00 (00000000)
% Any char 0x00-0x1F has upper 3 bits = 000

INPUT:  "Normal text\x03control"
OUTPUT: (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVUN INPUT, OUTPUT, 0xE0, 0x00
        % OUTPUT = "Normal text", stopped at 0x03
```

### Example 3: Copy across domains with termination

```assembly
% Copy from alternative domain to current domain
% Stop at apostrophe (ASCII 47 octal = 0x27 = 39 decimal)
ARG_PTR: % Pointer to arg in alt domain

        W1 CLR
        W2 CLR
        BY SMVUN ALT(IND(B.ARG_PTR)), LINE, 0xFF, 0x27
        % Copied until apostrophe found
```

### Example 4: Extract field until delimiter

```assembly
% Extract name field from CSV, stop at comma
CSV_LINE: "Smith,John,Engineer,50000"
NAME:     (40 bytes)

        W1 CLR
        W2 CLR
        BY SMVUN CSV_LINE, NAME, 0xFF, ','
        % NAME = "Smith", I1 points to comma
```

### Example 5: Copy alphanumeric characters only

```assembly
% Copy while in range '0'-'9' or 'A'-'Z' (not exact, simplified)
% This example shows concept - actual implementation more complex
ALPHAONLY: (80 bytes)

        W1 CLR
        W2 CLR
        % Mask to check if not alphanumeric
        BY SMVUN B.INPUT, ALPHAONLY, 0xFF, ' '
        % Stops at first space
```

### Example 6: Parse record field with terminator

```assembly
RECORD:
        .FIELD1: (20 bytes)
        .FIELD2: (20 bytes)

SOURCE: "Data1|Data2|Data3"

        W1 CLR
        W2 CLR
        BY SMVUN SOURCE, R.FIELD1, 0xFF, '|'
        IF=Z GO FOUND_DELIM
        % Handle missing delimiter
FOUND_DELIM:
        W INCR I1               % Skip delimiter
        W2 CLR
        BY SMVUN SOURCE, R.FIELD2, 0xFF, '|'
```

### Example 7: Local buffer field extraction

```assembly
PARSE: ENTS 100
        % Extract token until whitespace
        W1 CLR
        W2 CLR
        BY SMVUN B.INPUT, B.TOKEN, 0xFF, ' '
        IF<>Z GO NO_SPACE
        % Space found - token extracted
        W MOVE I2, B.TOKEN_LEN
        RET
NO_SPACE:
        % No delimiter - entire input is token
        RET
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0 Z=0, I1 and I2 unmodified, DR trap
  - Outside dest: K=1 Z=0, I1 and I2 unmodified, DR trap
  - Byte found: K=0 Z=1, I1 and I2 point to found byte in source
  - Source empty: K=0 Z=0, I1 and I2 point to next element
  - Dest full: K=1 Z=0, I1 and I2 point to next element

- **Terminating Byte**: NOT copied to destination
- **Overlap**: Not handled - use separate source and destination
- **Mask Patterns**:
  - `0xFF`: Exact byte match
  - `0x7F`: 7-bit ASCII (ignore high bit)
  - `0x80`: Test only high bit
  - `0xE0`: Test upper 3 bits (character class)
- **Performance**: O(n) where n is bytes copied
- **Typical Use**: Field extraction, delimiter parsing, conditional copy

---

## Reference Manual

**Section:** §14.4
**Title:** String move until

---

## See Also

- [SMVWH](smvwh.md) - String move while condition
- [SMVTR](smvtr.md) - String move translated
- [SMVTU](smvtu.md) - String move translated until
- [SSKIP](sskip.md) - Skip matching elements
