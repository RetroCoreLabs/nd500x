# SHL - Shift Left

**Mnemonic:** `shl`  
**Function:** Logical left shift  
**Class:** SHIFT  
**Format:** `tn SHL <count>`

**Description:** Shifts register left by count positions. Zeroes shift in from right. Bits shifted out go to carry.

**Examples:**

```assembly
% Multiply by 2
W1 := B.VALUE
W1 SHL 1

% Shift left 8 bits
W2 SHL 8
```

**Reference:** §10.24 Shift left  
**See Also:** [SHR](shr.md), [SHA](sha.md)
