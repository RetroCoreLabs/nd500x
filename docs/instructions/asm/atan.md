# ATAN - Arc Tangent

## Overview

**Mnemonic:** `atan`
**Function:** Arc tangent (inverse tangent)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ATAN <argument>`

---

## Description

Calculates the trigonometric arctangent (inverse tangent) of the argument and loads the result into the specified float or double float register. The result value gives the angle in radians in the range -π/2 to +π/2 (-1.5708 to +1.5708).

**Key Characteristics:**
- Hardware-accelerated inverse tangent computation
- Unrestricted domain: accepts any value (no range trap)
- Bounded range: result in [-π/2, +π/2] radians
- Cannot determine quadrant (use ATAN2 for full 2π range)
- Essential for slope-to-angle conversions
- Supports float (F) and double (D) precision
- 8 register variants (F1-F4, D1-D4) for flexible allocation
- Slower than forward trigonometric functions (150-200 cycles)

Unlike ATAN2, this single-argument version cannot determine the quadrant of the angle, as it only receives the ratio (opposite/adjacent) without knowing the signs of the individual components. For full quadrant information, use ATAN2.

This instruction is commonly used in coordinate conversions, angle calculations, and solving mathematical problems involving slopes and tangent ratios.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF6C | F | 1 | Float |
| 2/8 | 0xFF6D | F | 2 | Float |
| 3/8 | 0xFF6E | F | 3 | Float |
| 4/8 | 0xFF6F | F | 4 | Float |
| 5/8 | 0xFF98 | D | 1 | Double Float |
| 6/8 | 0xFF99 | D | 2 | Double Float |
| 7/8 | 0xFF9A | D | 3 | Double Float |
| 8/8 | 0xFF9B | D | 4 | Double Float |

---

## Operands

### Operand 1 (Argument)

The argument operand containing the tangent value (ratio).

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

**Note:** Unlike ASIN/ACOS, ATAN does not have range restrictions since tangent can be any value.

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of result (bit 31 for float, bit 63 for double)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Arc tangent from variable

```assembly
        % Load arc tangent of RAY
        F4 ATAN RAY
```

### Example 2: Calculate angle from slope

```assembly
        % Calculate angle from slope (rise/run)
        F1 := B.RISE
        F1 / B.RUN               % slope = rise/run
        F1 ATAN F1               % angle in radians
        F1 =: B.ANGLE
```

### Example 3: In-place arc tangent

```assembly
        % Replace ratio with angle
        D2 := B.RATIO
        D2 ATAN D2
        D2 =: B.ANGLE_RAD
```

### Example 4: Convert slope to degrees

```assembly
        % Calculate angle in degrees from slope
        F3 ATAN B.SLOPE          % Get angle in radians
        F3 * 180.0               % Convert to degrees
        F3 / 3.14159265          % Divide by pi
        F3 =: B.ANGLE_DEG
```

---

## Performance Notes

- **Typical cycles:** 150-200 cycles (complex transcendental function)
- **Best case:** ~150 cycles (simple argument values)
- **Worst case:** ~200 cycles (complex argument values, table lookup + polynomial approximation)

**Note:** Transcendental functions like ATAN are significantly slower than basic arithmetic operations due to polynomial approximation or table lookup algorithms.

---

## Reference Manual

**Section:** §12.10
**Title:** Arc tangent

---

## See Also

- [ATAN2](atan2.md) - Arc tangent two arguments (full quadrant)
- [ASIN](asin.md) - Arc sine
- [ACOS](acos.md) - Arc cosine
- [TAN](tan.md) - Tangent
- [Trap System](../../ND-500-TRAPS.md)
