# HELP-command crash: advanced-mode command-registration leaves entry 41 unfilled

Full path of this document: `/home/ronny/repos/nd500x/docs/HELP-CRASH-ADVANCED-CMD-REGISTRATION.md`

Written 2026-07-20. Supersedes the crash-cause sections of
`/home/ronny/repos/nd500x/docs/CARVE-QUESTION-321B-UEADM.md` (UEADM) and the
"command-table walk over-reads by one" framing. Both of those were disproven by
direct measurement, recorded below so the wrong turns are not repeated.

## Symptom

The ND Linker `HELP` command (and any abbreviated command form) aborts:

```
*** ND LINKER abortion ***
```

trap PROTECT VIOLATION, `trapPC=0xB0036975`, `dataAddr=0x00000000`,
`[MMU] TRAP: No data capability! domain=1 segment=0 vaddr=0x00000000`.

`B0036975: 2D E4 14 D1  by comp2 IND(b.20)(r1),W2` dereferences a name pointer
read from a command-table entry. On the crashing run that pointer is null.

Reproduce from `/home/ronny/repos/nd500x/build/link_sandbox` (relink
`diag_linkdrive` first - it is not built by `make`, see
`/home/ronny/repos/nd500x/docs`-tracked stale-artifact note):

```
../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
  'OPEN-DOMAIN "RTEST";;HELP;;EXIT' 300000000
```

## What was DISPROVEN (do not revisit without new evidence)

1. **Not MON 321B UEADM.** Crash is identical (same instr count, same address)
   whether UEADM returns error or success. UEADM's return is never dereferenced
   by the wrapper. See `CARVE-QUESTION-321B-UEADM.md`.

2. **Not a LOOP/LOOPI/LOOPD CPU bug.** A three-way audit of the manual
   (ND-05.009.4 sections 13.4/13.5/13.6), our C
   (`/home/ronny/repos/nd500x/src/cpu/instructions/BRANCH/{Loop,Loopi,Loopd}.c`)
   and C#
   (`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/{Loop,Loopi,Loopd}.cs`)
   found increment-order, comparison operator, signedness and operand data-type
   all in agreement. The old forced-WORD width bug is already fixed. The loop
   that walks the table (`w loopi b.52,b.80` at B003685E, limit b.80 = 41) is
   walking the correct number of entries.
   - Aside (not the cause; flagged for later): all three skip setting Z/S for the
     INTEGER LOOPI/LOOPD/LOOP variants, where the manual shows `index=0 -> Z`,
     `signbit -> S` for all data types. That is a real manual divergence but
     cannot change a trip count. If ever fixed it must change BOTH emulators.

3. **Not a DOM loader bug.** The value at VA `0xB0031234` is CORRECT at load
   time. Proven with the harness watch: the first write to that word reports its
   pre-existing value as `0xB003147E`, which equals the raw DOM file byte at
   offset `0x88A34`. The loader does a plain full-size `fread` of segment 22's
   DATA (file `0x057800`, size `0x05940C`, covering the whole table), so the
   preloaded image is intact. An earlier agent's "loader bug" verdict inferred
   fault from (file != crash-time-memory) WITHOUT checking load-time memory, and
   was wrong.

## What is PROVEN (byte/measurement level, 2026-07-20)

The word is zeroed at RUNTIME by exactly one instruction, once:

```
[WATCH B0031234] B003147E -> 00000000  by PC=B0013FE7 instr=47312 B=B0001ABC
[WATCH B003123C] 00000013 -> 00000000  by PC=B0013FE7 instr=47312 B=B0001ABC
```

`ND500X_BREAK_PC=0xB0013FE7` fires exactly ONCE (count = 1). This happens at
instr ~47k, during `SET-ADVANCED-MODE` at linker init - well before OPEN-DOMAIN
(~93k) and HELP (~171k). Entry 41 is therefore an ADVANCED-MODE command slot.

Disassembly around the clear (decoded from bytes, the `.asm` dump is unreliable):

