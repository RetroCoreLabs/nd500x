# HANDOFF - MON-call completeness for NC + LINKER (start here next session)

**Full path:** `/home/ronny/repos/nd500x/docs/HANDOFF_MON_COMPLETENESS.md`
Date: 2026-07-14

## Where we are (2026-07-14)

The ND-500 C compiler `nc-a06.dom` now **PARSES C cleanly** on nd500x after 8 verified fixes
(see `/home/ronny/repos/nd500x/docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md` UPDATE 60-62; the big one
was the SCOMP S-flag inversion in `src/cpu/instructions/COMPARE/Scomp.c`). `int x; main(){x=1;}`
-> "no errors detected". C# sync prompt (8 fixes + 11 SCOMP test regen) is in UPDATE 61/62.

NEW downstream blocker (not this task): after the clean parse, NC runs away in CODE GENERATION
(no halt at 30M instructions, `T.NRF` still 0 bytes, PC looping in 0x0802B000-0x0802E000).

## THIS TASK: make sure EVERY MON call NC and the LINKER need is implemented and correct vs carve

Goal: a complete, carve-verified MON-call set so NC and the ND LINKER run end-to-end. Two parts:

### 1. Build the required-call list (what NC + LINKER actually invoke)
- NC (compile): reuse `/home/ronny/repos/nd500x/test/diag_monlog.c` (mon_log_enable + INFO).
  Run FROM `build/nc_sandbox` (reset from `test/nc_fixtures/` first), cmd `COMPILE T,T,T\r`.
  Distinct calls seen so far (int a; compile): 0B 11B 12B 32B 41B 43B 50B 54B 62B 64B 73B 76B
  113B 114B 117B 120B 123B 143B 221B 256B 262B 312B 317B 321B 422B 503B 504B.
- LINKER: the ND LINKER is `/mnt/d/ND/500/nd-linker/linker-b01.dom` (analysis:
  `/mnt/d/ND/500/nd-linker/linker-b01.dom.moncalls.md`, `.analysis.md`, `.asm`). Load it the same
  way (adapt diag_monlog's DOM path) and drive it with a link job to capture its MON calls.
  Reference: `/home/ronny/repos/nd500x/docs/ND-860289-2-EN ND Linker User Guide and Reference
  Manual.md` and the auto-jobs `/mnt/d/ND/500/nd-linker/linker-auto-c.job`.

### 2. For each required call: implemented? correct vs carve?
- Our handlers: `/home/ronny/repos/nd500x/src/libmon/handlers/mon_<N>B_*.c` + dispatcher
  `src/libmon/mon_dispatcher.c`.
- ORACLE (ground truth, NOT the C# sibling):
  - Carve tree: `/mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/L-VSX-500/re/mon-analysis/<octal>B-<Name>/`
    (README.md + `<Name>.ASM` + `<Name>.pseudo.c`). Index: `.../re/MON-CALL-INDEX.md`.
  - NC-oracle contracts: `/mnt/e/Dev/Ronny/NDInsight/SINTRAN/ND500/mon-oracle-for-NC/*.md`.
  - `mon-status` skill reports per-call carve status.
- For each call: (a) do we dispatch/implement it? (b) does our return contract (registers, K flag,
  output params, byte counts, EOF/partial semantics) match the carve ASM/pseudo-C? Spawn agents
  to compare one call per agent (pattern used all session - it works well). Fix + re-test.

### Already audited vs carve (this session) - status
- 117B RFILE: FIXED (short read=success+count). 73B SMAX: FIXED (no ftruncate). 50B OPEN: fixed
  break; slot-0 write flagged (INFERRED, unresolved). 312B MOINF: fixed value 065453B.
- 41B ROBJE: page-count field correct; middle fields (access/device/dates/version) byte-shifted
  vs App F.6 - DEFERRED (NC only reads pages).
- 422B GSWSP: segment mapping clean. 262B CPUST: fills 24-byte buffer (bytes 2/3=0; oracle says
  no ND-500 encoding). 120B WFILE / 76B SETBS: match. 143B/312B/317B/321B: audited earlier.
- NOT yet audited vs carve: 0B 11B 12B 32B 43B 54B 64B 113B 114B 123B 221B 256B 503B 504B, plus
  ALL linker-specific calls (unknown until step 1).

## Gotchas (carried forward)
- ALWAYS reset the sandbox from `test/nc_fixtures/` before every run (NC writes B.CAT/B.LIST/B.NRF
  that change the next run). Use `COMPILE` (not `CHECK`). `ND500X_PIN_CLOCK=1`.
- The C# RetroCore emulator is the SAME author's code kept in sync - NOT an oracle. Only the ND
  manuals + carved SINTRAN L07 are truth.
- Diag harnesses are standalone gcc builds against `build/lib/*.a` - rebuild after any lib change.
  Link line in `docs/NC_CRASH_HANDOFF.md`.
- All our diagnostics are env-gated (ND500X_GETB_TRACE/GUARD, NC_WWATCH, SEG_DUMP, etc.), off by
  default; trap-dispatch is default-on.
