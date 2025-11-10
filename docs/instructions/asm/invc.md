# INVC - Invert with Carry Add

**Mnemonic:** `invc`  
**Function:** One's complement plus carry  
**Class:** LOGICAL  
**Format:** `Wn INVC`

**Description:** Calculates one's complement of register, adds carry bit. For multi-precision arithmetic.  
Operation: `Rn = ~Rn + C`

**Operands:** 0  
**Variants:** 4 (word only)

**Examples:**

```assembly
% Multi-precision negate (64-bit)
W NEG I1
W2 INVC  % Negate upper word with carry
```

**Reference:** §10.14 Invert with carry add  
**See Also:** [INV](inv.md), [NEG](neg.md), [SUBC](subc.md)
