# RETT - Trap Handler Return

## Overview

**Mnemonic:** `rett`
**Function:** Return from trap handler
**Class:** CALL
**Privilege:** supervisor

**Format:** `RETT`

---

## Description

Returns from a trap handler, restoring the complete processor state that was saved when the trap occurred. Loads the register block from the trap frame, restores OTE, TEMM, CED, and CAS from the domain information table, and restores the status register from both the trap frame and domain table.

**Operation:**
```
Register block loaded from B.arg2..B.arg40
OTE, TEMM, CED, CAS loaded from domain info table
Status register loaded from B.arg18..B.arg19 and domain table
```

**Key Characteristics:**
- Privileged instruction (supervisor only)
- Restores complete CPU state
- Restores domain context
- Paired with trap entry mechanism
- Returns to interrupted instruction
- Restores all registers and flags

**Common Use Cases:**
- Trap handler completion
- Exception handler exit
- Interrupt service routine return
- System call completion
- Error recovery completion

**Operands:** None
**Variants:** 1 opcode

---

## Examples

### Example 1: Overflow trap handler

```assembly
OVF_TRAP:
        % Saved state in trap frame
        % Handle overflow condition
        W1 := ERROR_CODE
        % Store error info
        RETT                % Resume interrupted code
```

**Explanation:** Return from overflow trap handler.

### Example 2: System call handler

```assembly
SYSCALL_HANDLER:
        % Determine syscall number
        W1 MOVE B.SYSCALL_NUM
        % Dispatch to handler
        % Perform system service
        RETT                % Return to user code
```

**Explanation:** Complete system call and resume user program.

### Example 3: Error recovery

```assembly
ERR_HANDLER:
        % Log error
        % Attempt recovery
        W1 COMP RECOVERY_OK, 0
        IF<GO FATAL
        % Recovery succeeded
        RETT                % Resume
FATAL:  % Cannot recover, terminate
```

**Explanation:** Conditional trap return after recovery attempt.

---

## Trap Conditions

- **Privilege violation**: Executed in user mode

---

## Data Status Bits

- **All restored**: Complete state restoration from trap frame

---

## Reference Manual

**Section:** §13.11
**Title:** Subroutine return

---

## See Also

- [ENTT](entt.md) - Enter trap handler
- [INT](int.md) - Generate interrupt
- [RTMM](rtmm.md) - Return from monitor
- [WAIT](wait.md) - Wait for interrupt
