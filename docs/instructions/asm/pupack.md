# PUPACK - Convert Packed to ASCII

## Overview
**Mnemonic:** `pupack` | **Function:** Unpack BCD to ASCII | **Class:** ARITHMETIC | **Privilege:** user | **Format:** `PUPACK <source/BCD>, <dest/ASCII>`

## Description
Converts packed BCD to ASCII decimal format for display/output. **Operation:** `source → dest` (unpacked)

## Variants
| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFEB6 | PUPACK |

## Examples
```assembly
PUPACK VAR1, IFIELD            % Basic unpack
PUPACK BCD_VAL, ASCII_OUTPUT   % Display conversion
PUPACK AMOUNT, DISPLAY_FIELD   % Currency display
PUPACK CALC_RESULT, TEXT_OUT   % Result to text
PUPACK TOTAL, REPORT_LINE      % Report generation
PUPACK BALANCE, SCREEN_FIELD   % Screen output
PUPACK RECORDS(W1), OUTPUTS(W1) % Batch conversion
```

## Trap Conditions
- **BCD overflow (BO)**, **Invalid operation (IVO)**

## Data Status Bits
- **Z,S,BO,K**: Set based on result

## Reference Manual
**Section:** §17.8

## See Also
- [PUPACKR](pupackr.md), [PPACK](ppack.md)
