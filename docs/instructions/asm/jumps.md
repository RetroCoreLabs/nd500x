# JUMPS - Jump to Supervisor

## Overview

**Mnemonic:** `jumps`
**Function:** Call supervisor mode
**Class:** BRANCH
**Privilege:** user (transitions to supervisor)

**Format:** `JUMPS <address>`

---

## Description

Transfers control to supervisor mode code at the specified address, saving the current context (PC and B register) for potential restoration. This instruction enables user programs to request privileged services from the operating system kernel.

**Operation:**
1. Save current PC → context.P
2. Save current B register → context.B
3. Load supervisor address → PC
4. Enter SOLO mode (single-CPU execution)
5. Return CPU number → W1

This is an ND-500/'87 extension instruction that implements the user-to-supervisor transition mechanism. It's conceptually similar to system calls (syscall/trap) on other architectures, providing controlled entry into privileged code for OS services like I/O, memory management, and process control.

**Common Use Cases:**
- **System Calls**: Request OS services (file I/O, memory allocation)
- **Privileged Operations**: Execute protected instructions
- **I/O Operations**: Access hardware through kernel drivers
- **Interrupt Handling**: Enter supervisor context
- **Resource Management**: Memory/device allocation

The SOLO mode implied by JUMPS ensures exclusive CPU access during supervisor execution, preventing interference from other processors in multi-CPU configurations.

**Operands:** 1
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0x00B9 | JUMPS |

---

## Operands

**Operand 1** (Supervisor Entry Point, Read):
- **Addressing modes**: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
- **Data type**: Word (W) - 32-bit address
- **Role**: Address of supervisor code to execute

**Result**:
- PC = supervisor address
- Context saved (PC, B)
- W1 = CPU number
- Mode = SOLO

---

## Trap Conditions

**None**: This instruction cannot trap

---

## Data Status Bits

**All flags unaffected**

---

## Examples

### Example 1: System call for file I/O

```assembly
% Request file read from OS
        W1 MOVE FILE_HANDLE, I1
        W2 MOVE BUFFER_ADDR, I2
        W3 MOVE BYTE_COUNT, I3
        JUMPS SYS_READ
        % Returns here after supervisor completes
```

### Example 2: Memory allocation request

```assembly
% Allocate memory from kernel
        W1 MOVE SIZE_BYTES, I1
        JUMPS SYS_MALLOC
        % W1 contains allocated address on return
```

### Example 3: Device I/O operation

```assembly
% Write to hardware device
        W1 MOVE DEVICE_ID, I1
        W2 MOVE DATA_WORD, I2
        JUMPS SYS_DEVICE_WRITE
```

### Example 4: Process control

```assembly
% Create new process
        W1 MOVE ENTRY_POINT, I1
        W2 MOVE STACK_SIZE, I2
        JUMPS SYS_FORK
        % W1 contains new process ID
```

### Example 5: Interrupt service

```assembly
% Enter supervisor for interrupt handling
INTERRUPT_VECTOR:
        JUMPS IRQ_HANDLER
```

### Example 6: Privileged instruction execution

```assembly
% Execute protected operation
        W1 MOVE OPERATION_CODE, I1
        JUMPS SYS_PRIVILEGED_OP
```

### Example 7: Multi-CPU coordination

```assembly
% Get CPU number
        JUMPS SYS_GET_CPU_INFO
        % W1 now contains CPU number
        % Can use for CPU-specific operations
```

---

## Performance Notes

- **Mode Switch**: Significant overhead due to privilege transition
- **Context Save**: PC and B saved automatically
- **SOLO Mode**: Exclusive CPU access during supervisor execution
- **Return Mechanism**: Supervisor code must restore context
- **Use Sparingly**: System calls are expensive - batch operations when possible
- **CPU Number**: W1 provides CPU identification in multi-processor systems

---

## Reference Manual

**Section:** §16.34
**Title:** JUMPS - Call supervisor ('87 extension)

---

## See Also

- [CALL](call.md) - User-mode subroutine call
- [JUMPG](jumpg.md) - General absolute jump
- [RET](ret.md) - Return from subroutine
- [SOLO](solo.md) - Enter SOLO mode
- [TUTTI](tutti.md) - Exit SOLO mode
