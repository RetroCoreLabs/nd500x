# HANDOFF: NDIX `resume()` jumps to garbage (unmapped u-area), 2026-07-31

Paths below are **relative to the nd500x repo root** unless marked otherwise.
The NDIX tree is outside this repo; it is referred to as `$NDIX` (export it, e.g.
`export NDIX=<your NDIX-C checkout>`). Guest kernel sources live at
`$NDIX/kernel/MASTER/`.

---

## 1. What is DONE and COMMITTED (do not redo)

### 1.1 CALL/ENT* sequence interlock — `nd500x 1260f5c`, `RetroCore fa0864920`

Symptom was `vi hello.cc` dying with
`[TRAP] ENTS at PC=0x0000FC15: Must be preceded by CALL/CALLG` →
`Memory fault - core dumped`. Two independent defects:

1. **The restore side never ran.** NDIX does not return from kernel traps with
   `RETT`; `$NDIX/kernel/MASTER/machine/locore.c:535` (`trapex`) ends every handler
   with `lregbl $CNTXMASK,r3`. Measured 132 saves / 0 restores in one boot.
   Fix: save in `ENTT` keyed on the trap frame address (`THA+256`), restore at
   **both** exits — `RETT` (frame-keyed) and the `lregbl` trap-return (LIFO).
   The key must **not** be the resume PC: the kernel rewrites it
   (`machine/trap.c:430` and `:472` set `ap->cx_p = &fuerror` when `pagein()` fails).
   Pairing went 132/0 → 253 pushes / 251 hits / 0 misses.

2. **`ENTS` committed after faulting.** No abort guard, so a page fault on a frame
   write did not stop the instruction: it ran on to load `L` and clear the pending
   call, and the retry raised a false ISE. Validated against MICRO-5800-B30, where
   the `L` load and `INVSEQ` are both in the **terminal** microword together with the
   final `WRITE` and the instruction exit:
   - `ENTS_END  @004206  ... D,DAC,REG05 ... WRITE ... ADDR=GET_NEXT`
   - `ENTSN_3   @004254  ... C,SEQ ... F,RETURN INVSEQ ... WRITE ...`

   Guards added to `ENTS`, `ENTSN`, `ENTB`, `ENTM`, `ENTF`, `ENTFN`; in each the `L`
   assignment also sat before later faultable writes and moved to the commit point.

Files: `src/cpu/cpu.c`, `src/cpu/cpu_protos.h`,
`src/cpu/instructions/CALL/{Ents,Entsn,Entb,Entm,Entf,Entfn,Entt,Rett}.c`,
`src/cpu/instructions/SYSTEM/Lregbl.c`, `test/test_ents_pending_call_trap.c`.
New regression test `test_lregbl_trap_return_restores` (21/21). vi now opens files.

RetroCore port is the same change plus deletion of the per-PCB single slot.
Note `CpuND500.MMU.cs` landed in another session's commit `5dc5e1638`, not in
`fa0864920` — content is correct, attribution is split.

### 1.2 Other confirmed-done items
- SIINTR delivered at the device's own IPL (per-device IPL from `FE_IDEV`).
- RetroCore commit unblocked by `git worktree prune` (13 stale agent worktrees, one
  with a corrupt nested gitdir, made every commit fail).

---

## 2. The OPEN bug (P1)

### 2.1 Symptom
Intermittent under multiuser/telnet load:

```
[MMU] TRAP: PS_AZI page fault! L1=112 L2=4 must be 0! vaddr=0x4700204D
[MMU] TRAP: No data capability! domain=0 segment=0 vaddr=0x01B84C7C   (repeats)
[TRAP] Runaway trap loop: protection violation at PC=0x4700204D - HALTING
[STOP] Invalid instruction 0x00 at PC=0x4700204D CED=0 CAD=3 B=0xC30000A4
```

Pre-existing (present in 2026-07-29 logs), **not** caused by the interlock work.

### 2.2 Fully traced chain (every step measured)

| # | Finding |
|---|---|
| 1 | Wild jump is `retd` at `0x00000654` in NDIX `_resume` (`machine/locore.c:960-970`). PC ring: `254: 0x654 -> 255: 0x4700204D` |
| 2 | `_resume` loads `B`/`L` from the u-area window (`b := r1.0` / `l := r1.4`, `0x638`/`0x63C`); `r1 = 0xE8000340` in **all** 158 samples — pointer always correct |
| 3 | On the bad iteration that VA translates to phys `0x00008340` instead of the usual `0x004CE340` / `0x004BB340` |
| 4 | It resolves via `PS_AZI` **direct**, not a page walk: `psn=16 pst{mode=0 pfn=0x10}`, and `0x8340 = (16<<11) + 0x340` |
| 5 | The segment-29 capability is the anomaly: healthy `0x802E/0x8033/0x8038/0x803D` (psn 46/51/56/61) vs bad `0x8010` (psn 16). `0x8010` = write-bit\|16 — **exactly what the guest wrote**, so the MMU and capability read are faithful |
| 6 | `_resume(pstindex, context)` sets the u-area capability from its first argument (`locore.c:920-931`). Probe at PC `0x611`: healthy `pstindex=51/46/56/61`, bad `pstindex=16`. Same caller (`swtch`, return `0x00037EED`) every time |

`pstindex=16` is **not garbage**: healthy slots are 46, 51, 56, 61 — spaced 5 apart
(UPAGES). The series extends down through 41, 36, 31, 26, 21, **16**. It is a
legitimate u-area slot that simply has no mapping.

