# SSKIP - String Skip

## Overview

**Mnemonic:** `sskip`
**Function:** Skip identical elements, compare first differing element
**Class:** STRING
**Privilege:** user

**Format:** `BY SSKIP <source/r/BY/I1=>, <test/r/BY>`

---

## Description

Advances through a string, skipping all bytes equal to a test value, until a different byte is encountered or the end of the string is reached. When a differing byte is found, it is compared against the test value with results reflected in the Z and S flags.

**Operation:**
```
while (source[I1] == test_value):
    I1 = I1 + 1
if (I1 at end of string):
    Z = 1, S = 0
else:
    Z = 0
    S = (source[I1] < test_value) ? 1 : 0
```

**Key Characteristics:**
- Skip-while-equal with unsigned byte comparison
- Uses I1 as implicit index register (auto-incremented)
- Z=1 if all bytes matched (string exhausted)
- Z=0 if differing byte found, S indicates comparison result
- K flag always cleared to 0
- Unsigned comparison (bytes treated as 0-255)
- DR trap on descriptor range violation
- Essential for whitespace trimming and delimiter skipping
- O(n) complexity where n = number of matching bytes

SSKIP combines skip-while-equal functionality with unsigned comparison, making it ideal for skipping runs of identical characters (like leading spaces) while simultaneously determining whether the first differing character is greater than or less than the test value.

The operation proceeds as follows:
1. Read byte from source at position I1
2. Compare with `<test>` value (unsigned comparison)
3. If equal: increment I1, continue
4. If not equal: stop, set Z=0, set S based on comparison:
   - If byte > test: S=0 (cleared)
   - If byte < test: S=1 (set)
5. If string exhausted: Z=1, S=0

Bytes are treated as unsigned 8-bit values (0-255) for all comparisons.

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDAE | BY SSKIP |

---

## Operands

### Operand 1: `<source/r/BY/I1=>`

Source string to scan.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the string to scan. The I1 register serves as the index pointer and is incremented while skipping equal bytes. When a differing byte is found, I1 points to it.

### Operand 2: `<test/r/BY>`

Test value to skip and compare against.

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the byte value to skip. The operation skips all bytes equal to this value, then compares the first differing byte against it (unsigned).

---

## Trap Conditions

- **Descriptor Range (DR)**: Occurs when the source operand is addressed outside its valid bounds. I1 is left unmodified.

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| K | Carry | Always cleared to 0 |
| Z | Zero | Set to 1 if source empty (all bytes matched), cleared to 0 if differing byte found |
| S | Sign | Set to 1 if differing byte < test, cleared to 0 if differing byte > test or source empty |

---

## Examples

### Example 1: Skip leading spaces

```assembly
% Skip ASCII spaces (0x20 = 32) from string
LINE: "    Hello world"

        W1 CLR
        BY SSKIP R.LINE, 32
        % I1 now points to 'H'
        % Z=0, S=0 (since 'H'=0x48 > 0x20)
```

### Example 2: Skip and classify next character

```assembly
% Skip zeros, check if next byte is greater or less
DATA: "\x00\x00\x00\x50"

        W1 CLR
        BY SSKIP DATA, 0
        IF=Z GO ALL_ZEROS       % Entire string was zeros
        IF=S GO BELOW_ZERO      % Next byte < 0 (impossible for unsigned)
        % Next byte > 0
        % I1 points to 0x50
```

### Example 3: Record field skip and compare

```assembly
RECORD:
        .TEXT: (80 bytes)
        .TYPE: 1

        W1 CLR
        BY SSKIP R.TEXT, ' '    % Skip spaces
        IF=Z GO EMPTY           % All spaces
        % I1 points to first non-space
        BY MOVE R.TEXT(I1), B.FIRST_CHAR
```

### Example 4: Skip delimiter and test range

```assembly
% Skip null bytes, check if next is in upper ASCII
BUFFER: "\x00\x00\x00\xFF"

        W1 CLR
        BY SSKIP BUFFER, 0
        IF=Z GO ONLY_NULLS
        % Found non-null byte at I1
        BY MOVE BUFFER(I1), I2
        W COMP I2, 0x80
        IF>=GO HIGH_ASCII
```

### Example 5: Local variable whitespace skip

```assembly
PARSE: ENTS 80
        % Skip leading spaces in local buffer
        W1 CLR
        BY SSKIP B.INPUT, ' '
        IF=Z GO BLANK_LINE
        % I1 points to first non-space
        % Copy remaining to output
        W MOVE I1, B.OFFSET
        RET
```

### Example 6: Skip and get lexical order

```assembly
% Skip character, compare next for sorting
STRING1: "aaaaabcd"

        W1 CLR
        BY SSKIP STRING1, 'a'   % Skip all 'a's
        % Z=0, I1 points to 'b'
        % S=0 because 'b' (0x62) > 'a' (0x61)
```

### Example 7: Multi-skip with position tracking

```assembly
% Skip padding, track position
PAD_CHAR: 0xFF
DATA:     (100 bytes)

        W1 CLR
        BY SSKIP DATA, PAD_CHAR
        IF=Z GO ALL_PADDING
        % Save position of first data byte
        W MOVE I1, B.DATA_START
        % Check if data byte is greater than padding
        IF<>S GO DATA_GT_PAD
        % Data byte < padding value
```

---

## Performance Notes

- **Termination Conditions**:
  - Outside source: K=0 Z=1 S=0, I1 unmodified, DR trap
  - Byte > test: K=0 Z=0 S=0, I1 points to differing element
  - Byte < test: K=0 Z=0 S=1, I1 points to differing element
  - Source empty: K=0 Z=1 S=0, I1 points to next element

- **Unsigned Comparison**: All bytes treated as unsigned (0-255)
- **Common Use**: Skip leading whitespace, skip padding, skip separators
- **Skip Count**: To count skipped bytes: `skip_count = I1 - original_I1`
- **Performance**: O(n) where n is number of matching bytes skipped
- **Typical Use**: Whitespace trimming, padding removal, delimiter skipping with classification

---

## Reference Manual

**Section:** §14.14
**Title:** String skip elements

---

## See Also

- [SSPAN](sspan.md) - String span (skip while condition with translation)
- [SSCAN](sscan.md) - String scan (find with translation)
- [SMATCH](smatch.md) - String match (substring search)
