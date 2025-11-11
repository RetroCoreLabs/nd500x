# IF=GO - Conditional Jump If Equal

## Overview

**Mnemonic:** `if=go`
**Function:** Conditional jump if zero flag set (equal)
**Class:** BRANCH
**Privilege:** user

**Format:** `IF = GO <<displacement>>`

---

## Description

Transfers control if Z=1 (zero flag set), indicating the last operation resulted in zero or that compared values were equal. The sign-extended displacement is added to PC when the condition is true. Most commonly used after COMP or TEST instructions.

**Operation:**
```
if Z=1 then PC += displacement
```

**Key Characteristics:**
- Tests Zero flag (Z=1)
- Sign-extended displacement
- Two variants: byte (-128 to +127) and halfword (-32768 to +32767)
- Follows comparison/test operations
- Branch prediction available on some implementations

**Common Use Cases:**
- Equality testing after COMP
- Loop termination on zero
- Null pointer checks
- Error code checking
- Switch/case implementations

**Operands:** 1 (signed displacement)
**Variants:** 2 opcodes (byte and halfword displacement)

---

## Variants

| Variant | Opcode | Displacement |
|---------|--------|--------------|
| 1/2 | 0x00C4 | Byte |
| 2/2 | 0x00C5 | Halfword |

---

## Operands

### Operand 1 (Displacement)

Signed displacement (-128 to +127 byte, -32768 to +32767 halfword).

---

## Trap Conditions

- **Branch trap (BT):** If enabled
- **Addressing traps:** Invalid displacement

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: Compare and branch

```assembly
        W COMP B.A, B.B
        IF = GO EQUAL
```

### Example 2: Test and branch

```assembly
        W TEST B.COUNTER
        IF = GO ZERO
```

### Example 3: Loop exit

```assembly
LOOP:
        W COMP B.ARRAY(I1), B.TARGET
        IF = GO FOUND
        W ADD2 I1, 4
        GO LOOP
FOUND:
```

---

## Performance Notes

- **Cycles:** 2-3 (not taken), 3-4 (taken)

---

## Reference Manual

**Section:** §13.3
**Title:** Conditional Jump

---

## See Also

- [IF><GO](if><go.md) - Jump if not equal
- [COMP](comp.md) - Compare
- [TEST](test.md) - Test
- [GO](go.md) - Unconditional jump
