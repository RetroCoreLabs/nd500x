# Handoff: align RetroCore (C#) with nd500x (C) - CPU/instruction changes

> ## STATUS UPDATE 2026-07-30 (re-checked against the live RetroCore tree)
>
> Most of this document is now DONE on the C# side - do not re-do it. Verified
> present in `$RETROCORE/Emulated.HW/ND/CPU/ND500`:
>
> | item | C# status |
> |---|---|
> | `InstructionAborted` flag exists (the doc's "primary alignment question") | YES - `CpuND500.Trap.cs` |
> | 16/32-bit read splits stop at first fault | DONE - `CpuND500.Memory.cs:341-344` |
> | 32-bit write split stops at first fault | DONE - `CpuND500.Memory.cs:405-409` |
> | 64-bit access | DONE - a real `ReadVirtualMemory64` that stops between halves |
> | `Jumpg` | DONE - guarded, cites `nd500x BRANCH/Jumpg.c:174` |
> | `Jumps` | DONE |
> | `Loop` / `Loopi` / `Loopd` | DONE |
>
> **STILL MISSING - the one real gap: `Instructions/CALL/Ret.cs`.**
> Zero occurrences of `InstructionAborted`. It reads the caller's frame at
> lines 59-60:
> ```csharp
> uint prevB   = ReadMemory(regs.B + 0, 4);  // B.PREVB
> uint retAddr = ReadMemory(regs.B + 4, 4);  // B.RETA
> ```
> and then commits at lines 109-111:
> ```csharp
> regs.P = retAddr;  regs.L = retAddr;  regs.B = prevB;
> ```
> The frame lives on the pageable USER STACK, so either read can fault; RET is
> on one of the hottest paths in the machine. C fixed this in `c77a8fd`
> (`src/cpu/instructions/CALL/Ret.c`, guard placed
> immediately after the two reads, before the domain-boundary logic).
>
> Also still open on the C# side: `Call.cs` / `Callg.cs` operand-read guards
> (see Change 3 - explicitly UNPROVEN, judge on merit), and the PRT trap
> (see the separate note at the end of this update).
>
> **PRT (Programmed Trap, bit 29) - new finding, affects both emulators.**
> It is how NDIX delivers ALL pending signals, profiling ticks and rescheduling.
> In C it is defined (`cpu_protos.h:218`) but never raised: `check_pending_traps`
> masks pending traps with OTE only, while NDIX arms PRT through `pcb_mte1`
> (MOTHER trap enable) at `machine/machdep.c:1273`. Consequence: a process that
> should die from SIGSEGV instead refaults forever and halts the machine.
> Full trace in the project task list; check whether RetroCore has the same
> OTE-only assumption in its pending-trap path.


For the validation LLM that cross-checks instruction semantics between the two
ND-500 implementations. All paths absolute.

- C emulator: `<nd500x repo root>`
- C# emulator: `$RETROCORE/Emulated.HW/ND/CPU/ND500`
- Microcode oracle (period-correct, independent of both):
  `$ND5000UC/microcode/MICRO-5800-B30.md` (decoded microwords)
  and `$ND5000UC/docs/MC/MICRO-5800-B30.LABE` (label -> address)
- Independent opcode/encoding oracle (1988 dbx table):
  `$NDIX/baseline/ucb/dbx/ops.nd500`

## THE THEME: an instruction that faults must ABORT, not finish

Every item below is one bug class. On the ND-500 a memory access can page-fault
at any point inside an instruction. In BOTH emulators the fault is dispatched
SYNCHRONOUSLY from inside the access helper: the trap handler PC is installed,
`in_trap_handler` is set, and the global trap state is CLEARED. Control then
returns to the middle of the instruction implementation, which happily carries
on with garbage and commits results - including, fatally, overwriting the
freshly installed handler PC.

The C side has a per-instruction abort flag for exactly this,
`cpu->instr_aborted` (documented at
`src/cpu/cpu_protos.h:145-151`). The correct guard
after ANY operand/memory access that can fault is:

```c
if (nd500_trap_occurred() || cpu->instr_aborted) return;
```

`nd500_trap_occurred()` alone is NOT sufficient, because a dispatched handler
clears the trap state - that is the whole reason `instr_aborted` exists.

**Primary alignment question for you: does RetroCore have an equivalent flag at
all?** I could not find one. If it does not, that is the single biggest
divergence and everything below is a symptom of it.

## Change 1 - split memory accesses must stop at the first fault

C commit `edcaf98` in `<nd500x repo root>`, file
`src/cpu/instruction_helpers.c`. New helper
`nd_split_faulted()` applied at 12 sites.

Mechanism: a split access performed its REMAINING parts after an earlier part
faulted. Because the fault was already dispatched, the next part ran from
INSIDE the handler - a double fault, which cannot nest, so the machine halted.
Signature: the halt is reported at the LAST byte/word of the access, never at
the first one that actually faulted.

Two sub-cases:

1a. **Page-straddle splits** - 8 sites (read/write 16/32, plain and `_domain`).
    Killed the native C front-end `cc1` at `0x15801` (= `0x157FE`+3) in
    `pftn.c`'s `stab[]` init loop (stride 49, so unaligned by construction).

1b. **64-bit accessors** - 4 sites (read/write 64, plain and `_domain`). These
    are two 32-bit halves that normally sit in the SAME page, so this is NOT
    the straddle case. Killed the native assembler `as` at `PC=0x4799`: the
    fault at `0x13990` was dispatched, then the machine halted on `0x13994`.

Restart-safety: stopping at the first fault is correct because the access
re-executes in full after `pagein` returns; bytes already committed to the
first page are rewritten identically.

### RetroCore status - BOTH sub-cases appear PRESENT (unfixed)

`$RETROCORE/Emulated.HW/ND/CPU/ND500/CpuND500.Memory.cs`
already has the straddle split (`CrossesPageBoundary` at line 269, used at
lines 302, 330, 359, ...) - it was ported from the earlier C fix - but the
split loops have NO abort check between bytes. E.g. `ReadVirtualMemory32`
lines 329-336:

```csharp
if (CrossesPageBoundary(virtualAddress, 4))
{
    uint v = 0;
    for (uint i = 0; i < 4; i++)
        v = (v << 8) | (ReadVirtualMemory(virtualAddress + i) & 0xFF);
    return v;                      // <-- no fault check inside the loop
}
```

The 64-bit idiom is at
`$RETROCORE/Emulated.HW/ND/CPU/ND500/Instructions/InstructionHelpers.cs:627-629`:

```csharp
uint high = cpu.ReadVirtualMemory32(address);
uint low  = cpu.ReadVirtualMemory32(address + 4);   // <-- no fault check between halves
return ((ulong)high << 32) | low;
```

There is no `ReadVirtualMemory64`/`WriteVirtualMemory64` in RetroCore; the
pair-of-32 idiom is used directly, so every such site needs the guard, not just
one helper. Please enumerate them.

## Change 2 - JUMPG must not overwrite the handler PC (THE self-hosting fix)

File `src/cpu/instructions/BRANCH/Jumpg.c`. NOT yet
committed at the time of writing.

`JUMPG` (opcode `0x00B4`) reads its absolute target from DATA memory. That read
can page-fault. The implementation then unconditionally did `cpu->PC = target`,
overwriting the handler PC that the dispatch had just installed - with a
garbage target read from an unmapped page. The CPU executed that address in
KERNEL context, and the `ENTS` found there raised a FALSE Instruction Sequence
Error, because trap dispatch had cleared the CALL/ENT sequence interlock.

Real-world symptom: the native assembler halted with
`[TRAP] ENTS at PC=0x0001595E: Must be preceded by CALL/CALLG`.

Evidence that identified it (reproduce this way if you need to re-derive it):
set `ND500X_STOPDBG=1` and read the PC ring, which now also records domain:

```
254: 0x0000309D  CED=6 CAD=0 inH=0     <- user, the JUMPG; operand faulted at 0x1E28
255: 0x0001595E  CED=0 CAD=6 inH=1     <- kernel, inside handler, but NOT the handler PC (0x381)
```

Fix verified: NDIX now assembles, links and runs a program entirely on the
ND-500 (`as` exit 0, `ld` exit 0, program prints its output).

### RetroCore status - SAME DEFECT, CONFIRMED

`$RETROCORE/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/Jumpg.cs:52-55`:

```csharp
uint address = (uint)ReadOperandValue(fi.Operands[0], DataType.W, 4);
regs.P = address;                  // <-- unconditional, no fault check
```

**This is not a JUMPG-specific bug.** Any instruction that (a) reads an operand
from memory and (b) then writes `PC`/`regs.P` or commits a destination has it.
Please sweep ALL instructions in both trees for that pattern - the two I
hardened below were found by inspection, not by failure, and I expect more.

## Change 2b - the rest of the sweep: JUMPS, RET, LOOP/LOOPI/LOOPD

Commits `c77a8fd` and `058d528`. Found by sweeping the C instruction set for
Change 2's pattern, so treat these as the completed version of step 4 in
"Suggested order of work" below - for the C tree only. RetroCore still needs
its own sweep.

- `BRANCH/Jumps.c` - byte-for-byte the same shape as JUMPG.
- `CALL/Ret.c` - reads PREVB and RETA from the caller's frame, which lives on
  the pageable USER STACK, then commits PC, B and L. RET is on one of the
  hottest paths in the machine, so this is the most likely of the set to have
  been biting in practice.
- `BRANCH/Loop.c` (float and integer paths), `Loopi.c`, `Loopd.c` - read
  index/step/limit, write the updated index back, and modify PC.

The ~20 conditional-branch files that matched a crude grep for "writes PC" are
FALSE POSITIVES: they take a PC-relative displacement out of the instruction
stream and touch no data memory. Do not "fix" them.

### Correction, and a genuine C#-vs-C divergence to check

The commit message for `058d528` claims the LOOP family "corrupts guest data as
well as clobbering the handler PC". **That is wrong for the C tree** and the
message is left uncorrected only because another session is working in that
repo. In C, memory-operand writes go through
`nd500_write_operand_value` -> `nd500_write_memory_*_domain`, and those helpers
ALREADY refuse to commit when a fault is pending
(`src/cpu/instruction_helpers.c:189`,
`src/cpu/cpu_instr.c:85`). So the write-back was
already blocked; only the PC clobber was real.

**This is exactly the thing to check in RetroCore.** The C guard lives in the
write helper, not in each instruction. If RetroCore's equivalent
(`CpuND500.Memory.cs` `WriteVirtualMemory*`) does NOT refuse to store while a
fault is pending, then C# genuinely does have the data-corruption bug that C
does not, and the per-instruction guards are not sufficient there. Establish
which model RetroCore uses before porting anything:
- C model: guard centrally in the write helpers, plus per-instruction guards
  ONLY where the commit is something the helpers cannot protect - i.e. PC,
  registers and status flags.
- Register/flag commits are self-correcting in C because the instruction
  re-executes in full after the handler returns and overwrites them. PC is the
  exception, and the reason this whole bug class exists: writing PC destroys
  the restart itself.

## Change 3 - CALL/CALLG operand-read guards (HARDENING, UNPROVEN)

Files `src/cpu/instructions/CALL/Call.c` and
`Callg.c`. NOT committed. **Be sceptical of these.** I added them while chasing
the ENTS bug on a theory that turned out to be WRONG (I believed the failing
`ENTS` was reached via a `CALL`; it was reached via the `JUMPG` above). They fix
no observed failure. I kept them only because they are the same latent pattern
as Change 2 and because an identical guard already existed a few lines further
down in both files, for the entry-point opcode fetch:

```c
uint8_t entry_opcode = nd500_fetch_memory_8(cpu, resolved_addr);
if (nd500_trap_occurred() || cpu->instr_aborted) { return; }
```

The new guards sit right after the subroutine-address and arg-count operand
reads (`Call.c` ~line 48-51, `Callg.c` line 172-175). Validate them on their
merits; if RetroCore's `Call.cs`/`Callg.cs` already handle this differently,
prefer whichever matches the microcode.

## Change 4 - PC ring records domain (DIAGNOSTIC ONLY, no semantics)

`src/cpu/cpu.c` - the `ND500X_STOPDBG` ring now stores
`CED`, `CAD` and `in_trap_handler` alongside each PC. Purely diagnostic, but it
is what made Change 2 findable: without the domain column, a user PC and a
kernel PC look identical in the dump. **Strongly recommend porting the
equivalent to RetroCore's trace** (`CpuND500.Trace.cs`) - no amount of source
reading substituted for it.

## Changes by the OTHER session (validate these too, not mine)

- `5dc738f` - "save/restore the CALL/ENT sequence interlock across trap
  dispatch". Adds `trap_saved_pending_call_*` to the trap context, saved in
  `invoke_trap_handler` (`src/cpu/cpu.c` ~1256-1271)
  and restored in `src/cpu/instructions/CALL/Rett.c:386`.
  Ships a 13-assertion test `test_ents_pending_call_trap`.
  **I validated this independently and it is CORRECT but it did NOT fix the
  reported bug** (Change 2 did). Microcode confirms the interlock is real
  hardware: `CALL` @`000646` and `CALLG` @`000652` both carry `C,SEQ`; `ENTD`
  @`000660` carries `C,SEQ` and branches `COND,MZRO` -> `ENT_SEQ_ERR` @`000661`
  -> `INS_SEQ_ERR` @`003141`; `ENTT1` @`014042` carries `C,SEQ`, i.e. the
  interlock IS part of trap-saved context. So RetroCore needs the same
  save/restore - check whether it models the interlock at all.
- `25859c0` - `Getbi`/`Putbi` clear S/C/O per rule 4040 (all data status bits
  not mentioned are cleared; microcode `GET_BIT_1` runs `ST,SAVA`).
- `c2211ce` - `RIOM` operand 3 (count) is a WORD, not the instruction prefix
  type. Affects both the read width AND the post-index scale. This one is
  explicitly documented as mirrored FROM RetroCore's `Riom.cs` /
  `Instructionset.Init.cs`, so C# should already agree - confirm it does.

## Regression gates (must stay green in the C tree)

```
build/bin/test_page_straddle            # 18/18
build/bin/test_double_register          # 48/48
build/bin/test_ents_pending_call_trap   # 13/13
cd $PCC_ND500 && make test                # 21/21
ctest --test-dir build -R nc_           # 4/4
```

Plus the end-to-end guest check, which is the one that actually matters:

```
cd <nd500x repo root> && ./run-ndix.sh      # login: root, no password
cd /tmp; /lib/as -o h.o /usr/src/hello.s; echo AS=$?
/lib/ld -e start -x -o hw /lib/crt0.o h.o /lib/libc.a; echo LD=$?
./hw
```
Expected: `AS=0`, `LD=0`, then `HELLO-FROM-NATIVE-ND500-TOOLCHAIN` / `2+2 = 4`.

Scripted driver (handles login):
`tools/ptyboot.py`

## Suggested order of work

1. Determine whether RetroCore has a per-instruction abort flag. If not, add
   one - everything else depends on it.
2. Port Change 2 (`Jumpg`) - it is the one with a proven end-to-end failure.
3. Port Change 1 (split accesses, both sub-cases) - enumerate every
   pair-of-32 site, there is no single 64-bit helper to fix.
4. Sweep both trees for "reads memory operand, then commits PC or destination
   without checking for a fault". Treat the three fixed sites as examples of a
   pattern, not as the complete list.
5. Cross-check the interlock modelling (`5dc738f`) against the microcode
   addresses quoted above.

## Constraints

- `$NDIX/kernel/` and `$NDIX/baseline/` are
  READ-ONLY historical preservation - never edit, a hook blocks it.
- Writable: `<nd500x repo root>`, `$PCC_ND500`,
  `$RETROCORE`.
- Do not kill or restart processes without the owner's approval.
- No Unicode in C sources or comments.
