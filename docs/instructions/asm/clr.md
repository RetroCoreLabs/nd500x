# CLR - Clear Register

**Mnemonic:** `clr`  
**Function:** Set register to zero  
**Class:** MOVE  
**Format:** `tn CLR`

**Description:** Sets entire register to all zeroes. For integer types, clears entire 32-bit register.

**Operands:** 0  
**Variants:** 24

**Examples:**

```assembly
% Clear register
W1 CLR

% Initialize counter
W2 CLR

% Reset accumulator
D3 CLR
```

**Reference:** §10.16 Clear register  
**See Also:** [STZ](stz.md), [:=](assignto.md)
