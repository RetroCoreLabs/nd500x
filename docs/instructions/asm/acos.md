# ACOS - Arc Cosine

## Overview

**Mnemonic:** `acos`
**Function:** Arc cosine (inverse cosine)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ACOS <argument>`

---

## Description

Calculates the trigonometric arccosine (inverse cosine) of the argument and loads the result into the specified float or double float register. The result value gives the angle in radians in the range 0 to π (pi).

**Key Characteristics:**
- Hardware-accelerated inverse cosine computation
- Restricted domain: argument must be [-1, +1]
- Bounded range: result in [0, π] radians (always positive)
- Traps on out-of-range arguments (IVO trap)
- Essential for angle recovery and dot product inversions
- Supports float (F) and double (D) precision
- 8 register variants (F1-F4, D1-D4) for flexible allocation
- Slower than forward trigonometric functions (150-200 cycles)

The argument must be in the range -1 to +1. If the argument is outside this range, an invalid operation (IVO) trap condition will occur and the specified register is set to zero.

This instruction is commonly used to find an angle when the cosine value is known, such as in vector mathematics, 3D graphics, or solving triangulation problems.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF64 | F | 1 | Float |
| 2/8 | 0xFF65 | F | 2 | Float |
| 3/8 | 0xFF66 | F | 3 | Float |
| 4/8 | 0xFF67 | F | 4 | Float |
| 5/8 | 0xFF90 | D | 1 | Double Float |
| 6/8 | 0xFF91 | D | 2 | Double Float |
| 7/8 | 0xFF92 | D | 3 | Double Float |
| 8/8 | 0xFF93 | D | 4 | Double Float |

---

## Operands

### Operand 1 (Argument)

The argument operand containing the cosine value (must be in range [-1, +1]).

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

### Example 1: Arc cosine from record field

```assembly
        % Load arc cosine of field FOO in record
        F4 ACOS R.FOO
```

### Example 2: Arc cosine from local variable

```assembly
        % Calculate angle from cosine value
        D1 ACOS B.COS_VALUE
        D1 =: B.ANGLE_RADIANS
```

### Example 3: Arc cosine with range validation

```assembly
        % Calculate arc cosine with trap handling
        F2 ACOS B.INPUT
        IF-KGO INVALID_INPUT     % Trap if argument out of range
        F2 =: B.RESULT
        GO DONE

INVALID_INPUT:
        % Handle invalid argument (|arg| > 1)
        F2 := 0.0

DONE:
```

### Example 4: Convert to degrees

```assembly
        % Calculate arc cosine and convert to degrees
        F1 ACOS B.COS_VAL        % Result in radians
        F1 * 180.0               % Convert to degrees
        F1 / 3.14159265          % Divide by pi
        F1 =: B.ANGLE_DEGREES
```

---

## Performance Notes

- **Typical cycles:** 150-200 cycles (complex transcendental function)
- **Best case:** ~150 cycles (simple argument values)
- **Worst case:** ~200 cycles (complex argument values, table lookup + polynomial approximation)

**Note:** Transcendental functions like ACOS are significantly slower than basic arithmetic operations due to polynomial approximation or table lookup algorithms.

---

## Reference Manual

**Section:** §12.8
**Title:** Arc cosine

---

## See Also

- [ASIN](asin.md) - Arc sine
- [ATAN](atan.md) - Arc tangent
- [ATAN2](atan2.md) - Arc tangent two arguments
- [COS](cos.md) - Cosine
- [SIN](sin.md) - Sine
- [Trap System](../../ND-500-TRAPS.md)
