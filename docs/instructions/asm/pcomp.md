# PCOMP - Packed Compare

## Overview

**Mnemonic:** `pcomp`
**Function:** Compare packed BCD numbers
**Class:** COMPARE (Packed BCD)
**Privilege:** user

**Format:** `PCOMP <a/r/BCD=>, <b/r/BCD=>`

---

## Description

Compares two packed Binary Coded Decimal (BCD) numbers by subtracting the second operand from the first and setting status flags based on the result. The result of the subtraction is discarded - only the flags are affected.

PCOMP automatically handles operands with different scale factors (decimal point positions) by aligning them before comparison. This allows direct comparison of values like 123.45 and 123.450 as equal, or 99.9 and 100.0 with proper magnitude ordering.

The instruction treats unsigned numbers as positive, and positive zero equals negative zero. This normalization ensures consistent comparison behavior across different BCD representations.

PCOMP is essential for conditional branching in financial and decimal arithmetic code, enabling range checks, bounds validation, and sorting operations on BCD-encoded values.

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFEB3 | PCOMP |

---

## Operands

### Operand 1: `<a/r/BCD=>`

First value (minuend).

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the BCD number to be compared (left side of comparison).

### Operand 2: `<b/r/BCD=>`

Second value (subtrahend).

**Role:** Source
**Access:** Read
**Addressing modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

Specifies the BCD number to compare against (right side of comparison).

---

## Trap Conditions

- **Invalid Operation (IVO)**: Invalid BCD digit encoding detected
- **Addressing traps**: Standard memory access violations

---

## Data Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| Z | Zero | Set if difference equals zero (operands equal) |
| S | Sign | Set if difference is negative (a < b) |
| K | Carry | Set if IVO occurred |

---

## Examples

### Example 1: Compare total with maximum

```assembly
% Check if TOTAL exceeds MAX limit
        PCOMP TOTAL, MAX
        IF>GO OVER_LIMIT        % Branch if TOTAL > MAX
        % Within limit
```

### Example 2: Range validation

```assembly
% Validate value is within acceptable range
MIN_VALUE: 0.00
MAX_VALUE: 999.99

        PCOMP B.INPUT, MIN_VALUE
        IF<GO TOO_SMALL
        PCOMP B.INPUT, MAX_VALUE
        IF>GO TOO_LARGE
        % Value is valid
```

### Example 3: Equality check

```assembly
% Check if two amounts are equal
        PCOMP AMOUNT1, AMOUNT2
        IF=Z GO AMOUNTS_EQUAL
        % Amounts differ
```

### Example 4: Sort comparison

```assembly
% Bubble sort comparison for BCD array
ARRAY:  % Array of BCD values

LOOP:   PCOMP ARRAY(I1), ARRAY(I2)
        IF<=GO NO_SWAP
        % Swap elements if first > second
        % ... swap code ...
NO_SWAP:
```

### Example 5: Threshold testing

```assembly
% Check if balance below minimum
BALANCE:  % Current account balance
MINIMUM:  % Minimum required balance

        PCOMP BALANCE, MINIMUM
        IF<GO LOW_BALANCE_WARNING
        % Balance acceptable
```

---

## Performance Notes

- **Scale Alignment**: Automatically aligns different decimal point positions
- **Zero Normalization**: +0 equals -0
- **Unsigned Treatment**: Unsigned values treated as positive
- **Result Discarded**: Only status flags affected, no destination operand
- **Typical Use**: Conditional branching, validation, sorting
- **Conditional Branches**: Use with IF=, IF<>, IF<, IF<=, IF>, IF>= after PCOMP

---

## Reference Manual

**Section:** §17.5
**Title:** Packed Compare

---

## See Also

- [PADD](padd.md) - Packed add
- [PSUB](psub.md) - Packed subtract
- [COMP](comp.md) - Integer compare
