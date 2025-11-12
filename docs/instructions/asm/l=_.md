# L=: - Store Link register

## Overview

**Mnemonic:** `l=`
**Function:** Store Link register
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `L=: <operand>`

---

## Description

Stores the Link register (return address) to the specified operand. Used for saving return addresses during context switches, debugging, or non-standard call patterns.

**Operation:**
```
L → <operand>
```

**Key Characteristics:**
- Stores return address from Link register
- Used in context switching and debugging
- Enables non-standard call patterns
- Required for coroutine implementations
- Part of system register access suite

**Common Use Cases:**
- Context switch save/restore
- Debugger breakpoint handling
- Return address inspection
- Coroutine implementation
- Manual call stack management

**Operands:** 1 (destination)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Store Link register |

---

## Operands

### Operand 1 (Destination)

Where to store Link register value.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Store to local variable

```assembly
        L=: B.SAVE_AREA
```

### Example 2: Store to register

```assembly
        L=: I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.8
**Title:** Store special register

---

## See Also

- [L:=](l_=.md) - Load Link register
- [Context Switching](../ContextSwitching.md)
