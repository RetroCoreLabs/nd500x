# POLY - Polynomial Evaluation

## Overview

**Mnemonic:** `poly`
**Function:** Polynomial evaluation
**Class:** FLOAT_MATH
**Privilege:** user
**Format:** `tn POLY <x>, <m>, <cm>, ..., <c1>, <c0>`

---

## Description

Evaluates a polynomial of degree m using Horner's method. The instruction computes: `c(m)*x^m + c(m-1)*x^(m-1) + ... + c(1)*x + c(0)` and stores the result in the specified floating-point register.

The polynomial degree `<m>` must be a positive constant less than 256. The instruction requires m+1 coefficient operands following the degree specification.

**Operation:**
```
result = c(m)
for i = m-1 down to 0:
    result = result * x + c(i)
Rn = result
```

**Key Characteristics:**
- Hardware-accelerated Horner's method (optimal evaluation)
- Variable operand instruction (m+3 operands total)
- Degree m must be constant 0-255 at assembly time
- Supports float (F) and double (D) precision
- More efficient than manual multiply-add loop
- Single instruction evaluates entire polynomial
- Common in scientific computing and function approximation
- Essential for Taylor series and numerical methods

**Common Use Cases:**
- Mathematical function approximation
- Taylor series evaluation
- Interpolation polynomials
- Curve fitting
- Signal processing transformations
- Scientific computations

**Operands:** Variable (x, m, coefficients)
**Variants:** 8 opcodes (2 float types × 4 registers)

---

## Variants

| Variant | Opcode | Type | Register | Assembly |
|---------|--------|------|----------|----------|
| 1-4 | 0xFCE0-0xFCE3 | F | I1-I4 | Fn POLY |
| 5-8 | 0xFCE4-0xFCE7 | D | I1-I4 | Dn POLY |

---

## Operands

**Operand 1** (X value, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: F or D (matches instruction prefix)
- **Role**: Independent variable value

**Operand 2** (Degree m, Constant):
- **Addressing modes**: CONSTANT only
- **Data type**: Byte constant
- **Role**: Polynomial degree (0-255)
- **Restriction**: Must be positive constant < 256

**Operands 3-(m+3)** (Coefficients, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: F or D (matches instruction prefix)
- **Count**: m+1 coefficients (c(m) through c(0))
- **Order**: Highest degree first, constant term last

---

## Trap Conditions

- **Addressing traps**: Invalid address, page fault, protection violation
- **Floating overflow (FO)**: Result or intermediate exceeds float range
- **Floating underflow (FU)**: Result or intermediate too small
- **Illegal operand specifier (IOS)**: m >= 256 or non-constant m

**Note:** Traps delayed until instruction completion - intermediate overflows don't interrupt

---

## Data Status Bits

- **Z (Zero)**: Set if final result = 0, cleared otherwise
- **S (Sign)**: Set to final result's sign bit
- **FU (Floating Underflow)**: Set if underflow occurred (any stage)
- **FO (Floating Overflow)**: Set if overflow occurred (any stage)

---

## Examples

### Example 1: Quadratic polynomial (ax² + bx + c)
```assembly
        % Evaluate A*X^2 + B*X + C
        F3 POLY X, 2, A, B, C
```

### Example 2: Cubic polynomial
```assembly
        % f(x) = 2x³ + 3x² - 5x + 7
        D1 POLY B.X, 3, 2.0, 3.0, -5.0, 7.0
```

### Example 3: Linear function (mx + b)
```assembly
        % Simple line: slope * x + intercept
        F2 POLY INPUT, 1, SLOPE, INTERCEPT
```

### Example 4: Taylor series approximation
```assembly
        % Approximate e^x using Taylor series (degree 5)
        % e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + x⁵/5!
        D2 POLY ANGLE, 5, 0.0083333, 0.0416667, 0.1666667, 0.5, 1.0, 1.0
```

### Example 5: Constant polynomial
```assembly
        % Evaluate constant (degree 0)
        F1 POLY X_VAL, 0, CONSTANT
```

### Example 6: High-degree polynomial
```assembly
        % Degree 7 polynomial with local coefficients
        D3 POLY B.X_VALUE, 7, B.C7, B.C6, B.C5, B.C4, B.C3, B.C2, B.C1, B.C0
```

### Example 7: Sine approximation
```assembly
        % sin(x) ≈ x - x³/6 + x⁵/120 (for small x)
        F4 POLY ANGLE, 5, 0.0, 0.0083333, 0.0, -0.1666667, 0.0, 1.0
```

### Example 8: Temperature conversion polynomial
```assembly
        % Non-linear temperature conversion
        D1 POLY TEMP_C, 2, A_COEFF, B_COEFF, C_COEFF
```

### Example 9: Interpolation
```assembly
        % Lagrange interpolation polynomial
        F1 POLY T, 3, C3, C2, C1, C0
```

### Example 10: Signal processing
```assembly
        % Digital filter transfer function
        D4 POLY Z_VALUE, 4, B4, B3, B2, B1, B0
```

---

## Performance Notes

- **Execution**: Variable, ~(2*m + 5) cycles
  - Degree 2: ~9 cycles
  - Degree 5: ~15 cycles
  - Degree 10: ~25 cycles
- **Method**: Uses Horner's method (optimal for evaluation)
- **Precision**: Full floating-point precision maintained
- **Trap delay**: Overflow/underflow traps delayed until completion

**Horner's method example:**
```
% Traditional: ax² + bx + c requires 3 multiplies, 2 adds
% Horner: ((a*x) + b)*x + c requires 2 multiplies, 2 adds

POLY evaluates: c(m) for i=m-1 to 0: result*x + c(i)
```

**Coefficient ordering:**
```assembly
% Polynomial: 3x³ + 2x² - x + 5
% Written as: POLY x, 3, 3, 2, -1, 5
%             POLY x, m, c(m), c(m-1), ..., c(1), c(0)
%                          ↑ highest      ↑ constant
```

**Optimization tips:**
```assembly
% Pre-load x value for multiple evaluations:
F1 MOVE X_VALUE
F2 POLY I1, 3, A, B, C, D
F3 POLY I1, 2, E, F, G

% Use constants when possible (faster encoding):
F1 POLY X, 2, 1.5, -2.3, 4.7

% Avoid very high degrees (use piecewise instead):
% Instead of degree-20, use multiple degree-5 over ranges
```

**Error handling:**
```assembly
% Check for overflow/underflow after POLY:
F3 POLY X, 5, C5, C4, C3, C2, C1, C0
IF FO GOTO OVERFLOW_HANDLER
IF FU GOTO UNDERFLOW_HANDLER
```

**Degree limits:**
```assembly
% Maximum degree: 255
F1 POLY X, 255, ... % Legal but very slow

% Degree must be constant:
W1 := 5
F1 POLY X, I1, ...  % ILLEGAL - IOS trap

% Correct:
F1 POLY X, 5, ...   % Constant degree
```

---

## Reference Manual

**Section:** §12.3
**Title:** Polynomial

---

## See Also

- [MULAD](mulad.md) - Multiply-add (for manual polynomial evaluation)
- [PSUM](psum.md) - Sum of products
- [MUL](mul.md) - Floating-point multiplication
- [ADD](add.md) - Floating-point addition
