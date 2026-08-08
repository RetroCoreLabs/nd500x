# SREGBL - Save Register Block

## Overview

**Mnemonic:** `sregbl`
**Function:** Save multiple registers to memory
**Class:** SYSTEM
**Privilege:** supervisor
**Extension:** '87 architecture extension

**Format:** `SREGBL <mask>, <address>`

---

## Description

Saves a block of CPU registers to memory based on a bit mask. Each set bit in the mask corresponds to a register that should be saved. Registers are stored sequentially at memory locations starting from the base address plus (register_number × 4).

**Operation:**
```
For each bit set in mask:
  Register[bit_position] → memory[address + bit_position * 4]
```

**Key Characteristics:**
- Privileged instruction (supervisor mode only)
- '87 architecture extension
- Saves multiple registers with single instruction
- Mask specifies which registers to save
- Registers saved at address + (register_num × 4)
- Used for context saving and task switching

**Common Use Cases:**
- Process context switching
- Interrupt handler prologues
- Exception handler state saving
- Task scheduler register preservation
- System call entry points
- Debugging and core dumps

**Operands:** 2 (mask, base address)
**Variants:** 1 opcode

---

## Variants

Total variants: 1

| Variant | Opcode | Assembly |
|---------|--------|----------|
| 1/1 | 0xFFF7 | SREGBL |

---

## Operands

### Operand 1 (Mask)

**Type:** Word
**Access:** Read

Bit mask specifying which registers to save. Each bit corresponds to a register number (see Chapter 2 of reference manual).

### Operand 2 (Address)

**Type:** Word
**Access:** Read

Base memory address where registers will be stored. Register N stored at address + N*4.

**Supported modes (both operands):**
- **LOCAL** - Local variable
- **RECORD** - Record field
- **CONSTANT** - Immediate value
- **REGISTER** - Register value
- **PRE_INDEXED** - Indexed access
- **ABSOLUTE** - Absolute address

---

## Trap Conditions

- **Privilege violation:** If executed in user mode
- **Illegal instruction code (IIC):** Not supported on pre-'87 systems
- **Addressing traps:** Invalid memory address

---

## Data Status Bits

No flags affected.

---

## Examples

### Example 1: Save all general-purpose registers

```assembly
        % Save I1-I4, A1-A4 registers
        W1 := 0x00FF                % Mask for registers 0-7
        W2 := SAVE_AREA
        SREGBL W1, W2
```

**Explanation:** Context switch: save working registers to memory.

### Example 2: Interrupt handler prologue

```assembly
INT_HANDLER:
        % Save minimal register set
        SREGBL 0x000F, INT_SAVE     % Save I1-I4
        % ... interrupt processing ...
```

**Explanation:** Interrupt handler saving only registers it will use.

### Example 3: Task context save

```assembly
        % Save complete task state
        W1 := 0xFFFF                % All registers
        W2 := TASK_CONTEXT_AREA
        SREGBL W1, W2
```

**Explanation:** Task scheduler saving full register context.

### Example 4: Selective register save

```assembly
        % Save only arithmetic registers
        SREGBL 0x00F0, ARITH_SAVE   % Save A1-A4
```

**Explanation:** Saving only subset of registers for efficiency.

### Example 5: System call entry

```assembly
SYSCALL_ENTRY:
        % Preserve caller registers
        W1 := CALLER_MASK
        W2 := SYSCALL_SAVE_AREA
        SREGBL W1, W2
        % ... system call processing ...
```

**Explanation:** System call preserving caller's register state.

### Example 6: Exception handler

```assembly
EXCEPT_HANDLER:
        % Save all state for debugging
        SREGBL 0xFFFF, EXCEPT_DUMP
        CALL LOG_EXCEPTION
```

**Explanation:** Exception handler capturing full register state for debugging.

### Example 7: Coroutine context switch

```assembly
        % Save current coroutine state
        W1 := COROUTINE_REGS
        W2 := CURRENT_CORO.SAVE_AREA
        SREGBL W1, W2
        % Switch to other coroutine
```

**Explanation:** Coroutine implementation saving register state.

---

## Performance Notes

- **Execution:** 5 + (2 × number_of_registers_saved) cycles
- **Memory accesses:** One write per saved register
- **Optimization:** Only set bits for registers that need saving

**Usage recommendations:**
- Use minimal mask for performance
- Pair with LREGBL (load register block) for context restore
- Essential for operating system context switching
- Consider selective saving for interrupt handlers

**Register numbering:**
- See Chapter 2 of reference manual for register numbers
- Typical mapping: I1-I4, A1-A4, E1-E4, etc.
- Implementation-specific layout

---

## Reference Manual

**Section:** §16.27.1
**Title:** SREGBL - Save register block ('87 extension)

---

## See Also

- [LREGBL](lregbl.md) - Load register block (restore registers)
- [SVERS](svers.md) - Store microprogram version
- [SLOCA](sloca.md) - System location access
- [SYSTEM instruction reference](../../instruction-reference/SYSTEM.md)
