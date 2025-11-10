# MULAD - Multiply and Add

**Mnemonic:** `mulad`  
**Function:** Fused multiply-add  
**Class:** ARITHMETIC  
**Format:** `tn MULAD <x>,<y>`

**Description:** Multiplies register by x, adds y. Operation: `Rn = Rn * x + y`

**Examples:**

```assembly
% Fused multiply-add
W1 := B.BASE
H1 MULAD 60:B, B.MINUTES
```

**Reference:** §11.19 Multiply and add  
**See Also:** [PSUM](psum.md), [MUL3](mul3.md)
