# CLTE - Clear Bit in Trap Enable Register

## Overview

**Mnemonic:** `clte`
**Function:** Clear bit in trap enable register
**Class:** CONTROL
**Privilege:** user
**Format:** `CLTE <bit no>`

---

## Description

Clears a specified bit in the Own Trap Enable (OTE) register. This instruction controls which trap conditions are handled locally versus propagated to the mother domain. When a bit is cleared, an ignorable trap will be ignored and no local trap handler is invoked unless the corresponding Mother Trap Enable (MTE) bit is set. Non-ignorable traps are always propagated to the mother domain.

The bit number operand is compared against a trap enable modify mask (TEMM) stored in the domain description table. Only bits with their corresponding TEMM bit set can be modified. Attempting to modify a non-modifiable bit causes an Illegal Operand Value trap.

**Common Use Cases:**
- Disabling specific trap handlers temporarily
- Selective trap masking for critical sections
- Trap handler configuration during initialization
- Dynamic trap control in multi-domain systems

**Operands:** 1
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFD3A | CLTE |

---

## Operands

**Operand 1** (Bit Number, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Byte
- **Role**: Bit number to clear in OTE register

**Result**: Specified bit cleared in Own Trap Enable register

---

## Trap Conditions
- **Addressing traps**: Invalid address or protection violation
- **Illegal operand value (IOV)**: Bit not modifiable per TEMM mask

---

## Data Status Bits
- **Z,S,C,V**: Unaffected

---

## Examples

### Example 1: Disable single instruction trap
```assembly
CLTE 17
```

### Example 2: Clear trap using register
```assembly
BY MOVE 5, I1
CLTE I1
```

### Example 3: Clear multiple traps
```assembly
CLTE 10
CLTE 11
CLTE 12
```

### Example 4: Clear from local variable
```assembly
CLTE B.16
```

### Example 5: Conditional trap disable
```assembly
% Disable overflow trap in critical section
CLTE 0
% ... critical operations ...
SETE 0  % Re-enable
```

### Example 6: Array-based trap control
```assembly
% Clear trap bits from array
W1 CLR
LOOP:
    CLTE TRAP_BITS(W1)
    W1 INC
    W1 COMP 8
    IF<GO LOOP
```

### Example 7: Clear trap with indexed access
```assembly
W2 MOVE 3, I2
CLTE TRAP_CONFIG(W2)
```

---

## Performance Notes
- Execution: 2-3 cycles
- Single-cycle register modification
- TEMM check adds validation overhead

---

## Reference Manual
**Section:** §16.6
**Title:** Clear bit in trap enable register

---

## See Also
- [SETE](sete.md) - Set bit in trap enable register
- [CLRK](clrk.md) - Clear K flag
- [SETK](setk.md) - Set K flag
