# PWCONV - Convert Packed to Binary Word

## Overview

**Mnemonic:** `pwconv`
**Function:** Convert packed BCD to binary integer
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `Wn PWCONV <source/BCD>`

---

## Description

Converts packed decimal (BCD) format to binary integer format and loads into the specified word register. The fractional part of the source is truncated without rounding. On integer overflow, the result is the least significant 32 bits of the binary result.

**Operation:**
```
<source> → Rn (truncated, no rounding)
```

**Key Characteristics:**
- Converts packed BCD to binary word
- Result loaded into specified register (W1-W4)
- Fractional part truncated (not rounded)
- Overflow produces least significant 32 bits
- Used for BCD to integer conversion
- Efficient for computational use of BCD data

**Common Use Cases:**
- User input validation and conversion
- Form data processing
- Database field conversion
- Financial calculations requiring integer arithmetic
- Converting decimal strings to integers

**Operands:** 1 (BCD source, dest is Wn register)
**Variants:** 4 opcodes (one per register W1-W4)

---

## Examples

### Example 1: Convert input field

```assembly
        % Convert user input to integer
        W1 PWCONV IFIELD
        % W1 now contains binary integer
```

**Explanation:** Convert BCD input field to binary for computation.

### Example 2: Form processing

```assembly
        % Process form fields
        W1 PWCONV FORM_QUANTITY
        W2 PWCONV FORM_PRICE
        W3 MOVE W1
        W3 MUL W2              % Calculate total
```

**Explanation:** Convert BCD form data for arithmetic operations.

### Example 3: Database conversion

```assembly
        % Load BCD database field as integer
        W3 PWCONV DB_AMOUNT
        W3 ADD ADJUSTMENT
        % Process adjusted value
```

**Explanation:** Convert BCD database value for calculations.

### Example 4: Array processing

```assembly
        % Convert array of BCD values
        W2 CLR
LOOP:   W1 PWCONV RECORDS(W2)
        % Process W1 value
        W2 ADD 1
        W2 COMP COUNT
        IF<GO LOOP
```

**Explanation:** Batch convert BCD array elements.

### Example 5: Validation with range check

```assembly
        % Convert and validate user input
        W1 PWCONV USER_ENTRY
        W1 COMP MIN_VALUE
        IF<GO TOO_SMALL
        W1 COMP MAX_VALUE
        IF>GO TOO_LARGE
        % Value is valid
```

**Explanation:** Convert BCD input and validate range.

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **Integer overflow (O)**: Result exceeds 32-bit signed range
- **Invalid operation (IVO)**: Invalid BCD digit encoding

---

## Data Status Bits

- **Z (Zero)**: Result equals zero
- **S (Sign)**: Result sign bit
- **O (Overflow)**: Integer overflow occurred
- **K (Error)**: IVO or O occurred

---

## Reference Manual

**Section:** §17.9
**Title:** Convert packed to binary word

---

## See Also

- [WPCONV](wpconv.md) - Convert word to packed BCD
- [PPACK](ppack.md) - Convert binary to packed BCD
- [PUPACK](pupack.md) - Convert packed BCD to ASCII
