# ALOG2 - Binary Logarithm (log₂)

## Overview

**Mnemonic:** `alog2`
**Function:** Binary logarithm (base 2)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ALOG2 <argument>`

---

## Description

Calculates the base 2 logarithm of the argument and loads the result into the specified float or double float register.

**Operation:**
```
Fn/Dn = log₂(<argument>)
```

**Key Characteristics:**
- Hardware-accelerated binary logarithm (base 2)
- Argument must be positive (> 0)
- IVO trap on non-positive argument (result = -5.8×10⁷⁶)
- Essential for bit requirements and complexity analysis
- 8 register variants (F1-F4, D1-D4)
- 180-220 cycles (complex transcendental function)
- Common in information theory (bits of information)
- Z flag set when result = 0 (e.g., log₂(1) = 0)
- Result loaded into specified float/double register

The argument must be positive (> 0). Zero or negative values cause an invalid operation (IVO) trap condition and the result is set to -5.8×10⁷⁶ (largest negative floating point number).

The binary logarithm is particularly useful in computer science and information theory for calculating bit requirements, analyzing algorithm complexity (O(log n)), and information content. If y = log₂(x), then x = 2ʸ.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF7C | F | 1 | Float |
| 2/8 | 0xFF7D | F | 2 | Float |
| 3/8 | 0xFF7E | F | 3 | Float |
| 4/8 | 0xFF7F | F | 4 | Float |
| 5/8 | 0xFFA8 | D | 1 | Double Float |
| 6/8 | 0xFFA9 | D | 2 | Double Float |
| 7/8 | 0xFFAA | D | 3 | Double Float |
| 8/8 | 0xFFAB | D | 4 | Double Float |

---

## Operands

### Operand 1 (Argument)

The argument operand containing the value to take the binary logarithm of (must be > 0).

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

- **Z (Zero):** Set if result = 0, cleared otherwise (e.g., log₂(1) = 0)
- **S (Sign):** Set to sign bit of result (bit 31 for float, bit 63 for double)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Binary logarithm from local variable

```assembly
        % Load binary logarithm of local variable RANGE
        F1 ALOG2 B.RANGE
```

### Example 2: Calculate bits required

```assembly
        % Calculate number of bits needed to represent value
        F2 := B.MAX_VALUE
        F2 ALOG2 F2              % log2(value)
        F2 INTR F2               % Round up to integer
        F2 =: B.BITS_NEEDED
```

### Example 3: Power of 2 detection

```assembly
        % Check if value is a power of 2
        D1 ALOG2 B.VALUE
        D1 INT D1                % Truncate to integer
        D1 =: B.EXPONENT
        % If exponent is whole number, value is power of 2
```

### Example 4: Information content calculation

```assembly
        % Calculate information content in bits: I = log2(1/p)
        F1 := 1.0
        F1 / B.PROBABILITY       % 1/p
        F1 ALOG2 F1              % log2(1/p)
        F1 =: B.INFO_BITS
```

---

## Performance Notes

- **Typical cycles:** 180-220 cycles (complex transcendental function)
- **Best case:** ~180 cycles (simple argument values)
- **Worst case:** ~220 cycles (complex argument values, table lookup + polynomial approximation)

**Note:** Logarithmic functions are significantly slower than basic arithmetic operations due to table lookup and polynomial approximation algorithms.

---

## Reference Manual

**Section:** §12.14
**Title:** Binary logarithm

---

## See Also

- [ALOG](alog.md) - Natural logarithm (ln)
- [ALOG10](alog10.md) - Common logarithm (log₁₀)
- [EXP](exp.md) - Exponential (eˣ)
- [SQRT](sqrt.md) - Square root
- [Trap System](../../ND-500-TRAPS.md)
