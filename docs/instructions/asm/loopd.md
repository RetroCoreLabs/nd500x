# LOOPD - Loop with Decrement

**Mnemonic:** `loopd`  
**Function:** Loop with decrement  
**Class:** CONTROL  
**Format:** `LOOPD <reg>,<limit>,<label>`

**Description:** Decrements register, compares with limit, loops if greater.

**Examples:**

```assembly
        W1 := 10
LOOP1:
        % Loop body
        LOOPD I1, 0, LOOP1
```

**Reference:** §13.5 Loop with decrement  
**See Also:** [LOOPI](loopi.md), [LOOP](loop.md)
