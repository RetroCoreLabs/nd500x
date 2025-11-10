# PWCONV - Convert Packed to Binary Word

## Overview
**Mnemonic:** `pwconv` | **Function:** Convert packed BCD to binary integer | **Class:** FLOAT_MATH | **Privilege:** user | **Format:** `Wn PWCONV <source/BCD>`

## Description
Converts packed decimal to binary integer in register. Fractional part truncated (no rounding). **Operation:** `source → Rn`

## Variants
| Variant | Opcode | Register | Assembly |
|---------|--------|----------|----------|
| 1-4 | 0xFEBC-0xFEBF | W1-W4 | Wn PWCONV |

## Examples
```assembly
W1 PWCONV IFIELD               % Convert input field
W2 PWCONV BCD_VALUE            % BCD to binary
W3 PWCONV USER_ENTRY           % User input conversion
W4 PWCONV FORM_FIELD           % Form data conversion
W1 PWCONV RECORDS(W2)          % Array conversion
W2 PWCONV CALC_RESULT          % Result to integer
W3 PWCONV DATABASE_FIELD       % Database value
```

## Trap Conditions
- **Integer overflow (O)**, **Invalid operation (IVO)**

## Data Status Bits
- **Z,S,O,K**: Set based on result

## Reference Manual
**Section:** §17.9

## See Also
- [PPACK](ppack.md), [PUPACK](pupack.md)
