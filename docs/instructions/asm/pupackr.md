# PUPACKR - Convert Packed to ASCII Rounded

## Overview
**Mnemonic:** `pupackr` | **Function:** Unpack BCD to ASCII with rounding | **Class:** ARITHMETIC | **Privilege:** user | **Format:** `PUPACKR <source/BCD>, <dest/ASCII>`

## Description
Converts packed BCD to ASCII with rounding. Identical to PUPACK but rounds before conversion.

## Variants
| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE93 | PUPACKR |

## Examples
```assembly
PUPACKR VAR1, IFIELD           % Rounded unpack
PUPACKR PRECISE_VAL, DISPLAY   % Display with rounding
PUPACKR AMOUNT, TEXT_FIELD     % Currency rounded
PUPACKR CALC, OUTPUT           % Calculation result
PUPACKR BALANCE, SCREEN        % Screen display
PUPACKR TOTAL, REPORT          % Report with rounding
PUPACKR VALUES(W1), TEXTS(W1)  % Batch processing
```

## Reference Manual
**Section:** §17.8

## See Also
- [PUPACK](pupack.md)
