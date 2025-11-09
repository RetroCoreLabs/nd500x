# SIN - Sine Function

## Overview

**Mnemonic:** `sin`
**Function:** Sine (trigonometric)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `Fn SIN <argument>` or `Dn SIN <argument>`

---

## Description

Calculates the trigonometric sine of an angle specified in radians and loads the result into the specified float or double-float register. This instruction implements the sine function sin(θ), one of the fundamental trigonometric functions essential for geometry, physics, and signal processing.

The instruction reads the angle argument θ (in radians) from memory or a register, computes sin(θ) using hardware-implemented algorithms (typically range reduction combined with polynomial approximation or CORDIC), and stores the result in the destination register specified by the register number suffix (n = 1-4).

Mathematical properties:
- sin(0) = 0
- sin(π/2) = 1
- sin(π) = 0
- sin(3π/2) = -1
- sin(2π) = 0 (periodic with period 2π)
- sin(-θ) = -sin(θ) (odd function)
- Range: [-1, +1]

The sine function is periodic with period 2π (≈6.283185...), meaning sin(θ) = sin(θ + 2πn) for any integer n. The hardware typically performs range reduction to normalize the argument into a standard range before computation, ensuring accuracy across all input values.

Common applications include:
- Coordinate transformations and rotations
- Wave generation (sine waves, oscillators)
- Signal processing and Fourier analysis
- Physics simulations (pendulum, harmonic motion)
- Graphics and animation (circular motion, curves)
- Navigation and trigonometric surveying
- Electromagnetic field calculations

The instruction supports both single-precision (F) and double-precision (D) operations, with 8 register variants (F1-F4/D1-D4) allowing flexible register allocation in complex trigonometric expressions.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

| Variant | Opcode | Register | Assembly Notation | Data Type |
|---------|--------|----------|-------------------|-----------|
| 1/8 | 0xFF58 | F1/D1 | F1 SIN / D1 SIN | Float/Double |
| 2/8 | 0xFF59 | F2/D2 | F2 SIN / D2 SIN | Float/Double |
| 3/8 | 0xFF5A | F3/D3 | F3 SIN / D3 SIN | Float/Double |
| 4/8 | 0xFF5B | F4/D4 | F4 SIN / D4 SIN | Float/Double |
| 5/8 | 0xFF84 | R1 | R1 SIN | Float/Double |
| 6/8 | 0xFF85 | R2 | R2 SIN | Float/Double |
| 7/8 | 0xFF86 | R3 | R3 SIN | Float/Double |
| 8/8 | 0xFF87 | R4 | R4 SIN | Float/Double |

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

- **Z (Zero)**: Set if result equals zero (angle is multiple of π)
- **S (Sign)**: Set if result is negative (angle in range (π, 2π))

---

## Examples

### Example 1: Sine of 45 degrees

```assembly
% Calculate sin(45°) = sin(π/4)
        F1 SIN 0.785398163      % π/4 radians
        % F1 = 0.707106781... (√2/2)
```

### Example 2: Generate sine wave sample

```assembly
% Generate sine wave: y = A * sin(2πft)
        F MULT TWO_PI, FREQUENCY, TWO_PI_F
        F MULT TWO_PI_F, TIME, ANGLE
        F1 SIN ANGLE
        F MULT AMPLITUDE, F1, SAMPLE
        % SAMPLE = sine wave value at time t
```

### Example 3: Rotate 2D vector

```assembly
% Rotate point (x,y) by angle θ
% x' = x*cos(θ) - y*sin(θ)
% y' = x*sin(θ) + y*cos(θ)
        F1 SIN ANGLE
        F2 COS ANGLE
        F MULT X, F2, X_COS
        F MULT Y, F1, Y_SIN
        F SUB X_COS, Y_SIN, X_PRIME
```

### Example 4: Harmonic oscillator

```assembly
% Position: x(t) = A * sin(ωt + φ)
        F MULT OMEGA, T, OMEGA_T
        F ADD OMEGA_T, PHASE, ARGUMENT
        F1 SIN ARGUMENT
        F MULT AMPLITUDE_A, F1, POSITION
```

### Example 5: Sine table generation

```assembly
% Generate sine lookup table
        W1 CLR                   % Index
        F MOVE 0.0, ANGLE
LOOP:
        F1 SIN ANGLE
        F MOVE F1, TABLE(I1)
        F ADD ANGLE, STEP, ANGLE
        W1 ADD 1, I1
        W1 COMP I1, TABLE_SIZE
        IF<GO LOOP
```

### Example 6: Signal phase calculation

```assembly
% Calculate I/Q components: I = cos(θ), Q = sin(θ)
        F1 SIN PHASE_ANGLE
        F2 COS PHASE_ANGLE
        F MULT MAGNITUDE, F2, I_COMPONENT
        F MULT MAGNITUDE, F1, Q_COMPONENT
```

### Example 7: Pendulum simulation

```assembly
% Angular acceleration: α = -(g/L) * sin(θ)
        F1 SIN THETA
        F MULT G_OVER_L, F1, TEMP
        F NEG TEMP, ACCELERATION
        % ACCELERATION = angular acceleration
```

---

## Performance Notes

- **Algorithm**: Range reduction + polynomial/CORDIC approximation
- **Precision**: Full IEEE 754 precision for result type
- **Range**: Input angle can be any value (hardware performs range reduction)
- **Result Range**: Always [-1, +1]
- **Periodicity**: sin(θ) = sin(θ + 2πn) for any integer n
- **Register Variants**: 8 variants (F1-F4, D1-D4) for flexible allocation
- **Typical Use**: Graphics, physics, signal processing, navigation
- **Performance**: Hardware implementation much faster than software
- **Accuracy**: Correctly rounded per IEEE 754 standard
- **Radian Input**: Argument must be in radians, not degrees

---

## Reference Manual

**Section:** §12.5
**Title:** Sine

---

## See Also

- [COS](cos.md) - Cosine function
- [TAN](tan.md) - Tangent function
- [ASIN](asin.md) - Arc sine (inverse sine)
- [ATAN](atan.md) - Arc tangent
