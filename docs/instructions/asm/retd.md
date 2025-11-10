# RETD - Return from Direct Subroutine

## Overview

**Mnemonic:** `retd`
**Function:** Return from display-based subroutine
**Class:** CALL
**Privilege:** user

**Format:** `RETD`

---

## Description

Returns from a direct subroutine (entered with ENTD). This is a simplified return that only transfers control via the link register. Used for leaf procedures or display-based calling conventions where the stack frame management is handled separately.

**Operation:**
```
L → PC
```

**Key Characteristics:**
- Returns via link register only
- No stack frame deallocation
- No base register restoration
- Paired with ENTD instruction
- Efficient for display-based procedures
- Used in nested procedure calling

**Common Use Cases:**
- Display-based procedure returns
- Nested procedure calling conventions
- Algol-style block-structured languages
- Leaf procedure optimization

**Operands:** None
**Variants:** 1 opcode

---

## Examples

### Example 1: Simple display procedure

```assembly
PROC:   ENTD 20
        % Procedure body with 20-byte frame
        % Access parent scope via display
        RETD                % Return via link register
```

**Explanation:** Return from display-based procedure.

### Example 2: Nested procedure

```assembly
OUTER:  ENTD 40
        % Outer procedure
        CALL INNER
        RETD

INNER:  ENTD 20
        % Can access OUTER's frame via display
        RETD                % Return to OUTER
```

**Explanation:** Nested procedures with display mechanism.

### Example 3: Conditional return

```assembly
FUNC:   ENTD 30
        % Check condition
        W1 COMP 0, ARG
        IF<GO ERROR
        % Normal processing
        RETD
ERROR:  % Error handling
        W1 := -1
        RETD
```

**Explanation:** Multiple return points in procedure.

---

## Trap Conditions

None

---

## Data Status Bits

- **Unaffected**: All flags remain unchanged

---

## Reference Manual

**Section:** §13.11
**Title:** Subroutine return

---

## See Also

- [ENTD](entd.md) - Enter with display
- [RET](ret.md) - Return with frame deallocation
- [RETK](retk.md) - Return with K flag set
- [CALL](call.md) - Call subroutine
