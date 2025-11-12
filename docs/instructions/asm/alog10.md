# ALOG10 - Common Logarithm (log₁₀)

## Overview

**Mnemonic:** `alog10`
**Function:** Common logarithm (base 10)
**Class:** FLOAT_MATH
**Privilege:** user

**Format:** `tn ALOG10 <argument>`

---

## Description

Calculates the base 10 logarithm (common logarithm) of the argument and loads the result into the specified float or double float register.

**Operation:**
```
Fn/Dn = log₁₀(<argument>)
```

**Key Characteristics:**
- Hardware-accelerated common logarithm (base 10)
- Argument must be positive (> 0)
- IVO trap on non-positive argument (result = -5.8×10⁷⁶)
- Essential for pH, decibels, Richter scale
- 8 register variants (F1-F4, D1-D4)
- 180-220 cycles (complex transcendental function)
- Common in scientific and engineering calculations
- Z flag set when result = 0 (e.g., log₁₀(1) = 0)
- Result loaded into specified float/double register

The argument must be positive (> 0). Zero or negative values cause an invalid operation (IVO) trap condition and the result is set to -5.8×10⁷⁶ (largest negative floating point number).

The common logarithm is widely used in science and engineering for pH calculations, decibel measurements, Richter scale (earthquakes), sound intensity, and scientific notation. If y = log₁₀(x), then x = 10ʸ.

**Operands:** 1
**Variants:** 8 opcode(s)

---

## Variants

Total variants: 8 (2 data types × 4 registers)

| Variant | Opcode | Prefix | Register | Data Type |
|---------|--------|--------|----------|-----------|
| 1/8 | 0xFF80 | F | 1 | Float |
| 2/8 | 0xFF81 | F | 2 | Float |
| 3/8 | 0xFF82 | F | 3 | Float |
| 4/8 | 0xFF83 | F | 4 | Float |
| 5/8 | 0xFFAC | D | 1 | Double Float |
| 6/8 | 0xFFAD | D | 2 | Double Float |
| 7/8 | 0xFFAE | D | 3 | Double Float |
| 8/8 | 0xFFAF | D | 4 | Double Float |

---

## Operands

### Operand 1 (Argument)

The argument operand containing the value to take the common logarithm of (must be > 0).

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

- **Z (Zero):** Set if result = 0, cleared otherwise (e.g., log₁₀(1) = 0)
- **S (Sign):** Set to sign bit of result (bit 31 for float, bit 63 for double)
- **O (Overflow):** Unaffected
- **K (Flag):** Unaffected
- **C (Carry):** Unaffected

---

## Examples

### Example 1: Common logarithm from variable

```assembly
        % Load common logarithm of BIGNUMB
        F4 ALOG10 BIGNUMB
```

### Example 2: pH calculation

```assembly
        % Calculate pH = -log10([H+])
        F1 ALOG10 B.H_CONCENTRATION
        F1 NEG                   % Negate to get pH
        F1 =: B.PH_VALUE
```

### Example 3: Decibel calculation

```assembly
        % Calculate dB = 10 * log10(P/P0)
        F2 := B.POWER
        F2 / B.REFERENCE_POWER   % P/P0
        F2 ALOG10 F2             % log10(P/P0)
        F2 * 10.0                % 10 * log10(P/P0)
        F2 =: B.DECIBELS
```

### Example 4: Order of magnitude

```assembly
        % Find order of magnitude of a number
        D1 ALOG10 B.VALUE
        D1 INT D1                % Truncate to integer
        D1 =: B.ORDER_OF_MAG
        % e.g., 1234 -> log10(1234) = 3.09... -> 3
```

---

## Performance Notes

- **Typical cycles:** 180-220 cycles (complex transcendental function)
- **Best case:** ~180 cycles (simple argument values)
- **Worst case:** ~220 cycles (complex argument values, table lookup + polynomial approximation)

**Note:** Logarithmic functions are significantly slower than basic arithmetic operations due to table lookup and polynomial approximation algorithms.

---

## Reference Manual

**Section:** §12.15
**Title:** Common logarithm

---

## See Also

- [ALOG](alog.md) - Natural logarithm (ln)
- [ALOG2](alog2.md) - Binary logarithm (log₂)
- [EXP](exp.md) - Exponential (eˣ)
- [SQRT](sqrt.md) - Square root
- [Addressing Modes](../AddressingModes.md)
- [Trap System](../ND500_TRAP_SYSTEM_COMPREHENSIVE.md)
