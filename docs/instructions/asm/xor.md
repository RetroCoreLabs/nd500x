# XOR - Bitwise XOR

**Mnemonic:** `xor`  
**Function:** Bitwise exclusive OR  
**Class:** LOGICAL  
**Format:** `tn XOR <operand>`

**Description:** Performs bitwise XOR between register and operand.  
Operation: `Rn = Rn ^ <operand>`

**Operands:** 1  
**Variants:** 16

**Examples:**

```assembly
% Toggle specific bits
W1 XOR 0xFF

% XOR encryption
W2 XOR B.KEY
```

**Reference:** §10.23 Exclusive or  
**See Also:** [OR](or.md), [AND](and.md)