### 2.3 Hypotheses REFUTED — do not revisit
- **Stale cached `g_termin_ring`.** Ran with `ND500X_FEDBG=1`, which enables the
  emulator's own staleness check (`src/cpu/nd500_fecall.c:632-640`). Crash
  reproduced, `"mx_bin phys MOVED"` printed **zero** times.
- **fecall ring overrun.** Ring end `0x0012C7DC` vs u-area phys `0x004CE340` — a
  3.8 MB gap; `max` was a sane 1000.
- **Terminal bytes corrupting memory.** `0x4700204D` = `'G' NUL ' ' 'M'` and
  `0xD00D4655` = `0xD0 CR 'F' 'U'` are **kernel low-memory bytes that merely look
  like ASCII** — a red herring that cost two hypotheses.
- **Stale TLB.** There is no TLB/translation cache; `DCTSB` is a documented no-op
  (`src/cpu/instructions/SYSTEM/Dctsb.c:164-177`).

The fecall/telnet path is fully exonerated. Client traffic matters **only** because
it drives process churn.

### 2.4 Leading hypothesis (UNVERIFIED)
`p_addr=16` belongs to a process whose u-area was **swapped out**. Swap support and
WIP/PGU tracking are unimplemented (6 stubbed instructions). If so, the missing
swap-in path is the fix.

---

## 3. Minimal-setup measurement (feeds the P2 work)

`vmunix.init` line 1 is `mmusetup` — it **does** run in `--ndix` mode (invoked via the
init script, not from C). It fills `PST[0..767]` with identity entries
(`src/debugger/commands.c:3084-3116`) and `nd500_mmu_set_pst_entry` **mirrors each into
the guest PST** at `PSTP` (`src/cpu/nd500_mmu.c:913-921`). That is where the
`PST[16] = 0x00000010` in the failing translation comes from.

Two measurements (probes since removed, tree clean):

```
[PSNUSE] total used=38  still-identity=2  guest-owned=36
guest-owned: 18 19 21 22 24 46 47 48 49 51 56 57 58 59 61 62 63 64
             66 67 68 69 71 72 73 74 76 77 78 79 768 769 770 771 800 801
still-identity: PSN 8 and PSN 16 only
```

```
[WHO816] psn=16 seg=29 cap=0x8010 va=0xE8000340 PC=0x0000063C DATA
[WHO816] psn=8  seg=8  cap=0x0008 va=0x4700204D PC=0x4700204D IFETCH
```

Both remaining identity consumers are **consequences of the bug**: PSN 16 only inside
the failing `_resume`, PSN 8 only as the instruction fetch **at** the wild address
after the bad `retd`. The probe was silent for the entire boot.

**Conclusion: NDIX needs nothing from the 768-entry identity fill.** All of
`PST[256..767]` (domains 1/2 demo) is never touched, and NDIX uses PSN 768-801, above
`mmusetup`'s range, which it creates itself.

**Caveat:** removing the fill does not by itself fix §2 — supplying identity for
PSN 16 is what turns a page fault into silent corruption. A crude test that zeroed
`PST[16,21,26,31,36,41]` **hung the boot** in one of two runs.

---

## 4. Reproduction

```bash
cd <nd500x repo root>
( sleep 400 ) | ND500X_STOPDBG=1 timeout 420 ./build/bin/nd500x \
    --ndix "$NDIX/rootfs_full.img" \
    --kernel "$NDIX/kernel/MASTER/GENERIC/vmunix" \
    --telnet=5107 > /tmp/run.log 2>&1 &
# once "login:" appears:
python3 tools/ndix/exp4.py 5107        # two-terminal login/churn driver
grep -c 4700204D /tmp/run.log          # ~4 hits in 6 runs
```

Hit rates measured: no `--telnet` 0/7; `--telnet` without a client 0/6;
`--telnet` + two-terminal churn 4/6.

Useful env probes (all off by default): `ND500X_STOPDBG` (PC ring on stop),
`ND500X_FEDBG` (fecall + ring staleness), `ND500X_PGFDBG`.

---

## 5. Prioritised plan

**P1 — stop the crash (unblocks confidence in all other testing)**
1. Determine whether `p_addr=16` is a swapped-out u-area (instrument the proc table /
   swap path where `swtch` selects it).
2. If yes, implement the swap-in path so the u-area PST entry is re-established
   before `_resume` reads it. If no, find what else allocates `p_addr` without
   mapping it.
3. Re-run the repro — target 0/6 against the ~4/6 baseline.

**P2 — make `--ndix` self-contained** (blocked by P1's mmusetup decision)
Derive `.pseg`/`.dseg` from `--kernel`; compute the dseg load address and map-kdata
size from the files instead of the hand-written `0x42000`/`0x3E800`; set
THA/CTE1/CTE2/CAD internally; apply only the minimal MMU setup from §3; keep a
`<name>.init` override. Also removes the machine-specific absolute paths from
`vmunix.init`.

**P3 — MMU correctness backlog** — C# guest capability read; C# demand paging;
settle the PFZPST divergence (C says `0xF`, C# says `0xD`).

**P4 — RetroCore parity** — fecall / MON-600 layer so RetroCore can boot NDIX.

**P5 — devices** — TAPE (generic device 2), read-only SIMH `.tap`.

**P6 — housekeeping** — hardcoded image paths in `test/diag_*.c`; the `.md` sweep;
the `src/_libmon.old/` deletion question.

---

## 6. State at handoff
- `nd500x`: tree clean, `git diff` empty, ctest 23/26 (3 pre-existing failures:
  `ote_instructions`, `mon_calls`, `instruction_validation`). All temporary probes
  removed.
- `RetroCore`: builds clean, 0 errors.
- No background work running.
