# DECR - Decrement

## Overview

**Mnemonic:** `decr`
**Function:** Decrement by one
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t DECR <operand>`

---

## Description

Decrements the `<operand>` by one. The Carry bit is set appropriately for borrow conditions.

**Operation:**
```
<operand> = <operand> - 1
```

**Key Characteristics:**
- Single-operand decrement (subtract 1)
- 5 data types supported (BY, H, W, F, D)
- Faster than SUB with constant 1
- Essential for loop counters and backwards iteration
- 3-5 cycles execution time
- Sets Z, S, O, C flags (carry on borrow)
- Common in countdown loops and pointer decrement
- Read-modify-write operation
- Destructive (operand overwritten)

**Operands:** 1
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Data Type | Opcode |
|-----------|--------|
| BY | 0x0051 |
| H | 0xFC8C |
| W | 0xFC8D |
| F | 0xFC8E |
| D | 0xFC8F |

---

## Operands

### Operand 1 (Destination)

The operand to decrement.

**Type:** Byte, Halfword, or Word
**Access:** Read/Write

**Supported modes:**
- **LOCAL**, **RECORD**, **REGISTER**, **PRE_INDEXED**, **ABSOLUTE**

---

## Trap Conditions

- **Addressing traps**, **Integer overflow (O)**

---

## Data Status Bits

- **Z, S, O, C**

---

## Examples

### Example 1: Decrement counter

```assembly
        % Decrement loop counter
        W DECR B.COUNT
```

### Example 2: Countdown loop

```assembly
        W1 := B.SIZE
LOOP:
        % Process element
        W DECR I1
        IF>=GO LOOP
```

### Example 3: Decrement pointer

```assembly
        % Move backwards through array
        W DECR B.PTR
```

---

## Performance Notes

- **Typical:** 3-5 cycles
- **Faster than:** SUB with constant 1

---

## Reference Manual

**Section:** §10.20
**Title:** Decrement

---

## See Also

- [INCR](incr.md), [SUB2](sub2.md)
