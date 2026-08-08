# WPCONV - Convert Binary Word to Packed BCD

## Overview

**Mnemonic:** `wpconv`
**Function:** Convert binary word to packed decimal
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `Wn WPCONV <dest>`

---

## Description

Converts the contents of a specified word register from binary format to packed BCD (Binary-Coded Decimal) format and stores the result in the destination operand. The destination scaling factor controls precision and determines how the value is represented in packed decimal.

**Operation:**
```
Rn (binary) → <dest> (packed BCD)
value = 0 → Z flag
value.signbit → S flag
overflow → BO flag (BCD Overflow)
BO → K flag
```

**Key Characteristics:**
- Converts 32-bit binary integer to packed BCD
- Supports scaled packed decimal with positive or negative scaling factors
- Negative scaling: loses least significant digits (truncation)
- Destination extended with zeros as required by scaling factor
- Can trigger BCD overflow trap if result doesn't fit

**Common Use Cases:**
- Financial calculations requiring decimal precision
- Converting internal binary values for BCD arithmetic
- Preparing data for decimal display or output
- Database field conversions (binary to decimal)
- Interfacing with BCD-based systems
- Accounting and monetary calculations

**Operands:** 1 (destination in packed BCD format)
**Variants:** 4 opcodes (one per register I1-I4)

---

## Variants

Total variants: 4 (W prefix, 4 registers)

| Variant | Opcode | Register | Assembly |
|---------|--------|----------|----------|
| 1/4 | 0xFEB8 | I1 | W1 WPCONV |
| 2/4 | 0xFEB9 | I2 | W2 WPCONV |
| 3/4 | 0xFEBA | I3 | W3 WPCONV |
| 4/4 | 0xFEBB | I4 | W4 WPCONV |

---

## Operands

### Operand 1 (Destination)

**Type:** Packed BCD (with scaling factor)
**Access:** Write

The destination receives the packed BCD representation of the binary value in the register.

**Scaling Factor Effects:**
- **Positive scaling:** Extends with high-order zeros
- **Negative scaling:** Truncates least significant digits
- **Zero scaling:** Direct conversion

**Supported modes:**
- **LOCAL** - Local variable (packed BCD)
- **RECORD** - Record field (packed BCD)
- **PRE_INDEXED** - Indexed array element
- **ABSOLUTE** - Absolute memory address

**Note:** Destination must have BCD data type with appropriate scaling factor.

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **BCD overflow (BO):** Result doesn't fit in destination's precision

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to result's sign bit
- **BO (BCD Overflow):** Set if overflow occurred
- **K (Overflow Flag):** Set if BO is set
- **C (Carry):** Unaffected
- **V (Overflow):** Unaffected

---

## Examples

### Example 1: Convert binary to packed decimal

```assembly
        % Convert W1 (binary 12345) to packed BCD
        W1 := 12345
        W1 WPCONV IFIELD
        % IFIELD now contains 12345 in packed BCD
```

**Explanation:** Basic conversion from binary integer to packed decimal format.

### Example 2: Financial calculation with scaling

```assembly
        % Convert cents (binary) to dollars (BCD with 2 decimals)
        W2 := 499                   % 499 cents = $4.99
        W2 WPCONV AMOUNT_BCD        % Scaled BCD: $4.99
```

**Explanation:** Scaling factor of destination determines decimal point placement for monetary values.

### Example 3: Loop converting array elements

```assembly
        % Convert binary array to packed BCD array
        W3 := 0                     % Index
CONV_LOOP:
        W1 := BINARY_ARRAY(W3)
        W1 WPCONV BCD_ARRAY(W3)
        W3 INC
        W3 COMP ARRAY_SIZE
        IF<GO CONV_LOOP
```

**Explanation:** Batch conversion of binary values to packed BCD for database export.

### Example 4: Handle BCD overflow

```assembly
        % Convert with overflow handling
        W4 := LARGE_VALUE
        W4 WPCONV DEST_FIELD
        IF BO GO OVERFLOW_HANDLER   % Check BO flag
        % Conversion succeeded
        GO CONTINUE
OVERFLOW_HANDLER:
        % Handle overflow condition
        CALL LOG_OVERFLOW_ERROR
CONTINUE:
```

**Explanation:** Checking BO flag to detect when value doesn't fit in packed BCD field.

### Example 5: Precision control with scaling

```assembly
        % Convert with truncation (negative scaling)
        W1 := 123456
        W1 WPCONV ROUNDED_FIELD     % Negative scale: loses low digits
        % Result might be 123400 (depends on scale)
```

**Explanation:** Negative scaling factor truncates least significant digits.

### Example 6: Database field conversion

```assembly
        % Prepare record for database write
        W2 := QUANTITY
        W2 WPCONV RECORD.QTY_FIELD
        W3 := PRICE
        W3 WPCONV RECORD.PRICE_FIELD
        CALL WRITE_DATABASE_RECORD
```

**Explanation:** Converting internal binary values to packed BCD for database storage.

### Example 7: Display formatting preparation

```assembly
        % Convert for decimal display
        W1 := COUNTER_VALUE
        W1 WPCONV DISPLAY_BUFFER
        CALL FORMAT_AND_PRINT
```

**Explanation:** Convert binary counter to packed BCD for decimal display output.

---

## Performance Notes

- **Execution:** 10-15 cycles
  - Binary to BCD conversion: 8-12 cycles
  - Scaling adjustment: +2-3 cycles
  - Overflow check: +1 cycle
- **Implementation:** Software conversion algorithm
- **Optimization:** Batch conversions when possible

**Usage recommendations:**
- Use for financial/decimal-precision calculations
- Check BO flag after conversion for large values
- Consider scaling factor to avoid overflow
- Pair with packed BCD arithmetic instructions (PADD, PSUB, etc.)
- Efficient for batch database conversions

**Comparison with related instructions:**
- `WPCONV` vs `PWCONV`: WPCONV is word→packed, PWCONV is packed→word
- Part of packed BCD instruction family (PADD, PSUB, PMUL, PDIV)
- Essential bridge between binary and BCD arithmetic

---

## Reference Manual

**Section:** §17.10
**Title:** Convert binary word to packed

---

## See Also

- [PWCONV](pwconv.md) - Convert packed to binary word (inverse operation)
- [PADD](padd.md) - Packed BCD addition
- [PSUB](psub.md) - Packed BCD subtraction
- [PMUL](pmul.md) - Packed BCD multiplication
- [PDIV](pdiv.md) - Packed BCD division
- [Packed BCD](../../ND500_PACKED_BCD.md) - Packed decimal format and the decimal instructions