```
B0013FDA: 0C 46                 w1 :=     b.24
B0013FDC: 54 01                 w1 +      $1
B0013FDE: 6C 0C                 w1 *      $12          ; w1 = (b.24+1)*12 = 0x1EC
B0013FE0: FE 25 E0 B0 03 10 48  by2 laddr $0xB0031048(r1) ; EA = 0xB0031048 + I1 = 0xB0031234 -> I2
B0013FE7: FD 20 00 F5 00 0C     by bmove  $0,r2.0,$12   ; clear 12 bytes of slot 41
B0013FED: 4D 45                 w set1    b.20
B0013FEF: 18 42                 r:=       b.8
B0013FF1: 1A 1A 85              w move    $26,r.20
B0013FF4: 1A C4 B0 03 10 38 86  w move    $0xB0031038,r.24
B0013FFB: FE 79 C4 B0 03 10 28 87 03  w bmove $0xB0031028,r.28,$3
B0014004: C3 B0 03 1F A4 00     call      $0xB0031FA4,$0   ; ents #88 = command registration
B001400A: D2 08                 if-kgo    $8               ; skip fill on K set
B001400C: C3 B0 01 3B 67 00     call      $0xB0013B67,$0
```

At `B0013FE7`: `B=B0001ABC R=B0001B90 I1=0x1EC I2=B0031234 I3=0 I4=1 ST1=0x02`.

After the registration call, at `B001400A`: `ST1=0x00000062` => **K flag CLEAR**
(K is ST1 bit 8 = 0x100, not set). So `if-kgo` does NOT skip - the fill path
runs. Yet the watch shows `0xB0031234` is never written again. Entry 40
(`0xB0031228`) is never written at runtime at all (it keeps its correct
file-preloaded value `0xB0031472`); only entry 41 is cleared.

Registration routine (`B0031FA4`, `ents #88`) as disassembled:

```
B0031FA4: ents #88
B0031FAB: w swap  $0xB00048FD4,W1
B0031FB2: w test  W1 ; if=go $9 ; else call $0xB0031DBD (reserve/wait)
B0031FBD: w2 div4 b.20,#256,W3
B0031FC6: by2 comp $0xB00495BC ; if>>go $644
B0031FCF: jumpg  $0xB00495C0(r2)         ; <-- JUMP TABLE
B0031FD5: r:=    b.8
B0031FD7: w bmove b.28,r.20,$3
B0031FDC: w move b.24,r.32
B0031FDF: call   $0xB003A617             ; <-- likely the actual insert
B0031FE6: w test W1 ; if=go $4 ; ret
```

## The open question handed to the trace

The registration completes with K clear but never stores entry 41's name pointer
(`0xB003147E`) and length (`0x13`) back into `0xB0031234`/`0xB003123C`. The
divergence is inside `B0031FA4` / `B003A617`. Candidate mechanisms, unresolved:
- a jump-table (`jumpg $0xB00495C0(r2)`) or `div4`/`comp` result sends control
  down an arm that skips the store;
- the store runs but computes a wrong effective address (writes elsewhere);
- the name pointer/length is computed as 0 before the store.

Whatever single instruction is found to diverge from the manual, the fix must be
mirrored in RetroCore C#
(`/mnt/e/Dev/Repos/Ronny/RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/`).

## ROOT CAUSE (manual-grounded, 2026-07-20 later): wrong trap type on address-0 read

The registration-refill theory above is DISPROVEN by trace: the slot-41 clear is
deterministic and real-HW-identical, the command-registration for command 26
routes (via `jumpg`) to an arm that legitimately does not refill the table, and
the sort bound 41 is a runtime-built value (not in the DOM file). The linker's
own code therefore walks to the cleared slot 41, whose name pointer and length
are both 0, and its byte-compare guard (`if greater, skip`) does NOT skip when
both lengths are 0 - so it dereferences the NULL name pointer, reading DATA
virtual address 0x00000000.

Reading data address 0 is NOT a Protect Violation on real ND-500. Per the
ND-500 Reference Manual (ND-05.009.4):

- p73 (line ~2181): "An address equal to zero will cause an Address Zero trap
  condition. ... A jump to address zero will also cause an AZ trap condition.
  The AZ bit is set/reset for each instruction with memory access."
- AZ is trap-condition bit 24, and is IGNORABLE and handled AFTER instruction
  execution: p63 "Ignorable trap conditions do not require any handling; they
  may be disabled and will have no effect on program execution." So if AZ is not
  enabled in the domain's Own Trap Enable (OTE), the condition is merely recorded
  and execution continues.
