# ENTT - Enter Trap Handler

## Overview

**Mnemonic:** `entt`
**Function:** Initialize trap handler with dedicated stack
**Class:** CALL
**Privilege:** supervisor

**Format:** `ENTT <trap handler stack demand/r/W>,<total trap stack demand/r/W>`

---

## Description

Initializes a trap handler with its own dedicated stack space separate from the main program stack. This prevents trap handlers from overflowing the user stack.

ENTT:
1. Switches to trap handler stack
2. Allocates `<trap handler stack demand>` words
3. Reserves `<total trap stack demand>` total space
4. Saves trap context

**Operands:** 2
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFD80 | ENTT |

---

## Examples

### Example 1: Trap handler entry

```assembly
TRAP_HANDLER:
        ENTT 50, 500        % 50 words local, 500 total
        % Handle trap
        RETT                % Return from trap
```

---

## Reference Manual

**Section:** §13.10
**Title:** Subroutine entry points

---

## See Also

- [RETT](rett.md) - Return from trap
- [ENTM](entm.md) - Main program entry
