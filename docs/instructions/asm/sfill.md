# SFILL - String Fill

## Overview
**Mnemonic:** `sfill` | **Function:** Fill string with pattern | **Class:** STRING | **Privilege:** user | **Format:** `t SFILL <source>, <dest>`

## Description
Fills destination string with repeated source pattern using descriptor bounds checking. Copies source element to dest repeatedly until dest is full. Index registers I1 (source) and I2 (dest) track positions.

## Variants
| Variant | Opcode | Type | Assembly |
|---------|--------|------|----------|
| 1-6 | 0xFD6C-0xFD71 | BI/BY/H/W/F/D | t SFILL |

## Examples
```assembly
BY SFILL PATTERN, BUFFER       % Fill with byte pattern
W SFILL 0, ARRAY               % Zero fill array
H SFILL TEMPLATE, DEST         % Halfword fill
BY SFILL SPACE, STRING         % Fill with spaces
W SFILL INIT_VAL, TABLE        % Initialize table
F SFILL 0.0, FLOAT_ARRAY       % Zero float array
D SFILL DEFAULT, DATA          % Fill with default
```

## Trap Conditions
- **Addressing traps**, **Descriptor range (DR)**

## Data Status Bits
- **K**: 0=source exhausted, 1=dest full

## Reference Manual
**Section:** §14.3

## See Also
- [SMOVE](smove.md), [SMOVN](smovn.md), [SCOMP](scomp.md)
