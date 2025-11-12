# TAN - Tangent Function

## Overview

**Mnemonic:** `tan`
**Function:** Tangent (trigonometric)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `Fn TAN <argument>` or `Dn TAN <argument>`

---

## Description

Calculates the trigonometric tangent of an angle specified in radians and loads the result into the specified float or double-float register. This instruction implements the tangent function tan(θ), defined as the ratio of sine to cosine (sin(θ)/cos(θ)), one of the fundamental trigonometric functions essential for angle calculations, slopes, and phase relationships.

The instruction reads the angle argument θ (in radians) from memory or a register, computes tan(θ) using hardware-implemented algorithms (typically range reduction combined with polynomial approximation or ratio of sin/cos tables), and stores the result in the destination register specified by the register number suffix (n = 1-4).

**Key Characteristics:**
- Hardware-accelerated trigonometric tangent (sin/cos ratio)
- Unbounded range: (-∞, +∞) unlike sin/cos
- Vertical asymptotes at odd multiples of π/2
- Period π (not 2π like sin/cos)
- Supports float (F) and double (D) precision
- 8 register variants (F1-F4, D1-D4) for flexible allocation
- Essential for slopes, projections, phase angles
- Overflow risk near asymptotes (±π/2, ±3π/2, ...)

Mathematical properties:
- tan(0) = 0
- tan(π/4) = 1
- tan(π/2) = undefined (approaches ±∞)
- tan(3π/4) = -1
- tan(π) = 0
- tan(θ) = sin(θ) / cos(θ)
- tan(-θ) = -tan(θ) (odd function)
- Range: (-∞, +∞)

The tangent function has period π (not 2π like sin/cos), meaning tan(θ) = tan(θ + πn) for any integer n. The function has vertical asymptotes (approaches infinity) at odd multiples of π/2 (±π/2, ±3π/2, ...) where cosine equals zero.

For arguments very close to these asymptotes, the result may overflow or produce very large values. The hardware typically handles this by returning the largest representable floating-point value with appropriate sign.

Common applications include:
- Slope and gradient calculations (rise/run)
- Angle determination from coordinate ratios
- Phase angle calculations in AC circuits
- Surveying and navigation (bearings, angles)
- Camera field-of-view and projection calculations
- Optics (angles of refraction and reflection)
- Trigonometric identities and transformations

The instruction supports both single-precision (F) and double-precision (D) operations, with 8 register variants (F1-F4/D1-D4) allowing flexible register allocation in complex trigonometric expressions.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

| Variant | Opcode | Register | Assembly Notation | Data Type |
|---------|--------|----------|-------------------|-----------|
| 1/8 | 0xFF68 | F1/D1 | F1 TAN / D1 TAN | Float/Double |
| 2/8 | 0xFF69 | F2/D2 | F2 TAN / D2 TAN | Float/Double |
| 3/8 | 0xFF6A | F3/D3 | F3 TAN / D3 TAN | Float/Double |
| 4/8 | 0xFF6B | F4/D4 | F4 TAN / D4 TAN | Float/Double |
| 5/8 | 0xFF94 | R1 | R1 TAN | Float/Double |
| 6/8 | 0xFF95 | R2 | R2 TAN | Float/Double |
| 7/8 | 0xFF96 | R3 | R3 TAN | Float/Double |
| 8/8 | 0xFF97 | R4 | R4 TAN | Float/Double |

---

## Operands

**Operand 1** (Angle, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Float (F prefix) or Double (D prefix)
- **Role**: Angle in radians

**Result**: Stored in specified register (Fn or Dn where n = 1-4), range (-∞, +∞)

---

## Trap Conditions

- **Addressing traps**: Invalid address or protection violation
- **Overflow**: May occur for angles very close to ±π/2, ±3π/2, etc.

---

## Data Status Bits

- **Z (Zero)**: Set if result equals zero (angle is multiple of π)
- **S (Sign)**: Set if result is negative

---

## Examples

### Example 1: Calculate slope from angle

```assembly
% slope = tan(θ)
        F1 TAN ANGLE
        % F1 = rise/run ratio (slope)
```

### Example 2: Angle from coordinates

```assembly
% Calculate tan(θ) where θ = atan(y/x)
% Verify: tan(atan(y/x)) = y/x
        F DIV Y, X, RATIO
        F1 ATAN RATIO
        F2 TAN F1
        % F2 should equal Y/X
```

### Example 3: Field of view calculation

```assembly
% Calculate half-angle for camera FOV
% tan(θ/2) = (width/2) / focal_length
        F DIV HALF_WIDTH, FOCAL_LENGTH, TAN_HALF_ANGLE
        F1 ATAN TAN_HALF_ANGLE
        F MULT F1, 2.0, FOV_ANGLE
```

### Example 4: Phase angle in AC circuit

```assembly
% tan(φ) = X/R (reactance/resistance)
        F DIV REACTANCE, RESISTANCE, TAN_ANGLE
        % TAN_ANGLE = tangent of phase angle
```

### Example 5: Surveying bearing calculation

```assembly
% Calculate bearing from north-south and east-west offsets
        F1 TAN BEARING_ANGLE
        F MULT DISTANCE_NS, F1, DISTANCE_EW
        % DISTANCE_EW = offset in east-west direction
```

### Example 6: Refraction angle

```assembly
% Snell's law: n1*sin(θ1) = n2*sin(θ2)
% Often expressed using tan for small angles
        F1 TAN INCIDENT_ANGLE
        F MULT INDEX_RATIO, F1, TAN_REFRACTED
        F1 ATAN TAN_REFRACTED
        % F1 = refracted angle
```

### Example 7: Periodic tangent table

```assembly
% Generate tangent lookup table (avoid asymptotes!)
        W1 CLR
        F MOVE 0.0, ANGLE
LOOP:
        F1 TAN ANGLE
        F MOVE F1, TABLE(I1)
        F ADD ANGLE, STEP, ANGLE  % Ensure STEP avoids π/2
        W1 ADD 1, I1
        W1 COMP I1, TABLE_SIZE
        IF<GO LOOP
```

---

## Performance Notes

- **Algorithm**: Range reduction + polynomial or sin/cos ratio
- **Precision**: Full IEEE 754 precision for result type
- **Range**: Input angle can be any value (hardware performs range reduction)
- **Result Range**: (-∞, +∞) - unbounded unlike sin/cos
- **Asymptotes**: tan(π/2 + nπ) undefined (approaches ±∞)
- **Periodicity**: tan(θ) = tan(θ + πn) for any integer n (period π)
- **Register Variants**: 8 variants (F1-F4, D1-D4) for flexible allocation
- **Typical Use**: Slopes, angles, projections, phase calculations
- **Performance**: Hardware implementation faster than sin/cos ratio
- **Accuracy**: Correctly rounded per IEEE 754 standard except near asymptotes
- **Radian Input**: Argument must be in radians, not degrees
- **Overflow Risk**: Large results near odd multiples of π/2

---

## Reference Manual

**Section:** §12.9
**Title:** Tangent

---

## See Also

- [SIN](sin.md) - Sine function
- [COS](cos.md) - Cosine function
- [ATAN](atan.md) - Arc tangent (inverse tangent)
- [ATAN2](atan2.md) - Two-argument arc tangent
