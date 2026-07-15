# NC compile crash (0x080241FC / "T1-B") - HANDOFF

**Full path:** `/home/ronny/repos/nd500x/docs/NC_CRASH_HANDOFF.md`
Date: 2026-07-13. Companion to the running log `NC_CRASH_0x08023EA4_ROOTCAUSE.md` (UPDATES 1-41).

## One-line status
NC (nc-a06.dom) compiling B.C crashes deterministically at **0x080241FC, instr 1,230,739**
(`CHECK B,B,B` from `build/nc_sandbox`) via a **use-after-free of a global "mailbox" pointer**.
Root traced end-to-end; NOT yet fixed. No single mis-executed instruction found - every flag,
branch, and instruction on the path verifies correct against operands.

## The crash chain (verified)
1. Global `0x08000200` is a scratch mailbox. At instr 1,110,309 it is set to list `0x1802A1B0`
   (valid at the time).
2. A concat (routine 0x08008A7C -> 0x08024829) reads the mailbox as an input, **frees its
   buffer** (freelist push 0x0802CEFE via wrapper 0x0802CEB3), stores the combined result to a
   DIFFERENT global 0x08010680, and **never clears/updates 0x08000200** -> mailbox dangles.
3. Cell 0x1802A1B0 is freed+reallocated (LIFO realloc) and by 1,142,883 is rebuilt as a
   **type-3 object** (constructor 0x0801FEAD writes tag byte 0x03 at offset 0).
4. At instr 1,193,163 the reader routine 0x08005024 reads the stale mailbox (0x08000200 =
   0x1802A1B0) into a local (PC 0x08005044, read-then-refresh pattern - it refreshes the
   mailbox to a fresh cell 0x18003000 AT 0x08005106, but AFTER the stale read).
5. The reader combines it (routine ...0x080050C9 -> concat 0x08024829). The count routine
   reads `count = mem16[recordA] + mem16[recordB]`; recordA = the type-3 object, so
   `mem16 = 0x0300` (tag) is read as a count -> `0x0300 + 2 = 0x0302 = 770` (instr 1,196,914).
6. It allocs a 770-entry list, the walk (0x0802411F laddr r1.(-2); walk 0x0802412A) reads one
   halfword before the filled region (base -2) and hits the 0xF0F0 GETB poison -> PV 0x080241FC.

## What is RULED OUT (with evidence)
- UEADM/312B MOINF: real bug found + fixed (312B now reports 321B available; see below), but it
  only moves the crash a few instrs - NOT this trigger.
- Store-width: 0x0802484F halfword store is correct by design (into 0xF0F0F0F0 GETB poison).
- The -2 walk base (0x0802411F laddr r1.(-2)): the -2 is in the REAL binary (FD 3C FC FF FF FF FE)
  and decoded identically. Not an emulator base bug.
- Use-after-free "read of freed slot": at the fatal read the cell is LIVE (a type-3 object).
- Flags/branches: verified 6+ branches (free-path x3, both flip-finder hits, comp2 path) - all
  correct vs true operands. Flip-finder over 930 branches only "avoids" the crash by derailment
  (not diagnostic). ST1 (not FLAGS) holds the status flags - branches follow ST1 correctly.

## Fixes made this session (in working tree, review before commit)
- `src/cpu/instructions/COMPARE/Comp2.c`: F/D COMP2 now compares as IEEE floats (was integer
  math on float bits). Real bug; not this crash's trigger.
- `src/libmon/handlers/mon_312B_CheckMonCall.c`: returns non-zero (real GOTAB[321B]=0o112376)
  for MON 321B so NC believes UEADM is available.
- `src/libmon/handlers/mon_321B_UEAdministrator.c`: returns SUCCESS (was deprecated-error).
  (These MON fixes are correct per authoritative SINTRAN L carvings but do not clear the crash.)

## The open question (why it diverges from real HW)
The reuse of cell 0x1802A1B0 is deterministic and appears FAITHFUL to NC's own code. Since both
emulators are the same author's (shared bugs - NOT a usable reference), and there is no
real-hardware trace, the true divergence (a subtle instruction/MON result upstream that shifts
allocator timing, OR NC's own latent UAF that real HW tolerated) cannot be pinned by tracing
alone. **An oracle for "correct" is needed** (ND-500 manuals per-instruction, or a real trace).

## Bounded audit surface (for the manual-audit route)
- **171 unique instruction opcodes** executed before the crash.
- **930 unique conditional-branch sites** executed (4131 static in the whole binary).
- Freelist/alloc code to audit first: wrapper `0x0802CEB3` (size-bucketed, uses FLOAT alog2/
  dconv/shl-by-62/int/add2/byconv to compute the bucket), GETB pop `0x0802CF08`, push
  `0x0802CEFE`, head table at `0x801D544`.

## How to reproduce / drive (CRITICAL gotchas)
- Run harnesses FROM `build/nc_sandbox` (has GUEST/ + SCRATCH/). From elsewhere A:C/B:C don't
  resolve -> NC takes a different path and crashes at 0x08023EA4 (a DIFFERENT crash).
- Always `ND500X_PIN_CLOCK=1` for deterministic runs.
- Diag harnesses are standalone C in `test/diag_*.c`, gcc-built (NOT by make) against
  `build/lib/*.a`; rebuild them after any lib change. Link line:
  `gcc -O2 -o build/bin/NAME test/NAME.c -Wl,--start-group build/lib/libnd500_debugger.a
   build/lib/libnd500_cpu.a build/lib/libnd500_machine.a build/lib/libmon.a
   build/lib/libnd500_ndlib.a build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm`
- Useful existing harnesses: diag_nc_checkpoints (crash point), diag_monlog (MON trace, needs
  mon_log_enable(1)+set_level), diag_listbuilder (count/allocBase), diag_uaf (free/alloc
  timeline), diag_global/diag_producer (mailbox writes), diag_branches (branch+ST1), diag_flipfind
  (branch flip sweep - has a state-leak between in-process runs; single-target mode is reliable).

## Next-day options
1. Audit the 171 opcodes / freelist float-bucketing against the ND-500 manual (only oracle route).
2. Add a defensive guard (validate record type-tag before reading mem16 as a count) to get PAST
   this crash and expose the next blocker.
3. Obtain a real-hardware / independent reference trace to diff allocator behavior.
