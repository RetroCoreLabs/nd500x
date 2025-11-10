# INV - Bitwise Invert

**Mnemonic:** `inv`  
**Function:** One's complement  
**Class:** LOGICAL  
**Format:** `tn INV`

**Description:** Calculates one's complement of register contents. For BI/BY/H, clears upper register bits.

**Operands:** 0  
**Variants:** 16

**Examples:**

```assembly
% Invert all bits in register
W1 INV

% Create bit mask
W2 := 0x00FF
W2 INV  % Now contains 0xFFFFFF00
```

**Reference:** §10.13 Invert  
**See Also:** [INVC](invc.md), [NEG](neg.md)
