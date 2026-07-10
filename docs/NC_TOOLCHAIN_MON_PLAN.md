# Plan: NC Compiler + ND Linker Running Under nd500x (MON Call Workstream)

Date: 2026-07-09
Goal (user-confirmed): compile a C file with the real NC compiler
(nc-a06.dom) AND link it with the real ND Linker (linker-b01.dom), entirely
under emulation, producing a runnable DOM. The WSL cross-linker is not
trusted for this and cannot produce DOM files.

## Established facts (four-way inventory, 2026-07-09)

1. **NC needs exactly 34 MON calls** (static analysis of nc-a06.asm, 50 call
   sites, all in the runtime wrapper section 0x0802DBxx-DExx). A data-segment
   scan of nc-a06.dom found no hidden indirect MON targets. The list matches
   /mnt/d/ND/500/FraTor/nc/mon-calls-described.md 1:1.
2. **nd500x already implements all 34** (libmon's 35 VALIDATED handlers were
   built from this binary). GUEST/A.O and B.O were produced by the emulator -
   NC already compiles simple files under nd500x. There are NO missing MON
   calls for NC itself.
3. **C# (RetroCore) has real implementations for 32 of 34**; MON 321B UEADM
   deliberately errors (correct, deprecated); MON 413B FSCDNT has 97 lines of
   real logic that is DEAD CODE due to broken registration wiring. Contrary
   to first impressions, C# does NOT have more working MON calls than C: of
   its 209 handler files, 160 are throw-NotImplemented skeletons; the ~40
   real handlers are the same set as C's 40 (35 validated + 5 in-progress).
   Nothing significant to migrate C# -> C; the work is VERIFICATION.
4. **The ND Linker exists as a DOM**: /mnt/d/ND/500/nd-linker/linker-b01.dom
   (724,992 bytes), with job scripts incl. linker-auto-c.job. A raw byte scan
   shows it uses the NC set PLUS roughly 25-30 more MON numbers, notably:
   162B OUTST (7 uses, C status IN_PROGRESS), 513B (14 hits - undocumented,
   verify against disassembly, may be false positives), 144B MAGTP, 74B SETBT
   (stub in C), 66B ISIZE, 71B/72B DESCF/EESCF, 45B, 53B RSEGM, 104B HOLD,
   13B CIBUF, 16B/17B MGTTY/MSTTY, 214B, 217B, 244B, 251B, 254B, 257B, 263B,
   273B, 320B, 322B, 336B, 423B, 505B GERRCOD, 511B, 512B, 514B 5TMOUT.
   These need disassembly-level confirmation (byte scan has false positives -
   proven by the phantom "MON 400B" in nc-a06 which was address+constant
   coincidence).
5. **Documentation is sufficient**: 231 structured YAML files in
   /mnt/e/Dev/Ronny/NDInsight/Developer/MON/calls/ (parameters with
   types/directions, ND-500 CALLG conventions, error-in-W1/K-flag), the full
   ND-860228.2 manual OCR with complete error-code appendix (A), plus
   kernel-side dispatch analysis (GOTAB) in NDInsight/SINTRAN/OS/.
   Caveats: YAMLs marked validated:false (OCR noise possible); per-call
   error codes mostly defer to the global appendix.

## Known bugs to fix first (all detected by the new ctest registration)

- [RESOLVED] MON 321B UEADM (deprecated) returns error + K flag. The error
  code is 124 (octal 174B "Illegal parameter"), NOT 52. Verified against the
  manual's Appendix A background-error table: decimal 124 = octal 174 =
  "Illegal parameter"; decimal 52 = octal 064 = "No such friend" (an
  unrelated code). C is at 124 (MON_ERR_ILLEGAL_PARAMETER). The earlier "52"
  in this doc and the plan was wrong - do not change C to 52.
- C test data for MON 312B MOINF expects 0/-1; both emulators correctly
  return fake entry address 0xF8000000+n for implemented calls. Fix the TEST.
- MON 1B queued-console test gets 'T' instead of 'X' - queue ordering bug in
  the C test infrastructure (C# semantics agreed: non-blocking, error 57 on
  empty).
- C# MON 413B: wire the existing FSCDNT logic into the registration table
  (name registered as FSDCNT - also fix spelling mismatch).
- C# 233B/275B STEFI/STRFI name/number crossing in Definitions.cs.

## Phases

