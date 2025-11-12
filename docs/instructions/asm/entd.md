# ENTD - Enter with Display

## Overview

**Mnemonic:** `entd`
**Function:** Enter subroutine with display register setup
**Class:** CALL
**Privilege:** user

**Format:** `ENTD <stack demand/r/W>`

---

## Description

Enters a subroutine and allocates a stack frame of the specified size, setting up display registers for block-structured language support (Pascal, Algol-68, Ada style).

The display mechanism maintains pointers to stack frames at each static nesting level, enabling efficient access to variables in enclosing scopes without chain walking.

**Operation:**
```
1. Check stack bounds (LL/HL)
2. Allocate <stack demand> words on stack
3. Save return address and current frame pointer
4. Update display register for current nesting level
5. Set up new frame pointer (B register)
6. Update TOS register
```

**Key Characteristics:**
- Display-based lexical scoping (O(1) outer scope access)
- Eliminates static chain traversal overhead
- Block-structured language support (Pascal, Algol-68)
- Paired with RETD for proper cleanup
- Stack overflow detection via LL/HL bounds
- Efficient nested procedure calls

**Common Use Cases:**
- Pascal procedure entry with nested scopes
- Algol-68 block structure implementation
- Ada nested procedure compilation
- Any language with static lexical nesting
- Compiler-generated code for block scoping

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
