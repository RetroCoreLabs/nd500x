# RETK - Return with K Flag Set

## Overview

**Mnemonic:** `retk`
**Function:** Return from subroutine with K flag set
**Class:** CALL
**Privilege:** user

**Format:** `RETK`

---

## Description

Returns from subroutine with local data area, setting the K flag bit in the status register. Restores the base register and return address from the current local data area, then transfers control to the caller. The K flag is set to 1 to indicate a specific return condition.

**Operation:**
```
1 → STATUS.K
B.RETA → PC → L
B.PREVB → B
```

**Key Characteristics:**
- Returns with K flag set to 1
- Deallocates stack frame
- Restores previous base register
- Restores return address to link register
- Paired with ENT/ENTB instructions
- Used for error/status indication

**Common Use Cases:**
- Error condition returns
- Status flag propagation
- Conditional return handling
- Function result indication
- Multi-exit procedures

**Operands:** None
**Variants:** 1 opcode

---

## Examples

### Example 1: Error return

```assembly
FUNC:   ENT 40
        % Try operation
        W1 COMP STATUS, 0
        IF<GO ERROR
        % Success
        RET                 % Return with K=0
ERROR:  RETK                % Return with K=1 (error)
```

**Explanation:** Use RETK to indicate error condition to caller.

### Example 2: Boolean function result

```assembly
ISVALID: ENT 20
        % Validate input
        W1 COMP ARG, MIN
        IF<GO INVALID
        W1 COMP ARG, MAX
        IF>GO INVALID
        RET                 % Valid: K=0
INVALID: RETK               % Invalid: K=1
```

**Explanation:** Return boolean result via K flag.

### Example 3: Caller checks K flag

```assembly
        % Call function
        CALL FUNC
        IF K GO HANDLE_ERROR
        % Success path
        GO CONTINUE
HANDLE_ERROR:
        % Error path
CONTINUE:
```

**Explanation:** Caller uses IF K to check return status.

---

## Trap Conditions

None

---

## Data Status Bits

- **K (Flag)**: Set to 1
- **Other flags**: Unaffected

---

## Reference Manual

**Section:** §13.11
**Title:** Subroutine return

---

## See Also

- [RET](ret.md) - Return with K flag cleared
- [ENT](ent.md) - Enter subroutine
- [RETBK](retbk.md) - Buddy return with K set
- [IF K RET](if-kgo.md) - Conditional return
