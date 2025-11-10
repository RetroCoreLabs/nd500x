# DMON - Data Memory Management On

## Overview

**Mnemonic:** `dmon`
**Function:** Enable data memory management
**Class:** SYSTEM
**Privilege:** supervisor

**Format:** `DMON`

---

## Description

Enables the data memory management system, causing subsequent data memory accesses to be translated through the memory management unit (MMU) rather than being interpreted as direct physical addresses. This privileged instruction activates virtual addressing, memory protection, and capability-based security for data operations.

When DMON executes, the data MMU is enabled for all load and store operations. This provides:
- Virtual-to-physical address translation via page tables for data
- Segment descriptor validation for data accesses
- Capability-based protection enforcement for data
- Memory protection domains for data isolation
- Process address space separation for data

Operating with data memory management enabled is the normal mode for multi-user operating systems, providing memory protection between processes and between user/supervisor modes. The MMU translates virtual data addresses through a three-level hierarchy:
1. Capability table (domain-based access rights)
2. Segment table (logical memory regions)
3. Page table (physical page mappings)

This instruction is essential during:
- Operating system initialization (after boot completes)
- Transition from firmware to OS kernel
- Recovery from diagnostic mode
- Re-enabling protection after hardware access
- Process creation and initialization

If data memory management is already enabled when DMON executes, the instruction has no effect, making it idempotent and safe to execute redundantly.

Unlike PMON which affects instruction fetches, DMON only affects data memory operations (loads and stores). Both program and data MMUs operate independently and must be managed separately.

**Operands:** None
**Variants:** 1 opcode(s)

---

## Variants

| Variant | Opcode | Assembly Notation |
|---------|--------|-------------------|
| 1/1 | 0xFF16 | DMON |

---

## Operands

None - operates on implicit data MMU state.

---

## Trap Conditions

- **Illegal Instruction Code (IIC)**: If executed in user mode (privilege violation)

---

## Data Status Bits

All status bits unaffected.

---

## Examples

### Example 1: OS initialization after boot

```assembly
% Enable data MMU after physical memory setup
BOOT_TO_OS:
        % Memory tables now configured
        DMON                    % Enable data translation
        PMON                    % Enable program translation
        % Now running with full memory protection
        CALL OS_MAIN
```

### Example 2: Recovery from diagnostic mode

```assembly
% Return to protected mode after diagnostics
        CALL RUN_MEMORY_TEST
        % Test complete, restore MMU
        DMON                    % Re-enable data protection
        % Data accesses now protected
```

### Example 3: After direct hardware access

```assembly
% Toggle MMU for device register access
        DMOF                    % Disable for I/O access
        W MOVE CONFIG, DEVICE_REG
        DMON                    % Re-enable protection
        % Back to safe protected mode
```

### Example 4: Process initialization

```assembly
% Set up new process with memory protection
CREATE_PROCESS:
        CALL SETUP_PAGE_TABLES
        CALL SETUP_SEGMENTS
        DMON                    % Enable data protection
        % Process has isolated address space
        RET
```

### Example 5: System call entry (MMU already on)

```assembly
% System call handler - ensure MMU enabled
SYSCALL_ENTRY:
        DMON                    % Idempotent - already on
        % Safe to assume data protection active
        CALL HANDLE_SYSCALL
        RET
```

### Example 6: Multiprocessor synchronization

```assembly
% Ensure all CPUs have data MMU enabled
MP_INIT:
        DMON                    % Enable on this CPU
        % Signal other CPUs to enable
        CALL BROADCAST_ENABLE_MMU
        % All CPUs now protected
```

### Example 7: Recovery from MMU fault

```assembly
% Re-enable after fixing page table
MMU_FAULT_FIXED:
        DMOF                    % Disable to fix tables
        CALL FIX_PAGE_TABLE
        DCTSB                   % Clear translation cache
        DMON                    % Re-enable with new tables
        % Resume with corrected mapping
```

---

## Performance Notes

- **Scope**: Affects only data memory accesses (load/store operations)
- **Program Flow**: Does not alter instruction pointer or control flow
- **Idempotent**: Safe to execute when already enabled
- **Translation Active**: Subsequent data accesses use MMU page table walks
- **Protection Enabled**: Full segmentation and capability checking for data
- **Typical Use**: OS initialization, protection enablement, recovery
- **Paired with DMOF**: DMOF disables data memory management
- **Independent of PMON**: Program and data MMUs operate separately
- **Cache Implications**: Data TLB/TSB becomes active for translation caching
- **Performance Cost**: MMU translation adds cycles to first access (cached thereafter)

---

## Reference Manual

**Section:** §16.13
**Title:** Data memory management on

---

## See Also

- [DMOF](dmof.md) - Data memory management off
- [PMON](pmon.md) - Program memory management on
- [PMOF](pmof.md) - Program memory management off
- [DCTSB](dctsb.md) - Data translation speedup buffer clear
