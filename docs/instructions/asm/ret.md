# RET - Return from Subroutine

**Mnemonic:** `ret`  
**Function:** Return from subroutine  
**Class:** CONTROL  
**Format:** `RET`

**Description:** Returns from subroutine. Pops return address from stack.

**Examples:**

```assembly
MYSUB:
        % Subroutine code
        RET
```

**Reference:** §13.11 Subroutine return  
**See Also:** [CALL](call.md), [RETK](retk.md)
