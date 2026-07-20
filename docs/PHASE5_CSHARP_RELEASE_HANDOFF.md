# Phase 5 release handoff to the C# emulator (RetroCore)

Full path of this document: `/home/ronny/repos/nd500x/docs/PHASE5_CSHARP_RELEASE_HANDOFF.md`

Written 2026-07-20, at the point where the end-to-end C compile+link+run pipeline first
worked under nd500x.

Target repository: `/mnt/e/Dev/Repos/Ronny/RetroCore/`

---

## 0. The milestone this release corresponds to

A real C program now goes all the way through, entirely under emulation:

```
B.C  ->  NC (nc-a06.dom)  ->  .CAT  ->  CAT-500 (cat-cat5-b06.dom)  ->  B.NRF
     ->  ND Linker (linker-b01.dom)  ->  BPROG.DOM  ->  runs, exits cleanly
```

Source `/home/ronny/repos/nd500x/build/nc_sandbox/GUEST/B.C`:

```c
#define VALUE 42
int x;
main()
{
 x = VALUE;
}
```

Result: 15374 instructions, the C runtime prints its termination banner, exit via MON 0B
LEAVE. Verified correct rather than merely non-crashing - the linker places `X` at data
address `114B D01`, and after the run memory at `0x0800004C` reads `00 00 00 2A`, i.e. 42.

Reproduce from `/home/ronny/repos/nd500x/build/link_sandbox`:

```
rm -f GUEST/BPROG.DOM
../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
 'OPEN-DOMAIN "BPROG";;LOAD B;;LOAD NC-LIB;;LOAD CAT-LIB;;\
  DEFINE-ENTRY stack-space,400000,d;;DEFINE-ENTRY heap-space,400000,d;;\
  REFER-ENTRY stack-space,rts_stack_size,d,d;;\
  REFER-ENTRY heap-space,rts_heap_size,d,d;;CLOSE;;EXIT' 500000000
```

Use `REFER-ENTRY` in full - the abbreviation `REFER` crashes the linker for a reason that is
ours, see section 5.

---

## 1. Already synced - no action needed

| Change | nd500x commit | RetroCore commit |
|---|---|---|
| LOOP / LOOPI / LOOPD index uses the instruction data type, not forced WORD | `a3047b1`, `c7de3ba` | `2a6bf1925` |

Detail in `/home/ronny/repos/nd500x/docs/HANDOFF_LOOPI_INDEX_DATATYPE.md` and
`/home/ronny/repos/nd500x/docs/HANDOFF_LOOP_LOOPD_DATATYPE_AND_ZEROFILL.md`. The latter also
records the ND-5800-B30 microcode confirmation (`TYP,DR` = "data type controlled by ICA") and
the matching generator fix in `ComprehensiveBranchGenerator.cs`.

---

## 2. To adopt: MON 256B DEABF returns K CLEAR on success

nd500x commit `bce9562`. Full detail:
`/home/ronny/repos/nd500x/docs/HANDOFF_256B_DEABF_K_FLAG_POLARITY.md`.

The handler had been forcing `K = 1` on the success path. The polarity is inverted: SINTRAN's
generic MON trampoline treats K as the ERROR flag, so success must leave K clear. Evidence is
byte-level, from the linker's own code: the generic trampoline at `B004D9E6-B004DA5B`, with
the error arm at `B004DBF1` and the success arm at `B004DBED`, plus the call sites at
`B0000A6D` / `B0000A76`.

`I1 = 0` on success is unchanged and still required.

Symptom if wrong: the ND Linker's `LOAD` command re-prompts in a loop.

---

## 3. To adopt: MON-connected segments must be demand-grown in PS_ADI

nd500x commit `94a603c`. Full detail:
`/home/ronny/repos/nd500x/docs/HANDOFF_SEGMENT_DEMAND_GROWTH.md`.

A file connected as a segment (MON 412B FSCNT) cannot be a flat pre-sized allocation. The ND
Linker connects a freshly created `:DOM` that is 4096 bytes at connect time and then writes
megabytes into it. nd500x now builds the segment as a two-level PS_ADI mapping with an empty
L1 table and grows pages at page-fault time, hooked in at BOTH PS_ADI miss points (the L1 miss
and the L2 miss).

One subtlety that cost real time and is easy to get wrong in a reimplementation:

> The L1 entry must stay writable regardless of the segment's data protection. PS_ADI checks
> BOTH levels' protection bits on a write, so a read-only L1 entry denies writes to every page
> beneath it.

---

## 4. To adopt: connected segments must be written BACK to the host file

nd500x commit `f201648`. Full detail:
`/home/ronny/repos/nd500x/docs/HANDOFF_412B_SEGMENT_WRITEBACK.md`.

"Connect a file as a segment" is a MAPPING, not a copy. The program reads and writes the FILE
through ordinary memory accesses to the segment. The ND Linker builds an entire `:DOM` this
way and never issues a single MON 120B WFILE for the domain body - so without a write-back the
linked domain came out as 6,312,168 bytes containing 8 non-zero bytes.

Flush on both MON 413B (explicit disconnect) and MON 43B (closing a file that is still
connected), before dropping the mapping. Only pages that are actually mapped are written, so a
demand-grown segment keeps the sparse shape the program built.

