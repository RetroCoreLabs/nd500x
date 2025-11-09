# GO - Unconditional Jump

**Mnemonic:** `go`  
**Function:** Unconditional relative jump  
**Class:** CONTROL  
**Format:** `GO <label>`

**Description:** Unconditional jump to label (PC-relative addressing).

**Examples:**

```assembly
LOOP:
        % ... code ...
        GO LOOP

% Skip code
        GO AFTER
SKIP:
        % This code is skipped
AFTER:
```

**Reference:** §13.1 Unconditional relative jump  
**See Also:** [CALL](call.md), conditional jumps
