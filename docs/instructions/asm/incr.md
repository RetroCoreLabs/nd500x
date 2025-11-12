# INCR - Increment

## Overview

**Mnemonic:** `incr`
**Function:** Increment by one
**Class:** ARITHMETIC
**Privilege:** user

**Format:** `t INCR <operand>`

---

## Description

Increments the `<operand>` by one. The Carry bit is set if a carry occurs from the sign bit position, otherwise reset. Carry occurs when and only when integer -1 is incremented.

**Operation:**
```
<operand> = <operand> + 1
```

**Key Characteristics:**
- Single-operand increment (add 1)
- 5 data types supported (BY, H, W, F, D)
- Faster than ADD with constant 1
- Essential for loop counters and forward iteration
- 3-5 cycles execution time
- Sets Z, S, O, C flags (carry when -1 incremented)
- Most common loop increment operation
- Read-modify-write operation
- Destructive (operand overwritten)

**Operands:** 1
**Variants:** 5 opcode(s)

---

## Variants

Total variants: 5

| Data Type | Opcode |
|-----------|--------|
| BY | 0x004E |
| H | 0x004F |
| W | 0x0050 |
| F | 0xFC8A |
| D | 0xFC8B |

---

## Operands

### Operand 1 (Destination)

The operand to increment.

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

### Example 1: Increment counter

```assembly
        % Increment loop counter
        W INCR B.COUNT
```

### Example 2: Increment array index

```assembly
        % Move to next element
        W INCR I2
```

### Example 3: Increment in loop

```assembly
LOOP:
        % Process element
        W INCR B.INDEX
        W COMP B.INDEX, B.SIZE
        IF<GO LOOP
```

---

## Performance Notes

- **Typical:** 3-5 cycles
- **Faster than:** ADD with constant 1

---

## Reference Manual

**Section:** §10.19
**Title:** Increment

---

## See Also

- [DECR](decr.md), [ADD2](add2.md)
