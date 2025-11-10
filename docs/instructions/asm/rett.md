# RETT - Return from Trap Handler

## Overview

**Mnemonic:** `rett`
**Function:** Return from trap handler to interrupted code
**Class:** CALL
**Privilege:** supervisor

**Format:** `RETT`

---

## Description

Returns from a trap handler entered with ENTT. Restores the complete processor state and returns to the interrupted instruction.

RETT:
1. Switches back from trap stack to user stack
2. Restores all processor state (PC, flags, registers)
3. Deallocates trap handler frame
4. Resumes interrupted execution

**Operands:** 0
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFDB3 | RETT |

---

## Examples

### Example 1: Trap handler

```assembly
TRAP_HANDLER:
        ENTT 50, 500
        % Handle trap condition
        % Fix issue
        RETT                % Resume user code
```

---

## Reference Manual

**Section:** §13.11
**Title:** Return instructions

---

## See Also

- [ENTT](entt.md) - Enter trap handler
- [INT](int.md) - Interrupt
- [INTR](intr.md) - Interrupt return
