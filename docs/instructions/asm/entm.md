# ENTM - Enter Main Program

## Overview

**Mnemonic:** `entm`
**Function:** Initialize main program stack and system
**Class:** CALL
**Privilege:** user

**Format:** `ENTM <bottom of stack/r/W>,<stack demand/r/W>,<total system stack demand/r/W>`

---

## Description

Initializes the runtime system for a main program. Sets up the stack base, allocates the main program's stack frame, and reserves total system stack space.

**Operation:**
```
1. Set stack base = <bottom of stack>
2. Allocate <stack demand> words for main locals
3. Reserve <total system stack demand> words total
```

**Key Characteristics:**
- Main program entry point initialization
- Three-operand stack setup (base, demand, total)
- Typically first instruction in program
- Allocates main program stack frame
- Reserves total system stack space
- STO trap on insufficient memory
- Essential for runtime system initialization
- 3-5 cycles execution time
- Sets up both local and total stack allocation

ENTM is typically the first instruction executed when a program starts. It:
1. Establishes the stack base pointer at `<bottom of stack>`
2. Allocates `<stack demand>` words for main program locals
3. Reserves `<total system stack demand>` words for the entire execution

This ensures the runtime has enough stack space for the main program plus all called subroutines.

**Operands:** 3
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD70 | ENTM |

---

## Operands

### Operand 1 (Bottom of Stack)

Base address where stack begins.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 2 (Stack Demand)

Stack space needed for main program locals (words).

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

### Operand 3 (Total System Stack Demand)

Total stack space reserved for entire program (words).

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Stack overflow (STO):** Insufficient memory for requested stack

---

## Examples

### Example 1: Program initialization

```assembly
START:  ENTM STACK_BASE, 100, 4096
        % Main program body
        % ...
        STOP

STACK_BASE:
        % 4096 words reserved for stack
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [ENTS](ents.md) - Simple entry
- [ENTD](entd.md) - Enter with display
- [ENTT](entt.md) - Enter trap handler
