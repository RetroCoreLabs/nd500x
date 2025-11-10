# RET - Return from Subroutine

## Overview

**Mnemonic:** `ret`
**Function:** Return from subroutine (clear K flag)
**Class:** CALL
**Privilege:** user

**Format:** `RET`

---

## Description

Returns from a subroutine by restoring the return address and base register from the current stack frame, then transferring control back to the caller. The K flag is cleared as a side effect, allowing the caller to test whether the subroutine succeeded (RETK sets K=1 for success).

**Operation:**
1. Clear K flag → 0
2. Load return address: B.RETA → PC
3. Save return address: PC → L
4. Restore base register: B.PREVB → B

This is the standard return mechanism for subroutines called with CALL or ENTB. The return address was previously saved in the stack frame by the caller, and PREVB points to the caller's stack frame.

**K Flag Semantics:**
- **RET**: Clears K (K=0) - typically indicates failure/false
- **RETK**: Sets K (K=1) - typically indicates success/true
- Caller can test K flag after return using IFKGO

**Common Use Cases:**
- Normal function/subroutine return
- Return with failure indication (K=0)
- Return from leaf functions
- Return from error handlers

**Operands:** 0
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation | K Flag |
|---------|--------|-------------------|--------|
| 1/1 | 0x0080 | RET | Cleared (0) |

---

## Operands

**None**: All operands implicit (stack frame)

**Result**:
- PC = B.RETA (return address)
- L = old PC
- B = B.PREVB (caller's base)
- K = 0 (cleared)

---

## Trap Conditions

**None**: This instruction cannot trap

---

## Data Status Bits

- **K (User flag)**: Cleared to 0
- **Z, S, C, V**: Unaffected

---

## Examples

### Example 1: Simple subroutine

```assembly
PRINT_MESSAGE:
        % Print message code
        CALL OUTPUT_CHAR
        RET               % Return with K=0
```

### Example 2: Failure return

```assembly
VALIDATE_INPUT:
        W1 COMP INPUT, MIN
        IF<GO INVALID
        W1 COMP INPUT, MAX
        IF>GO INVALID
        % Valid input
        RETK              % Return with K=1 (success)
INVALID:
        RET               % Return with K=0 (failure)
```

### Example 3: Caller testing return status

```assembly
        CALL OPEN_FILE
        IFKGO SUCCESS     % Branch if K=1 (RETK used)
        % File open failed (RET used, K=0)
        GO ERROR_HANDLER
SUCCESS:
        % File opened successfully
```

### Example 4: Nested returns

```assembly
OUTER:
        CALL INNER
        RET

INNER:
        % Inner function
        RET               % Returns to OUTER
```

### Example 5: Early return

```assembly
PROCESS:
        W1 COMP PARAM, 0
        IF=GO EARLY_EXIT
        % Main processing
        CALL DO_WORK
EARLY_EXIT:
        RET
```

### Example 6: Leaf function (no calls)

```assembly
ADD_TWO:
        W1 ADD A, B, RESULT
        RET
```

### Example 7: Multiple return paths

```assembly
SEARCH:
        W1 CLR
LOOP:
        W2 COMP ARRAY(I1), TARGET
        IF=GO FOUND
        W1 ADD 1, I1
        W1 COMP I1, SIZE
        IF<GO LOOP
        % Not found
        RET               % K=0
FOUND:
        RETK              % K=1
```

---

## Performance Notes

- **Size**: 2 bytes (single-word instruction)
- **Execution**: 2-3 cycles (pop + jump)
- **Stack**: Restores caller's frame
- **vs RETK**: RET clears K, RETK sets K
- **vs RETD**: RET uses stack frame, RETD uses L register
- **vs RETB**: RET for ENTB/CALL, RETB for ENTBB (heap-allocated frames)

---

## Reference Manual

**Section:** §13.11
**Title:** Subroutine return

---

## See Also

- [CALL](call.md) - Call subroutine
- [RETK](retk.md) - Return with K flag set
- [RETD](retd.md) - Return direct (from L register)
- [ENTB](entb.md) - Enter block
- [RETB](retb.md) - Return from buddy subroutine
- [IFKGO](ifkgo.md) - Branch if K flag set
