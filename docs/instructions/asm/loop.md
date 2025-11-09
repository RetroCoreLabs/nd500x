# LOOP - Loop General

**Mnemonic:** `loop`  
**Function:** General loop control  
**Class:** CONTROL  
**Format:** `LOOP <reg>,<label>`

**Description:** Decrements register, jumps if not zero.

**Examples:**

```assembly
        W1 := 10
LOOP1:
        % Loop body
        LOOP I1, LOOP1
```

**Reference:** §13.6 Loop general  
**See Also:** [LOOPI](loopi.md), [LOOPD](loopd.md)
