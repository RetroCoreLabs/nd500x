# PMOF - Program Memory Management Off

## Overview

**Mnemonic:** `pmof`
**Function:** Disable program memory management
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `PMOF`

---

## Description

Disables the program memory management system, causing subsequent instruction fetches to be interpreted as direct physical addresses rather than virtual addresses requiring translation. This privileged instruction is essential for low-level system initialization, debugging, and firmware operation.

**Operation:**
```
disable program MMU
L → PC  (jump to physical address)
```

**Key Characteristics:**
- Supervisor-only MMU control instruction
- Disables virtual-to-physical address translation
- Loads PC from L register (physical address jump)
- Atomic mode change and control transfer
- Idempotent (safe to execute when already off)
- No protection or translation for instruction fetches
- Essential for boot, firmware, and diagnostics
- 4-6 cycles execution time
- Can be used as indirect jump when already physical

When PMOF executes, control transfers to the physical address contained in the L register. This allows atomic transition from virtual to physical addressing mode while simultaneously positioning program execution at a known physical location.

If program memory management is already off when PMOF executes, control simply transfers to the L register address without additional side effects, allowing PMOF to be used as an indirect jump to physical address.

Operating with program memory management disabled means:
- No page table translation for instruction fetches
- No segment descriptor checking
- No capability-based protection
- Direct physical memory access for code

This mode is used during:
- System boot and initialization
- Low-level hardware diagnostics
- ROM/firmware execution
- Recovery from memory management faults
- Debugger operation at physical level

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF19 | PMOF |

---

## Operands

None - implicitly uses L register for next instruction address.

---

## Trap Conditions

- **Illegal Instruction Code (IIC)**: If executed in user mode (privilege violation)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: System boot initialization

```assembly
% Early boot code - switch to physical addressing
        W MOVE PHYS_START, L    % Load physical entry point
        PMOF                    % Disable MMU, jump to physical address
        % Now executing in physical mode at PHYS_START
```

### Example 2: ROM firmware entry

```assembly
% Transfer control to ROM diagnostics
        W MOVE ROM_BASE, L      % ROM physical address
        PMOF                    % Enter physical mode, execute ROM
```

### Example 3: Memory management fault recovery

```assembly
MMU_FAULT_HANDLER:
        % Disable MMU to access fault recovery code
        W MOVE RECOVERY_PHYS, L
        PMOF
        % Recovery code runs in physical mode
```

### Example 4: Hardware diagnostic mode

```assembly
% Enter diagnostic mode with direct hardware access
DIAGNOSTICS:
        W MOVE DIAG_CODE_PHYS, L
        PMOF                    % Physical addressing for diagnostics
```

### Example 5: Physical address jump (already in physical mode)

```assembly
% Use PMOF as indirect jump when already physical
        W MOVE NEW_LOCATION, L
        PMOF                    % Jump to L (no mode change)
```

---

## Performance Notes

- **L Register Critical**: Contains next physical instruction address
- **Atomic Operation**: Mode change and control transfer are atomic
- **No Translation**: Subsequent fetches bypass MMU entirely
- **Protection Disabled**: No segmentation or capability checking
- **Idempotent**: Safe to execute when already in physical mode
- **Typical Use**: Boot, firmware, recovery, diagnostics
- **Paired with PMON**: PMON re-enables memory management

---

## Reference Manual

**Section:** §16.16
**Title:** Program memory management off

---

## See Also

- [PMON](pmon.md) - Program memory management on
- [DMOF](dmof.md) - Data memory management off
- [DMON](dmon.md) - Data memory management on
- [PCTSB](pctsb.md) - Program translation speedup buffer clear
