# SWAP - Swap Two Operands

**Mnemonic:** `swap`  
**Function:** Exchange contents of two operands  
**Class:** MOVE  
**Format:** `t SWAP <op1>,<op2>`

**Description:** Exchanges contents of first and second operands. Both operands must be same data type.

**Operands:** 2  
**Variants:** 6

**Examples:**

```assembly
% Swap two variables
W SWAP B.A, B.B

% Swap register with memory
W SWAP I1, B.TEMP

% Exchange record fields
W SWAP R.X, R.Y
```

**Reference:** §10.8 Swap  
**See Also:** [MOVE](move.md)
