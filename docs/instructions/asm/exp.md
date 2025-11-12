# EXP - Exponential Function

## Overview

**Mnemonic:** `exp`
**Function:** Exponential (e^x)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `Fn EXP <argument>` or `Dn EXP <argument>`

---

## Description

Calculates the exponential function e^x (e raised to the power of the argument) and loads the result into the specified float or double-float register. This instruction implements one of the most important transcendental functions in mathematics, where e = 2.718281828459045... (Euler's number).

The instruction reads the argument x from memory or a register, computes e^x using hardware-implemented algorithms (typically polynomial approximation or range reduction with table lookup), and stores the result in the destination register specified by the register number suffix (n = 1-4).

**Key Characteristics:**
- Hardware-accelerated exponential (e^x) computation
- Result always positive (S flag always 0)
- Limited argument range: ±176.75 (255·ln(2))
- Overflow trap (IVO) for large positive arguments
- Underflow to zero (no trap) for large negative arguments
- Essential for growth/decay, statistics, probability
- Supports float (F) and double (D) precision
- 8 register variants (F1-F4, D1-D4) for flexible allocation

The exponential function has specific range limitations:
- **Maximum argument**: 255·ln(2) ≈ 176.75
  - Arguments exceeding this cause invalid operation trap (IVO)
  - Result register set to largest representable float (≈5.8×10^76)
- **Minimum argument**: -255·ln(2) ≈ -176.75
  - Arguments below this return zero (underflow)
  - No trap occurs for underflow

These limits reflect the hardware's internal representation using binary exponents, where 255 is the maximum 8-bit exponent value for the floating-point format.

Mathematical properties:
- e^0 = 1
- e^1 = e ≈ 2.71828...
- e^(ln(x)) = x (inverse of natural logarithm)
- e^(x+y) = e^x · e^y
- e^(-x) = 1/e^x

Common applications include:
- Compound interest and exponential growth/decay
- Probability distributions (exponential, normal)
- Signal processing (exponential envelopes)
- Natural logarithm inversion (antilog)
- Solving differential equations
- Statistical calculations
- Physics simulations (radioactive decay, cooling)

The instruction supports both single-precision (F) and double-precision (D) operations, with 8 register variants (F1-F4/D1-D4) allowing flexible register allocation in complex expressions.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

| Variant | Opcode | Register | Assembly Notation | Data Type |
|---------|--------|----------|-------------------|-----------|
| 1/8 | 0xFF74 | F1/D1 | F1 EXP / D1 EXP | Float/Double |
| 2/8 | 0xFF75 | F2/D2 | F2 EXP / D2 EXP | Float/Double |
| 3/8 | 0xFF76 | F3/D3 | F3 EXP / D3 EXP | Float/Double |
| 4/8 | 0xFF77 | F4/D4 | F4 EXP / D4 EXP | Float/Double |
| 5/8 | 0xFFA0 | R1 | R1 EXP | Float/Double |
| 6/8 | 0xFFA1 | R2 | R2 EXP | Float/Double |
| 7/8 | 0xFFA2 | R3 | R3 EXP | Float/Double |
| 8/8 | 0xFFA3 | R4 | R4 EXP | Float/Double |

---

## Operands

**Operand 1** (Argument/Exponent, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Exponent value for e^x (valid range ±176.75 approximately)

**Result**: Stored in specified register (Fn or Dn where n = 1-4)

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Invalid operation (IVO)**: Argument exceeds +255·ln(2) (overflow)

---

## Data Status Bits

- **Z (Zero)**: Set if result equals zero (severe underflow)
- **S (Sign)**: Always cleared (0) - e^x is always positive

---

## Examples

### Example 1: Natural logarithm inversion (antilog)

```assembly
% Calculate antilog: x = e^(ln(y))
        D1 EXP NATURAL_LOG
        % D1 = original value
```

### Example 2: Exponential decay

```assembly
% Calculate decay: N(t) = N0 * e^(-λt)
        F MULT DECAY_CONSTANT, TIME, EXPONENT
        F NEG EXPONENT, NEG_EXPONENT
        F1 EXP NEG_EXPONENT
        F MULT N0, F1, N_T
        % N_T = population at time t
```

### Example 3: Gaussian (normal) distribution

```assembly
% Calculate exp(-(x²/2)) for normal distribution
        F MULT X, X, X_SQUARED
        F DIV X_SQUARED, 2.0, X_SQ_HALF
        F NEG X_SQ_HALF, NEG_X_SQ_HALF
        F1 EXP NEG_X_SQ_HALF
        % F1 = exp(-x²/2) component of Gaussian
```

### Example 4: Compound interest

```assembly
% A = P * e^(rt)
        F MULT RATE, TIME, RT
        F1 EXP RT
        F MULT PRINCIPAL, F1, AMOUNT
        % AMOUNT = final value with continuous compounding
```

### Example 5: Exponential constant

```assembly
% Calculate e (Euler's number)
        F1 EXP 1.0
        % F1 = 2.718281828...
```

### Example 6: Sigmoid activation function

```assembly
% sigmoid(x) = 1 / (1 + e^(-x))
        F NEG X, NEG_X
        F1 EXP NEG_X              % e^(-x)
        F ADD 1.0, F1, DENOMINATOR
        F DIV 1.0, DENOMINATOR, SIGMOID
        % SIGMOID = activation value
```

### Example 7: Overflow handling

```assembly
% Safe exponential with overflow detection
        F1 EXP LARGE_VALUE
        % Traps if LARGE_VALUE > 176.75
        % Trap handler can substitute maximum value
OVERFLOW_HANDLER:
        F MOVE MAX_FLOAT, F1
        RET
```

---

## Performance Notes

- **Algorithm**: Hardware range reduction with polynomial approximation
- **Precision**: Full IEEE 754 precision for result type
- **Range Check**: Overflow trap for arguments > +176.75
- **Underflow Handling**: Returns zero for arguments < -176.75 (no trap)
- **Zero Arg**: e^0 = 1.0 exactly
- **Register Variants**: 8 variants (F1-F4, D1-D4) for flexible allocation
- **Typical Use**: Probability, growth/decay, signal processing, statistics
- **Performance**: Hardware implementation much faster than software
- **Accuracy**: Correctly rounded per IEEE 754 standard
- **Maximum Result**: ≈5.8×10^76 (largest representable float)

---

## Reference Manual

**Section:** §12.12
**Title:** Exponential

---

## See Also

- [ALOG](alog.md) - Natural logarithm (ln(x), inverse of EXP)
- [SQRT](sqrt.md) - Square root function
- [POLY](poly.md) - Polynomial evaluation
- [POW](pow.md) - General power function (x^y)
