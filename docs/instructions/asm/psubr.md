# PSUBR - Packed Subtract Rounded

## Overview
**Mnemonic:** `psubr` | **Function:** Subtract packed BCD with rounding | **Class:** ARITHMETIC | **Privilege:** user | **Format:** `PSUBR <a>, <b>, <c>`

## Description
Subtracts packed BCD operands with rounding. Identical to PSUB but rounds result before storing. **Operation:** `a - b → c` (rounded)

## Variants
| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFE86 | PSUBR |

## Examples
```assembly
PSUBR TOTAL, B.DISCOUNT, TOTAL     % Discount with rounding
PSUBR BALANCE, WITHDRAWAL, BALANCE  % Balance update
PSUBR GROSS, TAX, NET               % Tax calculation
PSUBR ORIG_PRICE, MARKDOWN, SALE    % Price reduction
PSUBR QTY_ON_HAND, QTY_SOLD, QTY_REM % Inventory
PSUBR AMOUNT_DUE, PAYMENT, BALANCE   % Payment
PSUBR BUDGET, EXPENSES, REMAINING    % Expense tracking
```

## Reference Manual
**Section:** §17.3

## See Also
- [PSUB](psub.md) - Without rounding
