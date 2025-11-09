# OR - Bitwise OR

**Mnemonic:** `or`  
**Function:** Bitwise OR operation  
**Class:** LOGICAL  
**Format:** `tn OR <operand>`

**Description:** Performs bitwise OR between register and operand, storing result in register.  
Operation: `Rn = Rn | <operand>`

**Operands:** 1  
**Variants:** 16 (4 data types × 4 registers)

**Trap Conditions:** Addressing traps

**Data Status Bits:** Z (zero), S (sign)

**Examples:**

```assembly
% Set specific bits in flags register
W1 OR 0x0010

% Combine bit masks
W2 OR B.MASK
```

**Reference:** §10.22 Or  
**See Also:** [AND](and.md), [XOR](xor.md), [INV](inv.md)
