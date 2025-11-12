# PSUM - Sum of Products

## Overview

**Mnemonic:** `psum`
**Function:** Multiply and accumulate
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `tn PSUM <x>, <y>`

---

## Description

Multiplies two operands and adds the product to the specified register (multiply-accumulate operation). This is a fused operation commonly used in vector mathematics, signal processing, and financial calculations. The instruction efficiently combines multiplication and addition in a single operation.

**Operation:**
```
<x> * <y> + Rn → Rn
```

**Key Characteristics:**
- Fused multiply-accumulate (MAC) operation
- Result accumulates in specified register (Rn)
- Supports all data types: BY, H, W, F, D
- 20 variants for different operand combinations
- Efficient for dot products and polynomial evaluation
- Sets flags based on final result

**Common Use Cases:**
- Vector dot product calculation
- Digital signal processing (FIR filters)
- Financial calculations (price × quantity)
- Matrix multiplication
- Polynomial evaluation
- Running sum of products

**Operands:** 2 (both read, register accumulates)
**Variants:** 20 opcodes

---

## Examples

### Example 1: Accumulate invoice line item

```assembly
        % Calculate running total: price × quantity
        F4 CLR
        F4 PSUM B.UNITCOST, B.UNITS
```

**Explanation:** Clear accumulator, then multiply unit cost by units and add to F4.

### Example 2: Vector dot product

```assembly
        % Compute dot product of two vectors
        W1 CLR
        W1 PSUM VECTOR_A(W2), VECTOR_B(W2)
        W2 ADD 1
        W1 PSUM VECTOR_A(W2), VECTOR_B(W2)
        W2 ADD 1
        W1 PSUM VECTOR_A(W2), VECTOR_B(W2)
```

**Explanation:** Accumulate products of corresponding vector elements.

### Example 3: FIR filter implementation

```assembly
        % Digital filter: y = Σ(coeff[i] * sample[i])
        F1 CLR
        F1 PSUM COEFF(W1), SAMPLES(W1)
        W1 ADD 1
        F1 PSUM COEFF(W1), SAMPLES(W1)
        W1 ADD 1
        F1 PSUM COEFF(W1), SAMPLES(W1)
```

**Explanation:** FIR filter tap computation using multiply-accumulate.

### Example 4: Polynomial evaluation

```assembly
        % Evaluate: ax² + bx + c
        F1 MOVE X
        F1 PSUM X, A              % ax² + F1
        F1 PSUM B, X              % bx + result
        F1 ADD C                  % + c
```

**Explanation:** Horner's method for polynomial evaluation.

### Example 5: Financial calculation

```assembly
        % Calculate total: Σ(price[i] × qty[i])
        D1 CLR
        W2 CLR
LOOP:   D1 PSUM PRICES(W2), QUANTITIES(W2)
        W2 ADD 1
        W2 COMP COUNT
        IF<GO LOOP
```

**Explanation:** Sum of products for multi-line invoice.

### Example 6: Byte-level MAC

```assembly
        % Accumulate byte products
        BY1 CLR
        BY1 PSUM FACTOR1, FACTOR2
        BY1 PSUM FACTOR3, FACTOR4
```

**Explanation:** Byte-sized multiply-accumulate for compact data.

### Example 7: Double precision accumulation

```assembly
        % High-precision financial math
        D1 CLR
        D1 PSUM RATE, PRINCIPAL
        D1 PSUM DISCOUNT, AMOUNT
```

**Explanation:** Double-precision for financial accuracy.

---

## Variants Table

| Variant | Opcode | Type | Assembly | Description |
|---------|--------|------|----------|-------------|
| 1/20 | 0xFCF8 | BY | BYn PSUM | Byte multiply-add |
| 2/20 | 0xFCF9 | BY | BYn PSUM | Byte multiply-add |
| 3/20 | 0xFCFA | BY | BYn PSUM | Byte multiply-add |
| 4/20 | 0xFCFB | BY | BYn PSUM | Byte multiply-add |
| 5/20 | 0xFCFC | H | Hn PSUM | Halfword multiply-add |
| 6/20 | 0xFCFD | H | Hn PSUM | Halfword multiply-add |
| 7/20 | 0xFCFE | H | Hn PSUM | Halfword multiply-add |
| 8/20 | 0xFCFF | H | Hn PSUM | Halfword multiply-add |
| 9/20 | 0xFD00 | W | Wn PSUM | Word multiply-add |
| 10/20 | 0xFD01 | W | Wn PSUM | Word multiply-add |
| 11/20 | 0xFD02 | W | Wn PSUM | Word multiply-add |
| 12/20 | 0xFD03 | W | Wn PSUM | Word multiply-add |
| 13/20 | 0xFD04 | F | Fn PSUM | Float multiply-add |
| 14/20 | 0xFD05 | F | Fn PSUM | Float multiply-add |
| 15/20 | 0xFD06 | F | Fn PSUM | Float multiply-add |
| 16/20 | 0xFD07 | F | Fn PSUM | Float multiply-add |
| 17/20 | 0xFD08 | D | Dn PSUM | Double multiply-add |
| 18/20 | 0xFD09 | D | Dn PSUM | Double multiply-add |
| 19/20 | 0xFD0A | D | Dn PSUM | Double multiply-add |
| 20/20 | 0xFD0B | D | Dn PSUM | Double multiply-add |

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **Integer overflow (O)**: Integer result overflow (BY, H, W)
- **Floating overflow (FO)**: Float/double result too large
- **Floating underflow (FU)**: Float/double result too small

---

## Data Status Bits

- **Z (Zero)**: Result equals zero
- **S (Sign)**: Result sign bit
- **C (Carry)**: Carry from most significant bit (integer only)
- **O (Overflow)**: Integer overflow occurred
- **FU (Float Underflow)**: Floating underflow occurred
- **FO (Float Overflow)**: Floating overflow occurred

---

## Reference Manual

**Section:** §11.20
**Title:** Sum of Products

---

## See Also

- [MUL](mul.md) - Multiply
- [ADD](add.md) - Add
- [PDIF](pdif.md) - Product difference
- [DIV](div.md) - Divide
