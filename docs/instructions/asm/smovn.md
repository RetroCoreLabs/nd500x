# SMOVN - String Move N Elements

## Overview
**Mnemonic:** `smovn` | **Function:** Move N string elements with bounds checking | **Class:** STRING | **Privilege:** user | **Format:** `t SMOVN <source>, <dest>, <count>`

## Description
Moves specified number of elements from source to dest with descriptor bounds checking. Similar to SMOVE but with explicit count rather than descriptor-based termination.

## Variants
| Variant | Opcode | Type | Assembly |
|---------|--------|------|----------|
| 1-6 | 0xFD78-0xFD7D | BI/BY/H/W/F/D | t SMOVN |

## Examples
```assembly
BY SMOVN SOURCE, DEST, 100     % Move 100 bytes
W SMOVN ARRAY1, ARRAY2, COUNT  % Move COUNT words
H SMOVN DATA, BUFFER, N        % Move N halfwords
BY SMOVN STR, COPY, LEN        % Copy LEN bytes
F SMOVN FLOATS, TARGET, 10     % Move 10 floats
W SMOVN SRC(W1), DST(W1), CNT  % Indexed move
D SMOVN DOUBLES, DEST, NUM     % Move NUM doubles
```

## Trap Conditions
- **Addressing traps**, **Descriptor range (DR)**

## Data Status Bits
- **K**: Termination condition | **Z,S,C,V**: Based on count

## Reference Manual
**Section:** §14.x

## See Also
- [SMOVE](smove.md), [BMOVE](bmove.md), [SFILL](sfill.md)
