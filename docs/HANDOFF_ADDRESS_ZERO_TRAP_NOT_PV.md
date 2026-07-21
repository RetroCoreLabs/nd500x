# C# handoff: null-pointer data read is Address Zero (ignorable), not Protect Violation

Full path of this document: `/home/ronny/repos/nd500x/docs/HANDOFF_ADDRESS_ZERO_TRAP_NOT_PV.md`

Written 2026-07-20. Target repo: `/mnt/e/Dev/Repos/Ronny/RetroCore/`.

## Symptom this fixes

The ND Linker (`linker-b01.dom`) `HELP` command - and any abbreviated command
form - aborts with `*** ND LINKER abortion ***`, a PROTECT VIOLATION at
`0xB0036975`, dataAddr `0x00000000`. Full-form commands work.

## Root cause (byte- and manual-verified)

During `SET-ADVANCED-MODE` at linker init the linker deterministically clears
command-table entry 41 (name pointer + length -> 0). This is correct/real-HW
behaviour. `HELP` then quicksorts the command table (bound 41 = walk indices
0..41), and its byte-compare guard does not skip the empty slot, so it
dereferences the NULL name pointer - a DATA read of virtual address
`0x00000000`.

On real ND-500 this is NOT a Protect Violation. Per ND-500 Reference Manual
ND-05.009.4:
- p73: "An address equal to zero will cause an Address Zero trap condition."
- p63: Address Zero (AZ, status bit 24) is IGNORABLE - "Ignorable trap
  conditions ... may be disabled and will have no effect on program execution",
  and AZ is handled AFTER the memory access, so the access completes.
- p75: an absent (zero) capability / a table lookup giving zero is a Page Fault;
  Protect Violation is reserved for access-code violations (write-to-read-only,
  parameter/ALT access).

The linker domain's OTE (Own Trap Enable, DOM header offset 0xF0) = `0x20000000`,
which has AZ (bit 24 = 0x01000000) CLEAR. So real hardware records AZ and
continues. Emulators that raise PV here turn an ignorable condition into a fatal
abort.

## Fix applied in nd500x (commit pending)

`src/cpu/nd500_mmu.c`, in the MMU translate, `capability == 0` branch:

```c
/* Null-pointer DATA access (effective address 0) = ignorable Address Zero,
   NOT protect violation. If AZ is disabled in the domain OTE, raise_trap only
   records the status bit and returns; execution continues. Map to physical 0
   (reads as 0, writes discarded). */
if (!is_instruction && virtual_addr == 0) {
    raise_trap(cpu, TRAP_AZ, cpu->PC, virtual_addr);   /* bit 24, ignorable */
    return 0;
}
/* ... unchanged: non-zero absent capability still PV ... */
```

Precondition already present in nd500x: the trap dispatcher treats bit-24 AZ as
ignorable and, when the bit is not set in OTE, sets the status bit and continues
without stopping. Only the null-pointer (address == 0) DATA case is changed;
non-zero unmapped accesses are untouched.

## APPLIED in RetroCore 2026-07-20

`Emulated.HW/ND/CPU/ND500/CpuND500.MMU.cs`, in `TranslateVirtualAddress`, right
after the capability is read (STEP 2b):

```csharp
if (!isInstruction && virtualAddress == 0 && capability == 0)
{
    RaiseTrap(TrapCondition.AZ, regs.PC, virtualAddress, "null-pointer data read (Address Zero)");
    return 0;
}
```

C#'s `RaiseTrap` already handles `TrapCondition.AZ` (bit 24) as ignorable: it
sets the status bit and returns; `CheckPendingTraps` invokes a handler only if AZ
is enabled in OTE. Without this guard the null read fell through to `PST[0]` and
raised a fatal Page Fault. Verified: `Emulated.HW` builds with 0 errors; the
ND500 MMU test suite passes 50/50 (2 skipped). No instruction files changed.

## What RetroCore should check / change (now DONE - kept for reference)

In `Emulated.HW/ND/CPU/ND500/` MMU translation (the capability lookup that today
raises Protect Violation on a zero/absent data capability):

1. If the effective DATA address is 0, raise the Address Zero condition (status
   bit 24), which is ignorable - if AZ is not enabled in the current domain's
   OTE, record it and let the access complete (read yields 0), do NOT trap.
2. Do not raise Protect Violation for an absent capability. PV is only for
   access-code violations. (Absent capability is architecturally a Page Fault;
   reclassifying that is optional and was deferred on the nd500x side too.)
3. Confirm RetroCore's trap model has an ignorable/OTE-gated path for bits 11-29
   so a disabled AZ simply continues.

This is an MMU/trap-layer change; it does NOT touch the CPU instruction set, so
no instruction implementations change on either side.

## Regression (nd500x)

- `test_instruction_validation --continue`: ALL PASSED.
- `ctest`: only the pre-existing `float_arithmetic` and `mon_calls` failures
  (mon_calls exactly its 3 known sub-failures); no new failures.
- `HELP` now runs to EXIT with 6 benign AZ reads instead of aborting.

Cross-reference: `/home/ronny/repos/nd500x/docs/HELP-CRASH-ADVANCED-CMD-REGISTRATION.md`.
