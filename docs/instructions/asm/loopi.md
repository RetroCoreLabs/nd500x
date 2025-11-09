# LOOPI - Loop with Increment

**Mnemonic:** `loopi`  
**Function:** Loop with increment  
**Class:** CONTROL  
**Format:** `LOOPI <reg>,<limit>,<label>`

**Description:** Increments register, compares with limit, loops if less.

**Examples:**

```assembly
        W1 := 0
LOOP1:
        % Loop body
        LOOPI I1, 10, LOOP1
```

**Reference:** §13.4 Loop with increment  
**See Also:** [LOOPD](loopd.md), [LOOP](loop.md)
