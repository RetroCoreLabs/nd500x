# SMVWH - String Move While

## Overview

**Mnemonic:** `smvwh`
**Function:** Copy bytes while masked test condition holds
**Class:** STRING
**Privilege:** user

**Format:** `BY SMVWH <source/r/BY/I1=>, <dest/w/BY/I2=>, <mask/r/BY>, <test/r/BY>`

---

## Description

Copies bytes from source to destination while each byte satisfies a masked test condition: `(source_byte AND mask) == test`. Copying continues as long as the condition holds true, and terminates when a byte fails the condition, or when source is exhausted or destination is full.

**Operation:**
```
while (source not empty and dest not full):
    if ((source[I1] AND mask) != test):
        K=0, Z=0, break  // Differing byte
    dest[I2] = source[I1]
    I1++, I2++
K=1, Z=1 if source empty or dest full
```

**Key Characteristics:**
- String copy while masked condition holds
- 4 operands: source, dest, mask, test value
- Uses I1 (source index) and I2 (dest index) implicitly
- K=0, Z=0 if differing byte found (condition fails)
- K=1, Z=1 if source empty or dest full
- Differing byte NOT copied to destination
- Does NOT handle overlap (requires separate buffers)
- Complement of SMVUN (while vs until)
- Essential for character class extraction and range validation
- O(n) complexity where n = bytes copied

SMVWH is the complement of SMVUN - while SMVUN copies UNTIL a condition is met, SMVWH copies WHILE a condition holds. This makes it ideal for extracting characters within a specific range, copying character classes (digits, letters, etc.), or processing runs of similar characters.

The operation proceeds byte by byte:
1. Read byte from source at position I1
2. Compute: `byte AND <mask>`
3. Compare result against `<test>` value
4. If equal: copy byte to destination, increment I1 and I2, continue
5. If not equal: stop, set Z=0, leave I1/I2 pointing to the differing byte

The first byte that fails the condition (differing byte) is NOT copied to the destination. Both index pointers are left pointing to this byte when the condition fails.

This instruction does NOT handle overlap - source and destination must be separate memory regions.

**Operands:** 4
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD72 | BY SMVWH |

---

## Operands

### Operand 1: `<source/r/BY/I1=>`

Source string to copy from.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the source string. The I1 register indexes into this string and is incremented for each byte copied. When the condition fails, I1 points to the differing byte.

### Operand 2: `<dest/w/BY/I2=>`

Destination buffer to copy into.

**Role:** Destination
**Access:** Write
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the destination buffer. The I2 register indexes into this buffer and is incremented for each byte written. When condition fails, I2 points to where the differing byte would have been written.

### Operand 3: `<mask/r/BY>`

Bit mask applied to source bytes.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the bit mask ANDed with each source byte before comparison. Common mask values:
- `0xFF`: Test full byte value
- `0xF0`: Test upper nibble
- `0x0F`: Test lower nibble
- `0x80`: Test sign bit only
- `0x7F`: Ignore sign bit

### Operand 4: `<test/r/BY>`

Test value compared against masked result.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the value compared against the masked source byte. Copying continues while `(source AND mask) == test` remains true.

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the source operand is addressed outside its valid bounds, or when the destination operand is addressed outside its valid bounds.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Set to 1 if destination full or source empty, cleared to 0 if different byte found |
| Z | Zero | Set to 1 if destination full or source empty, cleared to 0 if different byte found |
| S | Sign | Undefined after operation |

---

## Examples

### Example 1: Copy characters in specific range

```assembly
% Copy characters in range 0x40-0x5F (ASCII '@' to '_')
% Mask 0xE0 (11100000b), Test 0x40 (01000000b)
% Characters 0x40-0x5F all have upper 3 bits = 010

INPUT:  "ABC@#$DEF"
OUTPUT: (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVWH INPUT, OUTPUT, 0xE0, 0x40
        % OUTPUT = "ABC", stopped at '@' (0x40 matches, but next char '#' doesn't)
```

### Example 2: Copy while in character class

```assembly
% Copy characters as long as in range 0x40-0x7F
% Using mask 0x80 to check high bit = 0
% Works for standard ASCII (high bit clear)

        W1 CLR
        W2 CLR
        BY SMVWH INPUT, BUFFER, 0x80, 0x00
        % Copies while high bit is 0
```

### Example 3: Extract digit sequence

```assembly
% Copy while digits (rough approximation with masking)
% Actual digit check: '0'=0x30, '9'=0x39
% Mask 0xF0, test 0x30 catches 0x30-0x3F range

NUMBER: "12345abc"
DIGITS: (20 bytes)

        W1 CLR
        W2 CLR
        BY SMVWH NUMBER, DIGITS, 0xF0, 0x30
        % DIGITS = "12345", stopped at 'a'
```

### Example 4: Copy while in specified range (example from manual)

```assembly
% Copy from INPUT to BUFFER while characters in range 0x40-0x7F
% Mask 0xC0 (11000000), test 0x40 (01000000)
% Range 0x40-0x7F has bits 7-6 = 01

INPUT:  "Valid text\x80invalid"
BUFFER: (80 bytes)

        W1 CLR
        W2 CLR
        BY SMVWH INPUT, BUFFER, 0xC0, 0x40
        % Copies while in 0x40-0x7F range
```

### Example 5: Record field conditional copy

```assembly
RECORD:
        .STATUS: (1 byte)
        .DATA:   (80 bytes)

SOURCE: (100 bytes)

        % Copy while specific bit pattern
        W1 CLR
        W2 CLR
        BY SMVWH SOURCE, R.DATA, 0x0F, 0x05
        % Copies while lower 4 bits = 0101
```

### Example 6: Local buffer character class extraction

```assembly
EXTRACT: ENTS 100
        % Extract alphanumeric prefix
        W1 CLR
        W2 CLR
        BY SMVWH B.INPUT, B.OUTPUT, 0xC0, 0x40
        IF=K GO COMPLETED       % Source empty or dest full
        % Different byte found
        W MOVE I1, B.STOP_POS
        RET
```

### Example 7: Copy with multiple conditions

```assembly
% Copy uppercase letters (A-Z = 0x41-0x5A)
% Approximate with mask 0xE0, test 0x40

TEXT:   "ABCdef123"
UPPER:  (40 bytes)

LOOP:   W1 CLR
        W2 CLR
        BY SMVWH TEXT, UPPER, 0xE0, 0x40
        % UPPER = "ABC"
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0 Z=0, I1 and I2 unmodified, DR trap
  - Outside dest: K=1 Z=0, I1 and I2 unmodified, DR trap
  - Different bytes: K=0 Z=0, I1 and I2 point to differing byte
  - Source empty: K=1 Z=1, I1 and I2 point to next element
  - Dest full: K=1 Z=1, I1 and I2 point to next element

- **Differing Byte**: NOT copied to destination
- **Overlap**: Not handled - use separate source and destination
- **Mask Design**: Choose mask to isolate relevant bits for character class testing
- **Complement**: SMVUN copies UNTIL condition, SMVWH copies WHILE condition
- **Performance**: O(n) where n is bytes copied
- **Typical Use**: Character class extraction, range validation, run-length detection

---

## Reference Manual

**Section:** §14.3
**Title:** String move while

---

## See Also

- [SMVUN](smvun.md) - String move until (complementary operation)
- [SMVTR](smvtr.md) - String move translated
- [SMVTU](smvtu.md) - String move translated until
- [SSPAN](sspan.md) - String span (scan while)
