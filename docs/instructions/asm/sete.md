# SETE - Set Bit in Trap Enable Register

## Overview

**Mnemonic:** `sete`
**Function:** Enable trap by setting OTE register bit
**Class:** CONTROL
**Privilege:** user

**Format:** `SETE <bit no>`

---

## Description

Sets the specified bit in the Own Trap Enable (OTE) register to enable trap handling for that trap condition. The bit number operand is compared with a modify mask (TEMM) found in the domain description table. Only bits set in TEMM are modifiable; attempting to modify a non-modifiable bit causes an illegal operand value trap.

**Operation:**
```
Set bit <bit no> in OTE register
(subject to TEMM mask permissions)
```

**Key Characteristics:**
- Modifies Own Trap Enable register
- Bit must be modifiable per TEMM mask
- Domain-level trap control
- Enables specific trap conditions
- No effect on data status bits
- User-mode trap management

**Common Use Cases:**
- Enable overflow trap handling
- Enable floating-point exception traps
- Enable debugging traps
- Configure error detection
- Dynamic trap configuration

**Operands:** 1 (bit number)
**Variants:** 1 opcode

---

## Examples

### Example 1: Enable overflow trap

```assembly
        % Enable integer overflow trap (bit 9)
        SETE 9
        % Overflows now generate traps
```

**Explanation:** Enable overflow detection for integer arithmetic.

### Example 2: Enable multiple traps

```assembly
        % Enable several trap conditions
        SETE 9                 % Integer overflow
        SETE 11                % Floating overflow
        SETE 12                % Floating underflow
```

**Explanation:** Configure multiple trap handlers.

### Example 3: Dynamic trap configuration

```assembly
        % Enable trap based on variable
        W1 MOVE DEBUG_MODE
        IF<>GO SKIP_TRAP
        SETE 17                % Single instruction trap
SKIP_TRAP:
```

**Explanation:** Conditionally enable debugging trap.

### Example 4: Array-based configuration

```assembly
        % Enable traps from configuration table
        W1 CLR
LOOP:   W2 MOVE TRAP_CONFIG(W1)
        SETE W2
        W1 ADD 1
        W1 COMP CONFIG_COUNT
        IF<GO LOOP
```

**Explanation:** Enable multiple traps from configuration array.

### Example 5: Error detection setup

```assembly
        % Initialize error trap handling
        SETE 9                 % Overflow
        SETE 23                % Invalid operation
        % Setup complete, proceed with operations
```

**Explanation:** Enable error detection for critical section.

---

## Common Trap Bit Numbers

| Bit | Trap Condition | Description |
|-----|----------------|-------------|
| 9 | Integer Overflow | Arithmetic overflow (O) |
| 11 | Float Overflow | Floating overflow (FO) |
| 12 | Float Underflow | Floating underflow (FU) |
| 17 | Single Instruction | Debug trap after each instruction |
| 23 | Invalid Operation | Invalid BCD/operation (IVO) |

---

## Trap Conditions

- **Addressing traps**: Invalid operand address
- **Illegal operand value (IOV)**: Bit not modifiable per TEMM mask

---

## Data Status Bits

- **Unaffected**: Z, S, C, V remain unchanged

---

## Reference Manual

**Section:** §16.5
**Title:** Set bit in trap enable register

---

## See Also

- [CLTE](clte.md) - Clear bit in trap enable register
- [ROTE](rote.md) - Read own trap enable register
- [SETK](setk.md) - Set K flag
- [CLRK](clrk.md) - Clear K flag
