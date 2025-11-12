# PSHIFTR - Packed Shift Rounded

## Overview

**Mnemonic:** `pshiftr`
**Function:** Shift packed BCD with rounding
**Class:** SHIFT
**Privilege:** user

**Format:** `PSHIFTR <source/BCD>, <dest/BCD>`

---

## Description

Shifts the source BCD value to match the destination's scaling factor, rounding before storage. If source and destination have the same scale, performs a simple move. The value itself is unchanged except for rounding; only the decimal position changes. Destination is extended with leading zeros if necessary. Sign control via destination descriptor bit 26.

**Operation:**
```
<source> → <dest> (rescaled and rounded)
```

**Key Characteristics:**
- Rescales BCD value to destination precision
- Rounds before storing
- If same scale: simple move operation
- Destination padded with leading zeros if needed
- Sign control via descriptor bit 26
- Value unchanged except for rounding

**Common Use Cases:**
- Currency precision conversions
- Display formatting (high precision → low precision)
- Internal/external representation conversions
- Rounding intermediate calculations
- Database storage with specific precision

**Operands:** 2 (BCD source, BCD destination)
**Variants:** 1 opcode

---

## Examples

### Example 1: High precision to display precision

```assembly
        % Convert 4-decimal calculation to 2-decimal display
        PSHIFTR PRECISE_VAL, ROUNDED_RESULT
```

**Explanation:** Round 4-decimal internal value to 2-decimal display.

### Example 2: Currency formatting

```assembly
        % Round calculated amount to cents
        PSHIFTR CALC_AMT, DISPLAY_AMT
```

**Explanation:** Convert calculation result to display currency.

### Example 3: Database storage

```assembly
        % Store with database precision
        PSHIFTR HIGH_PREC, LOW_PREC
```

**Explanation:** Round to database field precision requirement.

### Example 4: Intermediate result rounding

```assembly
        % Round intermediate value
        PSHIFTR WORKING_AMT, STORED_AMT
```

**Explanation:** Round calculation step to avoid precision accumulation.

### Example 5: External interface

```assembly
        % Convert internal to external format
        PSHIFTR INTERNAL, EXTERNAL
```

**Explanation:** Round for external system precision requirements.

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **BCD overflow (BO)**: Result exceeds destination size
- **Invalid operation (IVO)**: Invalid BCD digit encoding

---

## Data Status Bits

- **Z (Zero)**: Value equals zero after rounding
- **S (Sign)**: Value sign bit
- **BO (BCD Overflow)**: Destination field too small
- **K (Error)**: BO or IVO occurred

---

## Reference Manual

**Section:** §17.6
**Title:** Packed shift

---

## See Also

- [PSHIFT](pshift.md) - Packed shift without rounding
- [PPACK](ppack.md) - Convert ASCII to packed
- [PUPACK](pupack.md) - Convert packed to ASCII