### Phase 1 - NC end-to-end loop hardened (C side, nd500x) - DONE 2026-07-09
1. [DONE] Fix the three C-side bugs above; make ctest fully green.
   (Also fixed along the way: systemic FLOAT_MATH register-operand bug,
   POLY decoder coefficient count, BYCONV overflow write, stale LGET MMU
   test setup - exposed by the regenerated 39,803-case test JSON.)
2. [DONE] Compile loop scripted: test_dom_integration gained --input
   (queued console via mon_queue_console_input) and --compare
   (byte-compare produced vs expected). The NC dialogue is
   "COMPILE <name>,<name>,<name>" with BARE SINTRAN names - typing
   "B.C" makes NC treat the dot as part of the NAME and append the
   default type (:C -> host B.C.C, fails "no rewrite"). NC appends
   default types itself: source :C, list :LIST, object :NRF.
3. [DONE] ctest tests dom_nc_compile_a/b: sandbox reset by CTest fixture
   (test/nc_sandbox_setup.cmake), fixtures in test/nc_fixtures/,
   byte-exact comparison against the January baselines. 16/16 green.
4. Exit criterion met for the regression loop. REMAINING (Phase 1b):
   - CONFIRMED 2026-07-09: the baseline .O files are PREPROCESSOR OUTPUT,
     not object code. B.O is the C source with the VALUE macro expanded
     ("x = 42;") plus a trailing 0x0D 0x13 terminator - 30 ASCII bytes.
     A real NRF object file is BINARY: see the genuine ND-produced sample
     /mnt/d/ND/500/FraTor/test-real/test-real.nrf (4677 bytes, dated
     1991-03-18, from test-real.pasc) - it starts 0A 00 01 70 44 ... with
     NRF control bytes and embedded symbol names (TEST_REAL, INPUT,
     OUTPUT). So the current regression baselines validate the
     PREPROCESSOR, not the compiler back end.
   - The NC compile TERMINATES EARLY: console shows "no rewrite" then
     " terminated"; the "generate-code" phase (string present in the DOM)
     is never reached. NC tries to open NC-A:INIT (the compiler config /
     init file) and fails with error -46 (no such file); no NC-A:INIT
     exists anywhere on /mnt/d. Hypothesis: without the init file the
     compiler runs preprocess-only and terminates. Next: find or
     reconstruct NC-A:INIT, or determine the command/option that drives
     code generation ("generate-code" option is in the string table).
   - Cross-emulator: C# compiling the same sources to identical bytes
     (tasked to the C# side) is the true exit criterion. Until codegen
     works, both emulators would only agree on preprocessor output.
   - test-real.nrf is a real GOLDEN NRF - promote it as the format oracle
     once codegen produces binary output (per review refinement 4).

**Baseline caveat (C# LLM refinement, accepted)**: comparing NC output
against GUEST/A.O + B.O is SELF-REFERENTIAL - those baselines were produced
by this same emulator, so this is a regression test, not ground truth. Both
emulators could share a bug and stay green. Label the ctest accordingly
("emulator-regression baseline"). If real ND-produced .O files turn up on
the /mnt/d disk images, promote those to golden. The REAL Phase 1 exit
criterion is CROSS-EMULATOR agreement: the C# emulator compiling the same
.C to byte-identical .O (that task is assigned to the C# side and accepted).

### Phase 2 - Shared MON test spec (C# side generates, both sides consume)
User decision: shared language-neutral JSON spec, one file per concern or
one mon_tests.json, generated by the C# side (which owns test generation
infra), consumed by BOTH the C test runner and a C# xunit harness.
- Spec source of truth: the YAML docs (parameter layouts, error codes from
  Appendix A), NOT either emulator. C# is process-oracle but manual is truth.
- **Per-case provenance tag (C# LLM refinement, accepted)**: every case
  carries "provenance": "manual" | "convention" | "yaml-unvalidated".
  Rationale: some settled semantics (e.g. MOINF 0xF8000000+n) are shared
  emulator conventions, not manual text; when a case fails, the tag tells
  you whether to fix the emulator or the spec. Essential while all YAMLs
  are validated:false.
- **Case shape = raw machine state (C# LLM refinement, accepted)**: the
  friendly form (mon number, args, console_input) exists only at GENERATION
  time. The emitted JSON contains raw initial: {regs, ram} / expected:
  {regs, ram} exactly like nd500_tests.json, plus console/file sections and
  post-return register/PC effects (not just W1 + K flag). This way neither
  harness builds its own CALLG frame - the spec tests MON semantics, not
  harness code - and the C runner becomes a small extension of the existing
  test_instruction_validation replayer instead of a new CALLG-aware harness.
- **Determinism conventions pinned in the spec header (C# LLM refinement,
  accepted)**: fixed-clock rule for time/RTC/elapsed-time MONs and terminal
  device numbers; binary file content encoded as byte arrays or base64;
  SINTRAN filename semantics (version suffix, default type) defined once in
  the header. File-system cases use a sandbox temp dir per test ("files"
  preamble section).
- Round 1 coverage: the 34 NC calls. Round 2: the confirmed linker set.
- C side work: a test_mon_spec runner (reuse test_instruction_validation
  pattern: cJSON, data file copied at build).

### Phase 3 - Linker under emulation (C side, with C# validating in parallel)
1. Disassemble linker-b01.dom (existing nd500x disassembler) and produce the
   confirmed MON list + a mon-calls-described.md equivalent, same method as
   nc-a06. Settle the 513B mystery.
   **513B pre-check result (2026-07-09)**: the cheap GOTAB upper-bound test
   is INCONCLUSIVE. GOTAB is 256 entries (0-377B) so it cannot index 513B,
   but the kernel dispatch guide (NDInsight/SINTRAN/OS/23-MON-CALL-DISPATCH-
   DEVELOPER-GUIDE.md, section 7) confirms calls >= 400B route through a
   separate ND-500 command path extending at least to 515B (SMTRANS).
   513B is therefore IN RANGE of that path and cannot be ruled out by table
   size. The YAML docs cover 505B, 507B, 514B, 515B but have a hole at
   506B/510B-513B - so 513B is either a scan artifact or a genuinely
   undocumented ND-500-path call. Only disassembly settles it.
2. Gap-implement the confirmed missing handlers in C, in dependency order
   (likely first: 162B OUTST finish, 74B SETBT, 66B ISIZE, 505B GERRCOD,
   13B CIBUF, 53B RSEGM). Each implemented strictly from its YAML +
   manual entry; add spec cases (Phase 2 format) for each BEFORE
   implementation (doc-driven, test-first).
3. Mirror in C#: same handlers implemented/verified there (C# LLM), spec
   cases shared.
4. Run linker-auto-c.job flow under emulation: NC output .O -> linker ->
   runnable DOM. Exit criterion: the produced DOM loads and runs its main
   under nd500x.

### Phase 4 - Continuous parity
- Both repos run the shared MON spec in CI/ctest.
- New SINTRAN binaries (assembler, PLANC, BASIC, backup-manager - DOMs exist
  under /mnt/d/ND/500/) get the same treatment: scan, confirm, gap-fill.

## Division of labor (user-confirmed)
- **C side (nd500x)**: Phase 1 entirely; Phase 2 C runner; Phase 3 items
  1-2 and 4.
- **C# LLM (RetroCore)**: MON test spec generation from YAMLs; 413B wiring
  fix + 233B/275B anomaly (both independently verified against the C# code
  by the C# side, 2026-07-09); dedicated C# tests for the 34 NC calls; AND
  run the same NC DOM compile end-to-end in the C# emulator to validate
  parity. C# side confirmed nc-a06.dom loads, boots to prompt, accepts
  commands and runs analysis passes there (25+ NC/DOM tests green) but no
  test yet asserts produced object-file content - that gap is accepted.
- **Status sweep must be mechanical (C# LLM refinement, accepted)**: rule =
  delegate null OR body is throw-only -> NotImplemented. No hand
  classification (it drifts). Additionally a permanent unit test asserts
  file-name <-> registered-name consistency - that check would have caught
  both of today's wiring bugs (413B FSDCNT/FSCDNT spelling, 233B/275B
  crossing) automatically. Mirror the same idea in C: a test asserting
  mon_registry.c entries match the handlers/mon_*B_*.c file set.

## Notes / risks
- OCR noise in YAMLs: any suspicious parameter layout must be cross-checked
  against the manual page cited in the YAML (page/line refs included).
- Name-table discrepancies exist between the nd500x disassembler's MON
  annotations and mon-calls-described.md (e.g. 32B MSG vs RDBK, 41B ROBJE vs
  OPEN annotation). The YAML/manual names are authoritative; fix the
  disassembler's MON name table as a side task.
- The RFILE/WFILE wrappers in NC share one parameter frame; block-size and
  byte-count semantics (ND-500 uses BYTE counts, ND-100 uses word counts)
  are the classic divergence point - the spec cases must pin them.
- MON 60B N500M (ND-100<->ND-500 control) is documented in depth in
  NDInsight but not needed by NC or (apparently) the linker; out of scope.
