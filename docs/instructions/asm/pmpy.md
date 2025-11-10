# PMPY - Packed Multiply

## Overview

**Mnemonic:** `pmpy`
**Function:** Multiply packed BCD numbers
**Class:** ARITHMETIC
**Privilege:** user
**Format:** `PMPY <a>, <b>, <c>`

---

## Description

Multiplies two packed BCD (Binary Coded Decimal) numbers and stores the result in a third operand. The result is automatically scaled according to the scale factor in the destination operand's descriptor before storing. This instruction is designed for decimal arithmetic in financial and business applications where exact decimal representation is required.

Special handling: When an operand with invalid digits is multiplied by zero, the result is zero (not an Invalid Operation trap). The K flag is set on BCD overflow or invalid operation conditions.

**Operation:**
```
a * b → c (with scaling)
```

**Common Use Cases:**
- Financial calculations (price * quantity)
- Currency arithmetic
- Accounting computations
- Percentage calculations

**Operands:** 3
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFEB4 | PMPY |

---

## Operands

**Operand 1** (Multiplicand, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Packed BCD
- **Role**: First factor

**Operand 2** (Multiplier, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Packed BCD
- **Role**: Second factor

**Operand 3** (Product, Write):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Packed BCD
- **Role**: Result with automatic scaling

**Result**: Product stored with scaling applied

---

## Trap Conditions
- **Addressing traps**: Invalid address or protection violation
- **BCD overflow (BO)**: Result too large for destination
- **Invalid operation (IVO)**: Invalid BCD digits (except 0 * invalid = 0)

---

## Data Status Bits
- **Z**: Set if product = 0
- **C**: Set if product sign bit is 1
- **BO**: Set if BCD overflow
- **K**: Set if BO or IVO occurred

---

## Examples

### Example 1: Simple BCD multiply
```assembly
PMPY QUANTITY, PRICE, TOTAL
```

### Example 2: Financial calculation with local variables
```assembly
PMPY B.PRICE, DISCOUNT, B.NET
```

### Example 3: Tax calculation
```assembly
% AMOUNT * TAX_RATE = TAX
PMPY AMOUNT, TAX_RATE, TAX
```

### Example 4: Percentage calculation
```assembly
% Calculate 15% of BASE
PMPY BASE, 0.15, RESULT
```

### Example 5: Extended precision multiply
```assembly
CALC_TOTAL:
        PMPY UNITS, UNIT_PRICE, SUBTOTAL
        PMPY SUBTOTAL, TAX_RATE, TAX
        RET
```

### Example 6: Currency conversion
```assembly
CONVERT:
        % AMOUNT * EXCHANGE_RATE = CONVERTED
        PMPY SOURCE_AMT, EXCH_RATE, DEST_AMT
        IFKGO OVERFLOW_ERROR
        RET
```

### Example 7: Batch calculation
```assembly
PROCESS_ITEMS:
        W1 CLR
LOOP:
        PMPY ITEMS(W1).QTY, ITEMS(W1).PRICE, ITEMS(W1).TOTAL
        W1 INC
        W1 COMP ITEM_COUNT
        IF<GO LOOP
        RET
```

---

## Performance Notes
- Execution: 8-15 cycles (BCD arithmetic slower than binary)
- Automatic decimal scaling
- Hardware BCD support where available
- Slower than binary MPY but exact decimal results

---

## Reference Manual
**Section:** §17.4
**Title:** Packed multiply

---

## See Also
- [PMPYR](pmpyr.md) - Packed multiply rounded
- [PADD](padd.md) - Packed add
- [PSUB](psub.md) - Packed subtract
