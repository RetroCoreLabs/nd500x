# IFKRET - Conditional Return if K Set

## Overview

**Mnemonic:** `ifkret`
**Function:** Conditional return if K flag is set
**Class:** CALL
**Privilege:** user
**Format:** `IFKRET`

---

## Description

Conditionally returns from a subroutine if the K flag (user flag) is set. This instruction combines a flag test with a return operation, providing an efficient way to exit subroutines early based on status conditions. If K is set, the return address is popped from the stack and loaded into the program counter. If K is clear, execution continues with the next instruction.

This instruction is commonly used in routines that perform validation or error checking, allowing early return when a condition is met. The K flag is typically set by string operations (when destination full or source empty), arithmetic operations that detect special conditions, or explicitly via SETK.

**Common Use Cases:**
- Early return from validation routines
- String operation termination checks
- Conditional function exit based on K flag
- Error handling with flag-based returns

**Operands:** 0 (implicit K flag test)
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0x009D | IFKRET |

---

## Operands

**No explicit operands** (operates on K flag and stack)

**Result**: If K=1, return to caller; if K=0, continue execution

---

## Trap Conditions
- **Addressing traps**: Invalid stack access
- **Branch trap (BT)**: If branch trapping enabled

---

## Data Status Bits
- **Z,S,C,V**: Unaffected
- **K**: Read but not modified

---

## Examples

### Example 1: Early return on string completion
```assembly
VALIDATE_STRING:
        W1 CLR
        W2 CLR
        BY SMOVE SOURCE, DEST
        IFKRET          % Return if dest full (K=1)
        % Continue processing if source exhausted
        RET
```

### Example 2: Validation with K flag
```assembly
CHECK_BOUNDS:
        W1 COMP LOWER, UPPER
        SETK            % Set K if in range
        IFKRET          % Return early if valid
        % Handle invalid case
        W1 MOVE -1, I1  % Error code
        RET
```

### Example 3: Search routine early exit
```assembly
FIND_ELEMENT:
        W1 CLR
LOOP:
        BY COMP TABLE(W1), TARGET
        IFEQGO FOUND
        W1 INC
        W1 COMP TABLE_SIZE
        IF<GO LOOP
        CLRK            % Not found
        RET
FOUND:
        SETK            % Found
        IFKRET          % Return with K=1
```

### Example 4: Multiple condition checks
```assembly
VALIDATE:
        CALL CHECK1
        IFKRET          % Return if CHECK1 set K
        CALL CHECK2
        IFKRET          % Return if CHECK2 set K
        CALL CHECK3
        RET
```

### Example 5: String processing loop
```assembly
PROCESS_STRINGS:
        W1 CLR
        W2 CLR
NEXT:
        BY SMOVE SRC_DESC, DST_DESC
        IFKRET          % Exit if destination full
        CALL PROCESS_CHUNK
        GO NEXT
```

### Example 6: Error flag propagation
```assembly
NESTED_CALL:
        CALL OPERATION
        IFKRET          % Propagate K flag to caller
        % Additional processing
        SETK            % Set our own success flag
        RET
```

### Example 7: Conditional cleanup
```assembly
ALLOCATE:
        CALL GET_RESOURCE
        IFKRET          % Return if allocation failed (K=1)
        % Allocation succeeded
        CALL INITIALIZE
        CLRK            % Clear K for success
        RET
```

---

## Performance Notes
- Execution: 1-2 cycles (not taken), 3-4 cycles (taken)
- Branch prediction may improve performance
- Faster than separate test + conditional jump + return

---

## Reference Manual
**Section:** §13.3
**Title:** Conditional Jump

---

## See Also
- [RET](ret.md) - Return from subroutine
- [SETK](setk.md) - Set K flag
- [CLRK](clrk.md) - Clear K flag
- [IFKGO](ifkgo.md) - Conditional jump if K set
