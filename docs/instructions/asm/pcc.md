# PCC - Program Cache Clear

## Overview

**Mnemonic:** `pcc`
**Function:** Clear program instruction cache
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `PCC`

---

## Description

Clears (invalidates) the program instruction cache, forcing subsequent instruction fetches to reload from main memory. This privileged instruction is essential for maintaining cache coherence after code modification, dynamic loading, or self-modifying code execution.

PCC marks all entries in the program cache as invalid without writing back any data (instruction caches are read-only). Following the PCC instruction, the next instruction fetch will reload from memory, ensuring that any modifications to program code are reflected in execution.

On systems without an instruction cache, PCC executes as a no-op, allowing code portability across different ND-500 configurations.

This instruction is typically used after:
- Loading or relocating executable code
- Applying runtime code patches
- Debugger breakpoint insertion/removal
- JIT compilation or code generation
- DMA transfers that modify program memory

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF14 | PCC |

---

## Operands

None - operates on implicit program cache.

---

## Trap Conditions

None (when executed in supervisor mode)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: After dynamic code loading

```assembly
% Load new code module into memory
        CALL LOAD_MODULE
        % Clear instruction cache to ensure new code is fetched
        PCC
        % Safe to execute new code
        CALL NEW_MODULE_ENTRY
```

### Example 2: Self-modifying code

```assembly
% Modify instruction in program memory
        W MOVE NEW_OPCODE, CODE_LOCATION
        % Ensure cache reflects modification
        PCC
        % Modified code will execute correctly
```

### Example 3: Debugger breakpoint management

```assembly
% Insert breakpoint (replace instruction with trap)
        BY MOVE TRAP_OPCODE, BREAKPOINT_ADDR
        PCC                     % Invalidate cached instruction
        % Breakpoint now active
```

### Example 4: Code patching

```assembly
% Apply runtime patch to code
PATCH:  % Copy patch code to target location
        CALL APPLY_CODE_PATCH
        PCC                     % Ensure patched code executes
        RET
```

### Example 5: Multi-processor synchronization

```assembly
% Update shared code, synchronize with other CPUs
        CALL UPDATE_SHARED_CODE
        PCC                     % Clear local cache
        % Signal other CPUs to clear their caches
        CALL BROADCAST_CACHE_CLEAR
```

---

## Performance Notes

- **No Effect on Data Cache**: Only program instruction cache affected
- **No Writeback**: Instruction caches are read-only, no data to flush
- **Implicit on Some Systems**: Some hardware may automatically invalidate on code writes
- **Performance Impact**: Next instruction fetch will be slower (cache miss)
- **Typical Use**: Code modification, dynamic loading, debugging
- **Supervisor Only**: Requires privilege level check

---

## Reference Manual

**Section:** §16.12
**Title:** Program cache clear

---

## See Also

- [DCC](dcc.md) - Data cache clear
- [DCTSB](dctsb.md) - Data translation speedup buffer clear
- [PCTSB](pctsb.md) - Program translation speedup buffer clear
