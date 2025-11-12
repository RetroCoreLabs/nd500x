# BP - Breakpoint

## Overview

**Mnemonic:** `bp`
**Function:** Software breakpoint for debugging
**Class:** CONTROL
**Privilege:** user

**Format:** `BP`

---

## Description

Causes a breakpoint trap, transferring control to a debug handler. This instruction is inserted by debuggers to halt program execution at specific points for inspection and debugging. When executed, it triggers a breakpoint trap if enabled, or an illegal instruction trap if disabled.

**Operation:**
- If breakpoint trap enabled → Breakpoint trap (BPT)
- If breakpoint trap disabled → Illegal instruction code trap (IIC)

**Key Characteristics:**
- Primary debugging mechanism for ND-500
- 2-byte instruction (easily overwrites most instructions)
- Triggers BPT trap if enabled, IIC trap if disabled
- Debuggers replace code temporarily with BP
- No operands required (implicit trap trigger)
- All flags unaffected
- Essential for interactive debugging
- Used for assertions, profiling, code coverage
- Never appears in production code (debug-only)

The BP instruction is the primary mechanism for interactive debugging on the ND-500. Debuggers replace target instructions with BP, catch the resulting trap, and provide inspection/control facilities. After debugging, the original instruction is restored.

**Common Use Cases:**
- Interactive debugging breakpoints
- Conditional breakpoints (when combined with code)
- Assertion checking
- Code coverage analysis
- Performance profiling points

**Operands:** 0
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x0002 | BP |

---

## Operands

**None**: Operates on trap system only

---

## Trap Conditions

- **Breakpoint instruction trap (BPT)**: If trap enabled
- **Illegal instruction code (IIC)**: If trap disabled

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: Simple breakpoint

```assembly
FUNCTION:
        W1 MOVE PARAM, I1
        BP                    % Debugger breakpoint
        CALL PROCESS
        RET
```

### Example 2: Conditional breakpoint

```assembly
DEBUG_CHECK:
        W1 COMP COUNTER, 100
        IF<>GO SKIP_BP
        BP                    % Break when counter == 100
SKIP_BP:
```

### Example 3: Assertion

```assembly
% Assert pointer is not null
        W1 COMP PTR, 0
        IF<>GO ASSERT_OK
        BP                    % Break on null pointer
ASSERT_OK:
```

### Example 4: Function entry breakpoint

```assembly
CRITICAL_FUNC:
        BP                    % Always break at entry
        ENTB 20, 3
        % Function body
        RET
```

### Example 5: Loop iteration breakpoint

```assembly
LOOP:
        BP                    % Break each iteration
        % Loop body
        W1 ADD 1, I1
        W1 COMP I1, LIMIT
        IF<GO LOOP
```

### Example 6: Error path breakpoint

```assembly
ERROR_HANDLER:
        BP                    % Break on error
        % Error handling code
        RET
```

### Example 7: Profiling point

```assembly
% Mark profiling checkpoint
CHECKPOINT_1:
        BP                    % Profiler collects data
        CALL EXPENSIVE_FUNC
        BP                    % End profiling region
```

---

## Performance Notes

- **Size**: 2 bytes
- **Normal Execution**: Traps immediately (not meant for production)
- **Debugger Use**: Replaces original instruction temporarily
- **Trap Overhead**: Significant (context switch to handler)
- **Production Code**: Should never contain BP instructions

---

## Reference Manual

**Section:** §16.4
**Title:** Break point

---

## See Also

- [NOOP](noop.md) - No operation
- [HALT](halt.md) - Halt processor
- [WAIT](wait.md) - Wait for interrupt
