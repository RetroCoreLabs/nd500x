# CALL - Call Subroutine

**Mnemonic:** `call`  
**Function:** Call subroutine (absolute addressing)  
**Class:** CONTROL  
**Format:** `CALL <address>`

**Description:** Calls subroutine at absolute address. Pushes return address on stack.

**Examples:**

```assembly
% Call subroutine
CALL MYSUB

MYSUB:
        % Subroutine code
        RET
```

**Reference:** §13.8 Call subroutine absolute  
**See Also:** [CALLG](callg.md), [RET](ret.md)
