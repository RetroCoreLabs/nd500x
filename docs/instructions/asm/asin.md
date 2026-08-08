# ASIN - Arc Sine

## Overview

**Mnemonic:** `asin`
**Function:** Arc sine (inverse sine)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ASIN <argument>`

---

## Description

Calculates the trigonometric arcsine (inverse sine) of the argument and loads the result into the specified float or double float register. The result value gives the angle in radians in the range -π/2 to +π/2 (-1.5708 to +1.5708).

**Key Characteristics:**
- Hardware-accelerated inverse sine computation
- Restricted domain: argument must be [-1, +1]
- Bounded range: result in [-π/2, +π/2] radians
- Traps on out-of-range arguments (IVO trap)
- Essential for angle recovery from sine values
- Supports float (F) and double (D) precision
- 8 register variants (F1-F4, D1-D4) for flexible allocation
- Slower than forward trigonometric functions (150-200 cycles)

The argument must be in the range -1 to +1. If the argument is outside this range, an invalid operation (IVO) trap condition will occur and the specified register is set to zero.

This instruction is commonly used to find an angle when the sine value is known, such as in solving right triangles, wave analysis, or physics calculations involving projectile motion.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF5C | F | 1 | Float |
| 2/8 | 0xFF5D | F | 2 | Float |
| 3/8 | 0xFF5E | F | 3 | Float |
| 4/8 | 0xFF5F | F | 4 | Float |
| 5/8 | 0xFF88 | D | 1 | Double Float |
| 6/8 | 0xFF89 | D | 2 | Double Float |
| 7/8 | 0xFF8A | D | 3 | Double Float |
| 8/8 | 0xFF8B | D | 4 | Double Float |

---

## Operands

### Operand 1 (Argument)

The argument operand containing the sine value (must be in range [-1, +1]).

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
- **Invalid operation (IVO):** Argument outside range [-1, +1]; result register set to zero

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise
- **S (Sign):** Set to sign bit of result (bit 31 for float, bit 63 for double)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: In-place arc sine

```assembly
        % Replace number in F2 with its arcsine
        F2 ASIN F2
```

### Example 2: Arc sine from local variable

```assembly
        % Calculate angle from sine value
        F1 ASIN B.SINE_VALUE
        F1 =: B.ANGLE_RADIANS
```

### Example 3: Arc sine with validation

```assembly
        % Calculate arc sine with error handling
        D2 ASIN B.INPUT
        IF-KGO ERROR_HANDLER     % Trap if |input| > 1
        D2 =: B.RESULT
        GO CONTINUE

ERROR_HANDLER:
        % Handle invalid argument
        D2 := 0.0

CONTINUE:
```

### Example 4: Solve for launch angle

```assembly
        % Calculate launch angle from range and velocity
        % sin(θ) = (g * range) / (velocity²)
        F1 := B.GRAVITY
        F1 * B.RANGE
        F2 := B.VELOCITY
        F2 * F2                  % velocity²
        F1 / F2                  % sin(θ)
        F1 ASIN F1               % θ in radians
        F1 =: B.LAUNCH_ANGLE
```

---

## Performance Notes

- **Typical cycles:** 150-200 cycles (complex transcendental function)
- **Best case:** ~150 cycles (simple argument values)
- **Worst case:** ~200 cycles (complex argument values, table lookup + polynomial approximation)

**Note:** Transcendental functions like ASIN are significantly slower than basic arithmetic operations due to polynomial approximation or table lookup algorithms.

---

## Reference Manual

**Section:** §12.7
**Title:** Arc sine

---

## See Also

- [ACOS](acos.md) - Arc cosine
- [ATAN](atan.md) - Arc tangent
- [ATAN2](atan2.md) - Arc tangent two arguments
- [SIN](sin.md) - Sine
- [COS](cos.md) - Cosine
- [Trap System](../../ND-500-TRAPS.md)
