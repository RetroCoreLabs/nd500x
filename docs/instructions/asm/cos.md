# COS - Cosine Function

## Overview

**Mnemonic:** `cos`
**Function:** Cosine (trigonometric)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `Fn COS <argument>` or `Dn COS <argument>`

---

## Description

Calculates the trigonometric cosine of an angle specified in radians and loads the result into the specified float or double-float register. This instruction implements the cosine function cos(θ), one of the fundamental trigonometric functions complementary to sine, essential for geometry, physics, and signal processing.

The instruction reads the angle argument θ (in radians) from memory or a register, computes cos(θ) using hardware-implemented algorithms (typically range reduction combined with polynomial approximation or CORDIC), and stores the result in the destination register specified by the register number suffix (n = 1-4).

**Key Characteristics:**
- Hardware-accelerated trigonometric cosine computation
- Bounded range: [-1, +1] (never overflows)
- Periodic with period 2π (automatic range reduction)
- Even function: cos(-θ) = cos(θ)
- Phase relationship: cos(θ) = sin(θ + π/2)
- Supports float (F) and double (D) precision
- 8 register variants (F1-F4, D1-D4) for flexible allocation
- Essential for rotations, projections, lighting, power factor

Mathematical properties:
- cos(0) = 1
- cos(π/2) = 0
- cos(π) = -1
- cos(3π/2) = 0
- cos(2π) = 1 (periodic with period 2π)
- cos(-θ) = cos(θ) (even function)
- cos(θ) = sin(θ + π/2) (phase relationship)
- Range: [-1, +1]

The cosine function is periodic with period 2π (≈6.283185...), meaning cos(θ) = cos(θ + 2πn) for any integer n. The hardware typically performs range reduction to normalize the argument into a standard range before computation, ensuring accuracy across all input values.

Common applications include:
- Coordinate transformations and rotations
- Projection calculations (dot products)
- Signal processing (phase components, Fourier analysis)
- Physics simulations (oscillatory motion, wave interference)
- Graphics rendering (lighting, shading, camera projection)
- Navigation and distance calculations
- Power factor calculations in electrical engineering

The instruction supports both single-precision (F) and double-precision (D) operations, with 8 register variants (F1-F4/D1-D4) allowing flexible register allocation in complex trigonometric expressions.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

| Variant | Opcode | Register | Assembly Notation | Data Type |
|---------|--------|----------|-------------------|-----------|
| 1/8 | 0xFF60 | F1/D1 | F1 COS / D1 COS | Float/Double |
| 2/8 | 0xFF61 | F2/D2 | F2 COS / D2 COS | Float/Double |
| 3/8 | 0xFF62 | F3/D3 | F3 COS / D3 COS | Float/Double |
| 4/8 | 0xFF63 | F4/D4 | F4 COS / D4 COS | Float/Double |
| 5/8 | 0xFF8C | R1 | R1 COS | Float/Double |
| 6/8 | 0xFF8D | R2 | R2 COS | Float/Double |
| 7/8 | 0xFF8E | R3 | R3 COS | Float/Double |
| 8/8 | 0xFF8F | R4 | R4 COS | Float/Double |

---

## Operands

**Operand 1** (Angle, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Angle in radians

**Result**: Stored in specified register (Fn or Dn where n = 1-4), range [-1, +1]

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation

---

## Data Status Bits

- **Z (Zero)**: Set if result equals zero (angle is odd multiple of π/2)
- **S (Sign)**: Set if result is negative (angle in range (π/2, 3π/2))

---

## Examples

### Example 1: Cosine of 60 degrees

```assembly
% Calculate cos(60°) = cos(π/3)
        F1 COS 1.047197551      % π/3 radians
        % F1 = 0.5
```

### Example 2: Rotate 2D vector

```assembly
% Rotate point (x,y) by angle θ
% x' = x*cos(θ) - y*sin(θ)
% y' = x*sin(θ) + y*cos(θ)
        F1 COS ANGLE
        F2 SIN ANGLE
        F MULT X, F1, X_COS
        F MULT Y, F2, Y_SIN
        F SUB X_COS, Y_SIN, X_PRIME
        F MULT X, F2, X_SIN
        F MULT Y, F1, Y_COS
        F ADD X_SIN, Y_COS, Y_PRIME
```

### Example 3: Dot product projection

```assembly
% Project vector onto axis: proj = |v| * cos(θ)
        F1 COS ANGLE
        F MULT MAGNITUDE, F1, PROJECTION
        % PROJECTION = component along axis
```

### Example 4: Damped harmonic motion

```assembly
% Position: x(t) = A * e^(-γt) * cos(ωt)
        F MULT NEG_GAMMA, T, DECAY_FACTOR
        F1 EXP DECAY_FACTOR
        F MULT OMEGA, T, OMEGA_T
        F2 COS OMEGA_T
        F MULT F1, F2, ENVELOPE
        F MULT AMPLITUDE, ENVELOPE, POSITION
```

### Example 5: Lambert's cosine law (lighting)

```assembly
% Intensity = I0 * cos(θ) for diffuse reflection
        F1 COS ANGLE_TO_NORMAL
        IF<0 Z GO NO_LIGHT       % Back-facing
        F MULT BASE_INTENSITY, F1, INTENSITY
```

### Example 6: Power factor calculation

```assembly
% Power factor = cos(phase angle)
        F1 COS PHASE_ANGLE
        % F1 = power factor (0 to 1 for leading/lagging)
```

### Example 7: Generate cosine wave

```assembly
% Generate cosine wave: y = A * cos(2πft + φ)
        F MULT TWO_PI, FREQUENCY, TWO_PI_F
        F MULT TWO_PI_F, TIME, ANGLE
        F ADD ANGLE, PHASE, TOTAL_ANGLE
        F1 COS TOTAL_ANGLE
        F MULT AMPLITUDE, F1, SAMPLE
```

---

## Performance Notes

- **Algorithm**: Range reduction + polynomial/CORDIC approximation
- **Precision**: Full IEEE 754 precision for result type
- **Range**: Input angle can be any value (hardware performs range reduction)
- **Result Range**: Always [-1, +1]
- **Periodicity**: cos(θ) = cos(θ + 2πn) for any integer n
- **Relationship**: cos(θ) = sin(θ + π/2)
- **Register Variants**: 8 variants (F1-F4, D1-D4) for flexible allocation
- **Typical Use**: Graphics, physics, projections, lighting, signal processing
- **Performance**: Hardware implementation much faster than software
- **Accuracy**: Correctly rounded per IEEE 754 standard
- **Radian Input**: Argument must be in radians, not degrees

---

## Reference Manual

**Section:** §12.7
**Title:** Cosine

---

## See Also

- [SIN](sin.md) - Sine function
- [TAN](tan.md) - Tangent function
- [ACOS](acos.md) - Arc cosine (inverse cosine)
- [ATAN](atan.md) - Arc tangent
