# SQRT - Square Root

## Overview

**Mnemonic:** `sqrt`
**Function:** Square root
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `Fn SQRT <argument>` or `Dn SQRT <argument>`

---

## Description

Calculates the square root of a floating-point argument and loads the result into the specified float or double-float register. This instruction implements the principal square root function (√x), returning the positive root for non-negative arguments.

The instruction reads the argument from memory or a register, computes its square root using hardware-implemented algorithms (typically polynomial approximation or Newton-Raphson iteration), and stores the result in the destination register specified by the register number suffix (n = 1-4).

Mathematical properties:
- √(x²) = |x| (absolute value)
- √(xy) = √x · √y for non-negative x, y
- √0 = 0 (sets Z flag)
- √(negative) = invalid operation trap

A negative argument is illegal and causes an invalid operation trap (IVO), with the result register set to zero. This trap allows error handling for domain violations, essential in scientific and engineering calculations where negative square roots indicate data or logic errors.

Common applications include:
- Distance calculations (Euclidean distance: √(dx² + dy²))
- Standard deviation and variance computations
- Vector magnitude/normalization (√(x² + y² + z²))
- Root mean square (RMS) calculations
- Geometric mean computations
- Pythagorean theorem implementations

The instruction supports both single-precision (F) and double-precision (D) operatio ns, with 8 register variants (F1-F4/D1-D4) allowing flexible register allocation in complex expressions.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

| Variant | Opcode | Register | Assembly Notation | Data Type |
|---------|--------|----------|-------------------|-----------|
| 1/8 | 0xFCD4 | F1/D1 | F1 SQRT / D1 SQRT | Float/Double |
| 2/8 | 0xFCD5 | F2/D2 | F2 SQRT / D2 SQRT | Float/Double |
| 3/8 | 0xFCD6 | F3/D3 | F3 SQRT / D3 SQRT | Float/Double |
| 4/8 | 0xFCD7 | F4/D4 | F4 SQRT / D4 SQRT | Float/Double |
| 5/8 | 0xFCD8 | R1 | R1 SQRT | Float/Double |
| 6/8 | 0xFCD9 | R2 | R2 SQRT | Float/Double |
| 7/8 | 0xFCDA | R3 | R3 SQRT | Float/Double |
| 8/8 | 0xFCDB | R4 | R4 SQRT | Float/Double |

---

## Operands

**Operand 1** (Argument, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Value to take square root of (must be non-negative)

**Result**: Stored in specified register (Fn or Dn where n = 1-4)

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Invalid operation (IVO)**: Argument is negative

---

## Data Status Bits

- **Z (Zero)**: Set if result equals zero (argument was zero)
- **S (Sign)**: Always cleared (0) - square root is never negative

---

## Examples

### Example 1: Euclidean distance calculation

```assembly
% Calculate distance = sqrt(dx² + dy²)
        F MULT DX, DX, DX_SQUARED
        F MULT DY, DY, DY_SQUARED
        F ADD DX_SQUARED, DY_SQUARED, SUM_SQUARES
        F1 SQRT SUM_SQUARES
        % F1 now contains the distance
```

### Example 2: Vector magnitude (3D)

```assembly
% Calculate length = sqrt(x² + y² + z²)
        F MULT X, X, X2
        F MULT Y, Y, Y2
        F MULT Z, Z, Z2
        F ADD X2, Y2, TEMP
        F ADD TEMP, Z2, MAGNITUDE_SQ
        F1 SQRT MAGNITUDE_SQ
```

### Example 3: Standard deviation calculation

```assembly
% stdev = sqrt(variance)
        D1 SQRT VARIANCE
        % D1 contains standard deviation
        IF=0 Z GO ZERO_DEVIATION
```

### Example 4: Normalize vector component

```assembly
% Normalize: x_norm = x / sqrt(x² + y² + z²)
        D1 SQRT MAGNITUDE_SQUARED
        D DIV X_COMPONENT, D1, X_NORMALIZED
```

### Example 5: Constant square root

```assembly
% Calculate sqrt(2) for normalization constant
        F1 SQRT 2.0
        % F1 = 1.4142135... (√2)
```

### Example 6: Array processing with domain check

```assembly
% Compute square roots with error handling
SQRT_ARRAY:
        W1 CLR
LOOP:
        F1 SQRT ARRAY(I1)          % May trap if negative
        F MOVE F1, RESULTS(I1)
        W1 ADD 1, I1
        W1 COMP I1, COUNT
        IF<GO LOOP
```

### Example 7: Quadratic formula

```assembly
% x = (-b + sqrt(b² - 4ac)) / (2a)
        F MULT B, B, B_SQUARED
        F MULT A, C, AC
        F MULT AC, 4.0, FOUR_AC
        F SUB B_SQUARED, FOUR_AC, DISCRIMINANT
        F1 SQRT DISCRIMINANT       % Traps if discriminant < 0
        F SUB 0.0, B, NEG_B
        F ADD NEG_B, F1, NUMERATOR
```

---

## Performance Notes

- **Algorithm**: Hardware polynomial approximation or Newton-Raphson iteration
- **Precision**: Full IEEE 754 precision for result type
- **Domain Check**: Negative arguments trapped before computation
- **Zero Handling**: √0 = 0 exactly, sets Z flag
- **Register Variants**: 8 variants (F1-F4, D1-D4) for flexible allocation
- **Typical Use**: Geometry, statistics, vector math, signal processing
- **Performance**: Significantly faster than software implementation
- **Accuracy**: Correctly rounded per IEEE 754 standard

---

## Reference Manual

**Section:** §12.4
**Title:** Square root

---

## See Also

- [EXP](exp.md) - Exponential function (e^x)
- [POLY](poly.md) - Polynomial evaluation
- [MULT](mult.md) - Multiplication for x² calculations
- [DIV](div.md) - Division for normalization
