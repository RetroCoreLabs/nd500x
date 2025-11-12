# NOOP - No Operation

## Overview

**Mnemonic:** `noop`
**Function:** No operation
**Class:** CONTROL
**Privilege:** user

**Format:** `NOOP`

---

## Description

Executes a complete instruction cycle without performing any operation or modifying any processor state. The instruction fetches, decodes, and executes normally, advancing the program counter (PC) to the next instruction, but makes no changes to registers, memory, or status flags.

**Key Characteristics:**
- Smallest instruction: 1 word (2 bytes), opcode 0x0003
- Fastest execution: typically 1 cycle
- Zero side effects (no registers, memory, or flags modified)
- Cannot trap (except instruction fetch failure)
- Essential for code patching without address changes
- Critical for timing loops and alignment padding
- Guaranteed safe operation (never causes state changes)
- Common in debugging and pipeline control

NOOP is the smallest instruction in the ND-500 architecture at just one word (2 bytes), with opcode 0x0003. Despite doing nothing functionally, NOOP serves several critical purposes in assembly programming:

**Primary Use Cases:**

1. **Code Patching and Modification**: Replacing unwanted instructions with NOOPs allows code deletion without changing addresses of subsequent code, preserving jump targets and call addresses
2. **Alignment and Padding**: Creating space for future code insertion or aligning instruction boundaries for performance or debugging
3. **Timing Loops**: Implementing precise delays by executing a known number of NOOPs in sequence
4. **Pipeline Control**: On pipelined processors, forcing instruction boundaries or synchronization points
5. **Debugging and Breakpoint Replacement**: Temporarily disabling instructions during debug without reassembly
6. **Atomic Operation Padding**: Ensuring instruction sequences maintain specific sizes for timing-critical code

The instruction has zero operands and affects no status bits. It cannot trap (except for the general case of instruction fetch from invalid memory). NOOP is the safest instruction in the architecture - it is guaranteed never to cause side effects.

**Operands:** 0
**Variants:** 1 opcode

---

## Variants

| Variant | Opcode | Assembly Notation | Binary Pattern |
|---------|--------|-------------------|----------------|
| 1/1 | 0x0003 | NOOP | 0000_0000_0000_0011 |

---

## Operands

**None**: This instruction has no operands and operates on no registers or memory locations.

**Result**: No result is produced. All processor state remains unchanged.

---

## Trap Conditions

- **None**: This instruction cannot trap under normal execution
- **Memory traps**: Only if instruction fetch itself fails (protection violation, non-existent memory)

---

## Data Status Bits

**All flags unaffected:**
- **Z (Zero)**: Unchanged
- **S (Sign)**: Unchanged
- **C (Carry)**: Unchanged
- **V (Overflow)**: Unchanged
- **K (Interrupt mask bits)**: Unchanged

---

## Examples

### Example 1: Code deletion/patching

```assembly
% Replace unwanted instruction with NOOP to preserve addresses
OLD_CODE:
        W1 ADD OFFSET, I1      % Original instruction

% After patching:
OLD_CODE:
        NOOP                   % Instruction removed but addresses preserved
```

### Example 2: Timing delay loop

```assembly
% Precise 10-cycle delay (assuming 1 cycle per NOOP)
DELAY_10:
        NOOP
        NOOP
        NOOP
        NOOP
        NOOP
        NOOP
        NOOP
        NOOP
        NOOP
        NOOP
        RET
```

### Example 3: Alignment padding

```assembly
% Ensure next label falls on even 16-byte boundary
FUNC_START:
        W1 CLR
        RET
        NOOP                   % Pad to alignment
        NOOP
        % Next function starts here at aligned address
NEXT_FUNC:
```

### Example 4: Space for future expansion

```assembly
% Leave space for later code insertion
INIT:
        CALL SETUP
        NOOP                   % Reserved for future initialization
        NOOP
        NOOP
        NOOP
        CALL MAIN
```

### Example 5: Pipeline synchronization

```assembly
% Force pipeline flush after critical operation
        W1 MOVE VALUE, ST1     % Write to special register
        NOOP                   % Allow write to complete
        NOOP                   % Before dependent operation
        W1 MOVE ST1, RESULT
```

### Example 6: Debugger breakpoint placeholder

```assembly
% Original code with breakpoint:
DEBUG_POINT:
        % Debugger replaces next instruction with trap
        W1 ADD DATA, I1

% When breakpoint removed:
DEBUG_POINT:
        NOOP                   % Temp replacement, will restore original
        W1 ADD DATA, I1
```

### Example 7: Conditional code disabling

```assembly
% Disable feature without reassembly
FEATURE_A:
        NOOP                   % Was: CALL EXPENSIVE_FUNCTION
        NOOP                   % Was: W1 MOVE F1, RESULT
        % Feature disabled but code structure preserved
```

---

## Performance Notes

- **Size**: 2 bytes (1 word) - smallest possible instruction
- **Execution Time**: Typically 1 machine cycle (fastest instruction)
- **No Side Effects**: Guaranteed safe - no memory access, no register modification
- **Pipeline**: May cause pipeline bubble on some implementations
- **Use in Loops**: Inefficient for long delays - use timer interrupts instead
- **Code Size**: Excessive NOOPs increase code size - use sparingly
- **Optimization**: Optimizing compilers typically remove NOOPs unless volatile
- **Debugging**: Extremely valuable for runtime code patching

---

## Reference Manual

**Section:** §15.10
**Title:** No operation

---

## See Also

- [HALT](halt.md) - Stop processor execution
- [WAIT](wait.md) - Wait for interrupt
- [RET](ret.md) - Return from subroutine
