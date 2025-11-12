# ENTS - Simple Subroutine Entry

## Overview

**Mnemonic:** `ents`
**Function:** Enter simple subroutine with stack allocation
**Class:** CALL
**Privilege:** user

**Format:** `ENTS <stack demand/r/W>`

---

## Description

Enters a simple subroutine by allocating the specified amount of stack space for local variables. This is the most common subroutine entry mechanism.

**Operation:**
```
1. Save return address on stack
2. Save previous frame pointer
3. Allocate <stack demand> words for locals
4. Set up new frame pointer
```

**Key Characteristics:**
- Most common subroutine entry mechanism
- Single operand (stack demand in words)
- Standard C-style function prologue
- Allocates local variable space on stack
- STO trap on insufficient stack
- Paired with RET for function exit
- 3-5 cycles execution time
- No nested scope support (see ENTD for that)
- Essential for standard function calls

ENTS:
1. Saves return address on stack
2. Saves previous frame pointer
3. Allocates `<stack demand>` words for local variables
4. Sets up new frame pointer

This is used for standard C-style functions without nested scopes or special calling conventions.

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD74 | ENTS |

---

## Operands

### Operand 1 (Stack Demand)

Number of words to allocate for local variables.

**Type:** Word
**Access:** Read

**Supported modes:** LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE

---

## Trap Conditions

- **Addressing traps:** Invalid address, page fault, protection violation
- **Stack overflow (STO):** Insufficient stack space

---

## Examples

### Example 1: Simple function

```assembly
ADD_FUNC:
        ENTS 4              % Allocate 4 words for locals
        W1 := B.ARG1
        W2 := B.ARG2
        W ADD I1, I2, B.RESULT
        RET                 % Return to caller
```

### Example 2: Function with local variables

```assembly
PROCESS:
        ENTS 20             % Allocate 20 words
        % B.0-B.19 are local variables
        W MOVE 0, B.COUNTER
        W MOVE 0, B.SUM
        % Function body
        RET
```

### Example 3: Leaf function (minimal frame)

```assembly
GETMAX:
        ENTS 0              % No locals needed
        W1 := B.A
        W2 := B.B
        W COMP I1, I2
        IF>GO RETURN_A
        W1 := I2
RETURN_A:
        RET
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [RET](ret.md) - Simple return
- [ENTSN](entsn.md) - Simple entry with argument count
- [ENTD](entd.md) - Enter with display
- [ENTF](entf.md) - Fortran entry