- PV (Protect Violation, status bit for access-code violations) is reserved for
  write-to-read-only and parameter/ALT access-code violations (p45/p75). An
  ABSENT (zero) capability is instead a Page Fault: p75 "Page fault is also
  caused if a memory management table lookup gives zero as result." There is no
  separate "capability not present" trap.
- Segment 0 is a normal segment; there is NO architectural zero page and no
  documented default segment-0 capability. The loader's FORTRAN-500 `DC[0]`->seg1
  alias (ndlib_dom_loader.c:477) has no manual backing and is NOT the fix.

VERIFIED precondition: the linker domain's OTE1 (DOM header offset 0xF0) =
0x20000000 (only bit 29). AZ = bit 24 = 0x01000000 is CLEAR. So AZ is disabled
for this domain -> on real hardware the null read records AZ and continues.

Our MMU (`src/cpu/nd500_mmu.c` ~line 226) instead raises PV
(`trap_protect_violation`) for any zero data capability, turning the ignorable
AZ condition into a fatal abort. That is the bug.

## FIX APPLIED 2026-07-20 (small-scope, verified)

`src/cpu/nd500_mmu.c`, inside the `capability == 0` branch of
`nd500_mmu_translate_domain`: a DATA reference whose effective address is 0 now
raises the ignorable Address Zero condition instead of a Protect Violation:

```c
if (!is_instruction && virtual_addr == 0) {
    raise_trap(cpu, TRAP_AZ, cpu->PC, virtual_addr);
    return 0;   /* map to physical 0 - reads as 0, writes discarded */
}
```

`raise_trap()` already implements the manual's semantics: `TRAP_AZ` (bit 24) is
in `TRAP_IGNORABLE_MASK`, so when it is not enabled in the domain's OTE it only
sets the status bit and returns WITHOUT stopping (cpu.c:404-414). The linker
domain's OTE1 = 0x20000000 has AZ (bit 24) clear, so execution continues - the
only behaviour change is the previously-fatal null read is now benign.

NON-zero absent-capability accesses are UNCHANGED (still PV). The manual-correct
PV->PGF reclassification for those was deliberately deferred (bigger blast
radius) and is NOT part of this fix.

### Verification
- HELP no longer aborts: `OPEN-DOMAIN "RTEST";;HELP;;EXIT` now runs all rounds to
  EXIT (instr 273764) with 6 benign AZ reads (the quicksort comparing the empty
  slot); previously it trapped at instr 185430.
- `test_instruction_validation --continue`: ALL TESTS PASSED.
- `ctest`: 16/18 - only the pre-existing `float_arithmetic` and `mon_calls`
  failures (mon_calls still exactly its 3 known sub-failures: 312B, 256B x2);
  zero new MMU-trap noise.

Adjacent note (not changed): `raise_trap` in cpu.c has an UNCONDITIONAL
`[DEBUG] raise_trap: ...` fprintf, so every AZ read now prints a debug line (6
per HELP). Pre-existing behaviour for all traps; left alone.

## Original proposed fix (superseded by the APPLIED section above)

In `src/cpu/nd500_mmu.c`:
1. A DATA reference whose effective (virtual) address == 0 should raise the AZ
   condition (status bit 24). AZ is ignorable: if bit 24 is not enabled in the
   current domain's OTE, do NOT trap - record it and allow the access to
   complete. UNVERIFIED detail: the byte value returned by a completed read at
   address 0 is not defined by the manual; returning 0 is the pragmatic choice
   and is sufficient for the sort to proceed (the exact value cannot change the
   "does not crash" outcome).
2. An absent/zero capability (non-zero address, unmapped segment) should raise
   PGF, not PV. This is a correctness item; it is NOT what fixes HELP (the HELP
   read has EA == 0, so path 1 applies).
3. Reserve `trap_protect_violation` strictly for write-to-read-only and
   parameter/ALT access-code violations.

This fix is entirely in the MMU/trap layer - it does NOT touch the CPU
instruction set, so RetroCore's `Instructions/` are unaffected. If RetroCore has
the same PV-on-absent-capability behavior it needs the same AZ/PGF correction in
its MMU, tracked as a separate handoff.

## Impact / workaround

Non-pipeline-blocking. Full-form commands (e.g. `REFER-ENTRY`) match earlier in
the table and never reach slot 41. Only `HELP` and abbreviated command forms
walk to the end and hit the unfilled slot. The end-to-end C compile+link pipeline
does not use HELP and is unaffected.
