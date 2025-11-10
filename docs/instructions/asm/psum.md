# PSUM - Sum of Products

## Overview
**Mnemonic:** `psum` | **Function:** Multiply and accumulate | **Class:** ARITHMETIC | **Privilege:** user | **Format:** `tn PSUM <x>, <y>`

## Description
Multiplies operands and adds product to register (multiply-accumulate). **Operation:** `x * y + Rn → Rn`. Supports BY, H, W, F, D types with 20 variants.

## Variants
| Variant | Opcode | Type | Assembly |
|---------|--------|------|----------|
| 1-20 | 0xFCF8-0xFD0B | BY/H/W/F/D | tn PSUM |

## Examples
```assembly
F4 PSUM B.UNITCOST, B.UNITS   % Accumulate cost
W1 PSUM VAL1, VAL2             % Integer accumulate
D1 PSUM PRICE, QTY             % Double precision
BY1 PSUM FACTOR1, FACTOR2      % Byte multiply-add
H2 PSUM X, Y                   % Halfword accumulate
F3 PSUM COEFF, VALUE           % Float coefficient
W4 PSUM ITEMS(W1), PRICES(W1)  % Array processing
```

## Trap Conditions
- Integer overflow (O), Floating overflow (FO), Floating underflow (FU)

## Data Status Bits
- **Z,S,C,O,FU,FO**: Set based on result

## Reference Manual
**Section:** §11.20

## See Also
- [MUL](mul.md), [ADD](add.md)
