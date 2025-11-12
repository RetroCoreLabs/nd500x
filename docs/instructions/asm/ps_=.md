# PS:= - Load Process segment

## Overview

**Mnemonic:** `ps=`
**Function:** Load Process segment
**Class:** MOVE
**Privilege:** user/supervisor

**Format:** `PS:= <operand>`

---

## Description

Loads the PS (Process Segment) register from the specified operand. The PS register points to the current process control block (PCB), which contains process state and control information. Used for process switching and system initialization.

**Operation:**
```
<operand> → PS
```

**Key Characteristics:**
- Loads process control block pointer
- Critical for process switching
- Points to PCB in physical memory
- System-level register
- Used by operating system

**Common Use Cases:**
- Process context switching
- OS initialization
- Process creation/termination
- Domain switching
- System call handling

**Operands:** 1 (source)
**Variants:** 1

---

## Variants

| Variant | Opcode | Description |
|---------|--------|-------------|
| 1/1 | (varies) | Load Process segment |

---

## Operands

### Operand 1 (Source)

Value to load into Process segment.

---

## Trap Conditions

- **Addressing traps:** Invalid address
- **Privilege violation:** If supervisor-only

---

## Data Status Bits

Unaffected.

---

## Examples

### Example 1: Load from local variable

```assembly
        PS:= B.SAVED_VALUE
```

### Example 2: Load from register

```assembly
        PS:= I1
```

---

## Performance Notes

- **Cycles:** 3-4

---

## Reference Manual

**Section:** §16.7
**Title:** Load special register

---

## See Also

- [PS=:](ps=_.md) - Store Process segment
- [Context Switching](../ContextSwitching.md)
