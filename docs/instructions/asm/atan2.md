# ATAN2 - Arc Tangent Two Arguments

## Overview

**Mnemonic:** `atan2`
**Function:** Arc tangent with quadrant information (inverse tangent of y/x)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ATAN2 <num>,<den>`

---

## Description

Calculates the trigonometric arctangent of `<num>/<den>` and loads the result into the specified float or double float register. The result value gives the angle in radians in the correct quadrant in the range -π to +π (-3.1416 to +3.1416).

**Operation:**
```
Fn/Dn = atan2(<num>, <den>)
```

**Key Characteristics:**
- Quadrant-aware arctangent (full -π to +π range)
- Two-argument version of ATAN (numerator and denominator separate)
- Essential for Cartesian to polar coordinate conversion
- Returns correct angle for all four quadrants
- IVO trap when both arguments are zero
- 8 register variants (F1-F4, D1-D4)
- Slower than ATAN (180-220 cycles vs 150-180)
- Hardware-accelerated transcendental function
- Result loaded into specified float/double register

Unlike the single-argument ATAN instruction, ATAN2 takes both numerator and denominator separately, allowing it to determine the correct quadrant based on the signs of both arguments. This is essential for converting Cartesian coordinates (x, y) to polar coordinates (r, θ).

If both `<num>` and `<den>` are zero, an invalid operation (IVO) trap condition occurs and the specified register is set to zero.

**Operands:** 2
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF70 | F | 1 | Float |
| 2/8 | 0xFF71 | F | 2 | Float |
| 3/8 | 0xFF72 | F | 3 | Float |
| 4/8 | 0xFF73 | F | 4 | Float |
| 5/8 | 0xFF9C | D | 1 | Double Float |
| 6/8 | 0xFF9D | D | 2 | Double Float |
| 7/8 | 0xFF9E | D | 3 | Double Float |
| 8/8 | 0xFF9F | D | 4 | Double Float |

---

## Operands

### Operand 1 (Numerator)

The numerator operand (typically y-coordinate or opposite side).

**Type:** Float or Double Float (matching register type)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Float register (A1-A4 or D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

### Operand 2 (Denominator)

The denominator operand (typically x-coordinate or adjacent side).

**Type:** Float or Double Float (matching register type)
**Access:** Read

**Supported modes:**
- **LOCAL** - Local data area access (B.variable)
- **RECORD** - Record-relative access (R.field)
- **CONSTANT** - Immediate constant value
- **REGISTER** - Float register (A1-A4 or D1-D4)
- **PRE_INDEXED** - Indexed addressing with offset
- **ABSOLUTE** - Absolute memory address

---

## Trap Conditions

- **Addressing traps:** Invalid address, descriptor range violation, page fault, protection violation
- **Invalid operation (IVO):** Both `<num>` and `<den>` are zero; result register set to zero

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of result (bit 31 for float, bit 63 for double)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Arc tangent from two variables

```assembly
        % Load arc tangent of WIDTH/DIST
        D3 ATAN2 WIDTH, DIST
```

### Example 2: Cartesian to polar conversion

```assembly
        % Convert (x, y) to angle θ
        F1 ATAN2 B.Y_COORD, B.X_COORD
        F1 =: B.THETA            % Angle in radians
```

### Example 3: Calculate bearing from two points

```assembly
        % Calculate bearing from point1 to point2
        F2 := B.POINT2_Y
        F2 - B.POINT1_Y          % dy
        F3 := B.POINT2_X
        F3 - B.POINT1_X          % dx
        F1 ATAN2 F2, F3          % bearing in radians
        F1 =: B.BEARING
```

### Example 4: Quadrant-aware angle calculation

```assembly
        % Calculate angle with correct quadrant
        % ATAN2 handles all four quadrants correctly:
        %   Quadrant I:   x > 0, y > 0  -> 0 to π/2
        %   Quadrant II:  x < 0, y > 0  -> π/2 to π
        %   Quadrant III: x < 0, y < 0  -> -π to -π/2
        %   Quadrant IV:  x > 0, y < 0  -> -π/2 to 0

        D1 ATAN2 B.NUMERATOR, B.DENOMINATOR
        D1 =: B.FULL_ANGLE
```

---

## Performance Notes

- **Typical cycles:** 180-220 cycles (complex transcendental function with quadrant logic)
- **Best case:** ~180 cycles (simple argument values)
- **Worst case:** ~220 cycles (complex argument values, table lookup + polynomial approximation + quadrant determination)

**Note:** ATAN2 is slightly slower than ATAN due to additional quadrant determination logic.

---

## Reference Manual

**Section:** §12.11
**Title:** Arc tangent two arguments

---

## See Also

- [ATAN](atan.md) - Arc tangent single argument
- [ASIN](asin.md) - Arc sine
- [ACOS](acos.md) - Arc cosine
- [TAN](tan.md) - Tangent
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
