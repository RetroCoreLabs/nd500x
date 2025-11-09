# POLY - Polynomial Evaluation

**Mnemonic:** `poly`  
**Function:** Evaluate polynomial  
**Class:** MATH  
**Format:** `Fn POLY <coeffs>,<degree>`

**Description:** Evaluates polynomial using Horner's method.

**Examples:**

```assembly
% Evaluate polynomial
F1 := B.X
F1 POLY B.COEFFS, 3
```

**Reference:** §12.3 Polynomial  
**See Also:** [EXP](exp.md), [SQRT](sqrt.md)