**Return contract matters** - get this wrong and the emulator lies in its own logs:

| return | meaning | what the handler may print |
|---|---|---|
| `1` | flushed | "segment N written back to PATH" |
| `0` | nothing to flush (not tracked, or mapped read-only) | say nothing |
| `< 0` | write error | "write-back FAILED" |

The first version returned 0 for both "skipped" and "success", so read-only input libraries
that were never touched were reported as written back. Keying the guard on the mapping's
`writable` flag is what makes the distinction correct.

---

## 5. Known-open items - do NOT port a fix for these yet

### 5a. MON 321B UEADM is a stub, and that stub crashes the ND Linker

Both `HELP` and any abbreviated command form (e.g. `REFER` for `REFER-ENTRY`) make the linker
print a register block and `*** ND LINKER abortion ***`, trap 7644B PROTECT VIOLATION at
`0xB0036975`.

Root cause is ours: 321B returns an error, the linker's wrapper at `B004D383` branches to its
`ret` WITHOUT reloading `w1`, so our injected error code 124 travels on as a legitimate result
and eventually becomes a null string descriptor that the linker's own unguarded byte compare
dereferences.

Do not invent semantics for it. The full evidence chain and the exact open questions are in
`/home/ronny/repos/nd500x/docs/CARVE-QUESTION-321B-UEADM.md`.

Note for both emulators: "DEPRECATED" is an emulator classification, not a documented fact.
The carve shows `MCTAB[321B] = 065453B = UEADM` with a real worker body in `003-S3CP`.

Also: `312B MOINF` carries a deliberate hard-coded exception reporting 321B as PRESENT
(`065453B`) because the NC front-end requires it. Both emulators must keep returning that
value. Changing 321B's own registration must not change the 312B answer.

### 5b. The NRF BEG language byte says FORTRAN, and that is normal

Our NC/CAT-500-produced objects start `BEG NL=2 N=00 01`, i.e. `language=1 (FORTRAN)` where
Appendix D puts C at 8. Do not "fix" this. Language histograms over the vendor libraries show
CAT-500 stamps 1 on everything it emits, including the genuine vendor-built C standard
library:

| File | Languages |
|---|---|
| `NC-LIB-A06.NRF` (the C stdlib) | 95 FORTRAN, 9 PLANC |
| `CAT-LIB-B06.NRF` | 442 FORTRAN |
| `USLIB3.NRF` | 28 FORTRAN, 2 PLANC |
| `COBOL-85-LIB-K01.NRF` | 344 PLANC, 8 ASSEMBLY |
| `debugger-b.nrf` | 22 PLANC, 1 ASSEMBLY |

Consequence: at `CLOSE` the linker selects its auto-job by the language of the module holding
the main start address and so hunts `LINKER-AUTO-FORT:JOB`, warning "No such file name
(0000:56)". Expected and harmless, because the C auto-job's commands are issued manually.
Do NOT stage `linker-auto-fort.job` to silence it - that job sets 16 trap conditions and then
`SPECIAL-LOAD (SYSTEM)FORTRAN-LIB`, i.e. it would pull the FORTRAN runtime into a C program.

Still unverified: what a genuine vendor C *main* module (one carrying an MSA group) is
stamped. None of the library modules above carries an MSA, so the corpus cannot settle it.

---

## 6. Shared tooling now available

`/home/ronny/repos/nd500x/tools/nrfdump.py` decodes ND Relocatable Format sequentially, and
`/home/ronny/repos/nd500x/tools/nrfcheck.py` verifies module checksums. Both are written from
Appendix D of the linker manual and validated against six vendor NRFs. Useful on the C# side
for exactly the same job: proving what a compiler actually emitted rather than inferring it.

Two gotchas encoded in the tool and worth knowing independently:

- `LDN`'s numeric field is a byte COUNT and must be read UNSIGNED. Read signed, any count
  `>= 0x80` rewinds the parse position and the reader loops on garbage - which looks exactly
  like a corrupt file. This is what made me briefly and wrongly conclude the recovered C
  libraries were damaged.
- Dumps of these files contain high bytes, so plain `grep` treats the output as binary and
  silently prints nothing. Use `grep -a`.

---

## 7. Regression baseline at this release

| Check | Result |
|---|---|
| `test_instruction_validation --continue` | ALL TESTS PASSED |
| `ctest` | 16/18 (`float_arithmetic`, and `mon_calls` with exactly 3 sub-failures - both pre-existing) |
| `diag_fscnt` | PASS: mapped segment bytes match the file |
| End-to-end C pipeline | PASS, x == 42 |

---

## 8. Process note that cost this project more time than any real bug

Five separate investigations were spent on STALE artifacts. Two of them were this session,
including a full session-length hunt for a "linked program calls address 0" that simply did not
exist in current code.

Two independent staleness sources, neither of which announces itself:

1. Hand-linked diagnostic harnesses are NOT built by `make`. `test/diag_linkdrive.c` and
   friends must be relinked manually after every build or they silently run old library code.
2. Output files in the sandboxes persist across sessions.

Before forming any hypothesis: `rm -f` the output and regenerate it, and check the diag binary
timestamps against the libraries. If a symptom looks impossible given the source you just read,
staleness is the first suspect, not the last.
