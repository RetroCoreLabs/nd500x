# ALOG - Natural Logarithm (ln)

## Overview

**Mnemonic:** `alog`
**Function:** Natural logarithm (base e)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ALOG <argument>`

---

## Description

Calculates the natural logarithm (base e = 2.718281828459045...) of the argument and loads the result into the specified float or double float register.

**Operation:**
```
Fn/Dn = ln(<argument>)
```

**Key Characteristics:**
- Hardware-accelerated natural logarithm (base e)
- Argument must be positive (> 0)
- IVO trap on non-positive argument (result = -5.8×10⁷⁶)
- Inverse of exponential function (ln(eˣ) = x)
- 8 register variants (F1-F4, D1-D4)
- 180-220 cycles (complex transcendental function)
- Essential for entropy, growth/decay, statistics
- Z flag set when result = 0 (e.g., ln(1) = 0)
- Result loaded into specified float/double register

The argument must be positive (> 0). Zero or negative values cause an invalid operation (IVO) trap condition and the result is set to -5.8×10⁷⁶ (largest negative floating point number).

The natural logarithm is the inverse of the exponential function: if y = ln(x), then x = eʸ. It's commonly used in mathematics, physics, engineering, and data analysis for growth/decay calculations, entropy, information theory, and statistical distributions.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF78 | F | 1 | Float |
| 2/8 | 0xFF79 | F | 2 | Float |
| 3/8 | 0xFF7A | F | 3 | Float |
| 4/8 | 0xFF7B | F | 4 | Float |
| 5/8 | 0xFFA4 | D | 1 | Double Float |
| 6/8 | 0xFFA5 | D | 2 | Double Float |
| 7/8 | 0xFFA6 | D | 3 | Double Float |
| 8/8 | 0xFFA7 | D | 4 | Double Float |

---

## Operands

### Operand 1 (Argument)

The argument operand containing the value to take the natural logarithm of (must be > 0).

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
- **Invalid operation (IVO):** Argument ≤ 0; result set to -5.8×10⁷⁶

---

## Data Status Bits

- **Z (Zero):** Set if result = 0, cleared otherwise (e.g., ln(1) = 0)
- **S (Sign):** Set to sign bit of result (bit 31 for float, bit 63 for double)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Natural logarithm from array

```assembly
        % Load natural logarithm of R1th element of array COEFF
        D1 ALOG COEFF(R1)
```

### Example 2: Natural logarithm from local variable

```assembly
        % Calculate ln(x)
        F2 ALOG B.VALUE
        F2 =: B.LN_RESULT
```

### Example 3: Natural logarithm with validation

```assembly
        % Calculate ln with error handling
        F1 := B.INPUT
        F1 COMP 0.0              % Check if positive
        IF<=GO ERROR_NEGATIVE
        F1 ALOG B.INPUT
        F1 =: B.RESULT
        GO DONE

ERROR_NEGATIVE:
        % Handle non-positive input
        F1 := 0.0

DONE:
```

### Example 4: Calculate entropy

```assembly
        % Calculate Shannon entropy: H = -p * ln(p)
        F1 := B.PROBABILITY
        F2 ALOG B.PROBABILITY    % ln(p)
        F1 * F2                  % p * ln(p)
        F1 NEG                   % -p * ln(p)
        F1 =: B.ENTROPY
```

---

## Performance Notes

- **Typical cycles:** 180-220 cycles (complex transcendental function)
- **Best case:** ~180 cycles (simple argument values)
- **Worst case:** ~220 cycles (complex argument values, table lookup + polynomial approximation)

**Note:** Logarithmic functions are significantly slower than basic arithmetic operations due to table lookup and polynomial approximation algorithms.

---

## Reference Manual

**Section:** §12.13
**Title:** Natural logarithm

---

## See Also

- [EXP](exp.md) - Exponential (eˣ) - inverse of ALOG
- [ALOG2](alog2.md) - Binary logarithm (log₂)
- [ALOG10](alog10.md) - Common logarithm (log₁₀)
- [SQRT](sqrt.md) - Square root
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
