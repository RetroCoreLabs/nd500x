# ENTD - Enter with Display

## Overview

**Mnemonic:** `entd`
**Function:** Enter subroutine with display register setup
**Class:** CALL
**Privilege:** user

**Format:** `ENTD <stack demand/r/W>`

---

## Description

Enters a subroutine and allocates a stack frame of the specified size. Sets up display registers for block-structured language support (Pascal, Algol style).

The display mechanism maintains pointers to stack frames at each static nesting level, enabling efficient access to variables in enclosing scopes. ENTD:
1. Allocates `<stack demand>` words on the stack
2. Saves return address and frame pointer
3. Updates display register for current nesting level
4. Sets up new frame pointer

This is essential for compiled code from block-structured languages with nested procedures.

**Operands:** 1
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD7C | ENTD |

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

### Example 1: Pascal procedure entry

```assembly
PROC:   ENTD 20             % Allocate 20 words for locals
        % Procedure body with access to outer scopes
        RETD                % Return with display cleanup
```

### Example 2: Nested procedure

```assembly
OUTER:  ENTD 10             % Outer procedure
        % ...
INNER:  ENTD 5              % Inner procedure (level 2)
        % Can access OUTER's variables via display
        RETD
        % ...
        RETD
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [RETD](retd.md) - Return with display
- [ENTS](ents.md) - Simple entry
- [ENTF](entf.md) - Fortran entry
- [CHAIN](chain.md) - Multilevel chain
