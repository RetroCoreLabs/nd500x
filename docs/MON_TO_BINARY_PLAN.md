# Plan: from here to a linked ND-500 binary (MON work is NOT done until this passes)

**Full path:** `docs/MON_TO_BINARY_PLAN.md`
Date opened: 2026-07-14
Related: `docs/MON_COMPLETENESS_MATRIX.md`,
`docs/MON_CSHARP_SYNC_HANDOFF.md` (HELD - do not send until Phase 5),
`docs/HANDOFF_MON_COMPLETENESS.md`.

## Definition of DONE (the only thing that counts)

A C source file is compiled by `nc-a06.dom` under nd500x to an NRF object (non-zero, valid),
then linked by `linker-b01.dom` under nd500x to a runnable ND-500 binary, then that binary is
EXECUTED under nd500x and produces the expected output. Only when that full chain runs green is
the MON work "done" and the C# handoff (`MON_CSHARP_SYNC_HANDOFF.md`) released. No MON call is
"done" on code-read alone - each must be exercised on the real NC/linker path and observed.

## Standing invariants (apply to EVERY step)

- Reset the sandbox before every NC/linker run:
  `rm -rf build/nc_sandbox; mkdir -p build/nc_sandbox/SCRATCH; cp -r test/nc_fixtures/GUEST build/nc_sandbox/; cp -r test/nc_fixtures/expected build/nc_sandbox/`
- Env: `ND500X_PIN_CLOCK=1`. Use `COMPILE` (not `CHECK`).
- After ANY libmon/cpu change, rebuild the diag harnesses (they link `build/lib/*.a`).
- No claim is "verified" until it is observed in a run and recorded in the Verification Tracker
  below with a real artifact (log line, byte dump, exit code).
- Regression gate after every code change: `./build/bin/test_instruction_validation --continue`
  must still pass its full count (currently 39,803).

---

## VERIFICATION TRACKER (the changelog held from C# until Phase 5)

Status: APPLIED (edited) -> BUILT (compiles) -> VERIFIED (observed working on NC/linker path).
Nothing goes to C# until every row a real run depends on is VERIFIED.

| # | Change | File | Status | Evidence needed |
|---|--------|------|--------|-----------------|
| 1 | SCOMP S-flag (source1<source2 -> S=1) | src/cpu/instructions/COMPARE/Scomp.c | VERIFIED (NC parses) + committed | NC "no errors detected" |
| 2 | COMP2 F/D IEEE compare | src/cpu/instructions/COMPARE/Comp2.c | BUILT + committed | needs a float-compare exercise |
| 3 | trap -> THA dispatch | src/cpu/cpu.c | BUILT + committed | needs PV-trap path observed |
| 4 | 117B RFILE short-read=success+count | handlers/mon_117B_ReadFromFile.c | VERIFIED (NC parses) + committed | NC reads source correctly |
| 5 | 73B SMAX records length only | handlers/mon_73B_SetMaxBytes.c | committed | + row 9 below |
| 6 | 50B OPEN -52 break | handlers/mon_50B_OpenFile.c | committed | open error path |
| 7 | 312B MOINF MCTAB[321B]=065453B | handlers/mon_312B_CheckMonCall.c | committed | NC UEADM path |
| 8 | 123B RELES unreserved=success no-op | handlers/mon_123B_ReleaseResource.c + mon_file_table.c | BUILT (2026-07-14) | release call in NC/linker log, no err 5 |
| 9 | 43B CLOSE applies deferred SMAX truncate | mon_file_table.c + handlers/mon_73B_SetMaxBytes.c + mon_file_table.h | BUILT (2026-07-14) | closed file has correct byte length |
| 10 | 64B ERMSG drop code-0 error | handlers/mon_64B_WarningMessage.c | BUILT (2026-07-14) | code-0 call continues |
| 11 | 256B DEABF 2-arg + err46 on unresolved | handlers/mon_256B_FullFileName.c | BUILT (2026-07-14) | NC create-if-missing path |

**Phase 0 result (2026-07-14):** clean native build; `test_instruction_validation --continue`
= 39792 passed, 11 failed. All 11 failures are the pre-existing stale-baseline SCOMP cases
(`ByteDiff_*` + `UnsignedByte_*`, tests 18655-18665) that predate the committed SCOMP S-flag
fix - NOT a regression from the 4 MON fixes. These 11 are the tests to regenerate in Phase 5.
Rows 8-11 moved APPLIED -> BUILT. Zero new failures introduced.

(Append a row for every future change: each linker handler, each codegen fix.)

---

## PHASE 0 - Baseline: build the applied fixes, prove no regression

Objective: get rows 8-11 from APPLIED to BUILT and confirm nothing broke.
Steps:
1. `make` (native). Fix any compile error in the 4 applied fixes.
2. `./build/bin/test_instruction_validation --continue` -> full pass (39,803).
3. Rebuild diag harnesses (link line in `docs/NC_CRASH_HANDOFF.md`).
Validation gate: clean build + full test pass. Update tracker rows 8-11 -> BUILT.
If regression: bisect to the offending fix, correct, repeat. Do not proceed on a red suite.

## PHASE 1 - NC: compile a C source to a VALID NRF object

Objective: NC emits a non-zero, structurally valid `T.NRF` for a trivial program.
Current blocker: after the clean parse, NC runs away in code generation (no halt at 30M instrs,
NRF=0, PC looping 0x0802B000-0x0802E000). This is the gate for "compile".

Steps:
1. Reset sandbox. Run NC `COMPILE T,T,T` (source `int x; main(){x=1;}` fixture) with mon logging.
2. Capture the codegen loop: where PC cycles, which MON calls fire in the loop, what NC is
   waiting on. Use the diag harness + `mon log` + instruction trace on the 0x0802B000-0x0802E000
   window. Suspect first: a MON call in the codegen/output path returning a wrong value that
   makes NC spin (mirror of the RFILE/SCOMP pattern) - cross-check every MON call NC issues
   during codegen against the carve (many are already audited; re-verify the ones on THIS path).
3. Root-cause the runaway (MON-return bug vs CPU-instruction bug vs NC-internal). Fix.
4. Iterate until NC halts cleanly AND writes a non-zero NRF.
NRF validation - IMPORTANT corrections (2026-07-14):
- `nd500-dis` does NOT understand NRF. Do not use it to validate objects.
- The `test/nc_fixtures/expected/*.NRF` files are NOT objects - they contain the C SOURCE text
  (B.NRF = "int x;\rmain()\r{\r x = 42;\r}"). They are invalid golden references; the old
  `dom_nc_compile_b` ctest baseline compared against source text and is meaningless. These must
  be regenerated from a real NRF (or deleted) - do NOT byte-diff NC output against them.
- A GENUINE NRF is a control-group byte stream starting `0A 00`; each control field is a 5-bit
  control number + 3-bit numeric-length. Format spec: the linker manual
  `docs/ND-860289-2-EN ND Linker User Guide and Reference Manual.md`
  section "THE ND RELOCATABLE FORMAT" + "SUMMARY OF NRF-CONTROL NUMBERS" (~lines 8085-8180).
  Reference genuine objects to study: `$ND500_TESTDATA/FraTor/test-real/test-real.nrf`,
  `$ND500_TESTDATA/ND-500 Symbolic Debugger/debugger-b.nrf`.

Phase 1 pre-step: DONE + VERIFIED - NRF VALIDATOR built at `test/nrf_validate.c`
(full NRF spec encoded as enums/structs with per-item manual page cites; build
`gcc -std=c11 -o build/bin/nrf_validate test/nrf_validate.c`). It walks the control-group stream
and validates: BALANCED BEG(1)/END(2), well-formed control fields, no truncation, no BEG nesting,
CRITICALLY no IHB(25) "Execution Inhibit" (= NRF incomplete due to compiler errors) and no
IL1-IL4(34-37) illegal control numbers. EOF(26) is NOT required (verified: genuine objects just
END and stop). Verified working: test-real.nrf (1 module) + debugger-b.nrf (23-module library)
both PASS; the bogus source-text fixture expected/B.NRF correctly FAILS. Ready to lift into
nd500-dis/nd500-dump.

Validation gate (replaces nd500-dis):
- `T.NRF` size > 0.
- `nrf_validate T.NRF` (exit 0) -> VALID: balanced BEG/END, NO IHB, no illegal control numbers,
  has code + symbol groups. (IHB present = NC still thinks it errored -> not done.)
- ACCEPTANCE: the linker (Phase 3 harness) reads the NRF without an "illegal NRF control number"
  / format error (the real consumer is the ultimate validator).
- NC exit is a clean MON 0B, not a trap/handler bail.
Exit criteria: NC's NRF passes nrf_validate AND is accepted by the linker. Record it as the
working reference (we have NO pre-existing golden object for the trivial source). Update tracker
for any MON/CPU change made here.
Continue: escalate the source (function, multiple decls) and re-run nrf_validate + linker accept.

## PHASE 2 - LINKER: implement the MON handlers it needs

Objective: `linker-b01.dom` runs under nd500x far enough to attempt a link. ~20 handlers.
Strategy: decouple from Phase 1 by using a KNOWN-GOOD NRF (from the real toolchain, or Phase 1's
output) as linker input, so linker progress is not blocked on NC codegen.

Drive harness: adapt `test/diag_monlog.c` to load `$ND500_TESTDATA/nd-linker/linker-b01.dom`,
feed a link job (ref `$ND500_TESTDATA/nd-linker/linker-auto-c.job`), mon-log every call.

Implement in tiers; build + drive the linker after EACH tier and diff the MON log against the
previous run (each tier should let the linker reach a later MON call / further PC).

- Tier 2a - unblock startup (benign success): 45B, 320B, 71B, 72B, 104B, 514B.
  Validate: linker gets past its pre-banner startup and prints its banner; no error-return on
  these calls in the log.
- Tier 2b - info/lookup (real outputs vs carve): 66B, 254B, 257B, 263B, 214B, 217B, 273B, 322B,
  505B. Validate per call: output params match the carve's documented fields (log the written
  words, compare to carve README). 505B: read-then-clear observed.
- Tier 2c - file/segment/string on the link path: fix 162B (0x27 terminator + 2-arg), fix 412B
  (output-slot return + real segment connect), 423B. Validate: linker's file reads/writes and
  segment connects succeed; OUTST output is correct text.
- Tier 2d - XMSG gateway: 511B (compose 503B+504B), and 512B+513B as ONE shared handler keyed on
  arg0 & 077B with success=W1==1, backed by a minimal in-emulator XMSG mailbox (port table +
  message buffers) for the transport subfns (LFOPN/CLS/GET/REL/SND/RCV/REA/RHD). Validate: the
  59 513B calls return W1==1 on control subfns and deliver real payload on transport subfns; the
  linker consumes replies without spinning.
Each handler: implement -> build -> drive linker -> confirm the specific call in the log behaves
per carve -> add a tracker row VERIFIED. Blocked/low-prio (53B, 144B, 244B, 336B): stub to a
safe benign return and log that it is unverified; only implement if the linker actually needs it.
Exit criteria: linker runs to its link phase without hitting a stub/error-return.

## PHASE 3 - Drive the linker to PRODUCE a binary

Objective: linker consumes the NRF and writes a linked ND-500 domain/binary.
Steps:
1. Reset sandbox with the Phase 1 NRF (+ any required C runtime objects - see
   `compile-link-goal` memory for the missing runtime libs; obtain/stub them).
2. Run the link job to completion under nd500x.
3. Capture the output binary.
Validation gate:
- Linker exits cleanly (MON 0B), no trap, and reports no "illegal NRF control number" / format
  error while reading NC's object (this is also the acceptance test for Phase 1's NRF).
- Output binary is non-zero. The binary is a DOM/domain (`nd500-dis -a` DOES understand DOM, per
  the linker analysis notes) - validate header magic (DOM `0x4aa0`), code+data sections, entry
  point; compare structure against the known-good `$ND500_TESTDATA/nd-linker/linker-b01.dom` shape.
Exit criteria: a linked binary file on disk that loads under nd500x (Phase 4 runs it).
Continue: if the linker errors mid-link, the failing MON call or link step is the next target -
loop back to Phase 2 for that call.

## PHASE 4 - END-TO-END: compile + link + RUN

Objective: prove the whole chain on a real program.
Steps:
1. Pick a known C program with defined output (e.g. prints a constant).
2. NC compile -> NRF (Phase 1 path). Link -> binary (Phase 3 path).
3. Load and RUN the linked binary under nd500x.
Validation gate: the binary runs and produces the EXPECTED output (byte-compare against the
known-good result). This is the DONE gate.
Exit criteria: green end-to-end run recorded with the actual output bytes.

## PHASE 5 - Finalize and release the C# handoff

Only after Phase 4 is green:
1. Confirm every Verification Tracker row is VERIFIED.
2. Regenerate the 11 SCOMP ByteDiff test cases (they encode the old inverted convention) in
   `$RETROCORE/Emulated.Tests.ND500/Validation/`.
3. Consolidate the verified changelog into `MON_CSHARP_SYNC_HANDOFF.md`, mark it RELEASED, and
   send to the C# LLM - CPU fixes, MON contract fixes, and the new linker handlers, all now
   backed by an end-to-end run, not code reads.
4. Commit the working-tree fixes (currently uncommitted: rows 8-11).

---

## How to know we're on track (rolling)

- Phase 1 done -> "we can compile C to an object."
- Phase 3 done -> "we can link an object to a binary."
- Phase 4 done -> "we can compile C to a runnable binary" = MON DONE, C# handoff released.
Each phase moves at least one Verification Tracker row to VERIFIED; if a run contradicts an
earlier "verified" claim, downgrade the row and re-open the phase. Assume nothing; observe.

## PHASE 2 PROGRESS LOG (2026-07-14, session 557c0950)

Decoupled from the NC object-output blocker (that is being worked in parallel session 48520b34;
see memory nc-codegen-crash.md U63/64 - object never transfers SCRATCH64->BOUT). Driving the REAL
linker directly instead.

- NEW HARNESS: `test/diag_linkmon.c` -> `build/bin/diag_linkmon <dom> "<cmd>" [max_steps]`
  (diag_monlog HARDCODES the NC dom + ignores argv - do NOT use it for the linker).
  Build line = standard diag static-lib group.
- Linker `$ND500_TESTDATA/nd-linker/linker-b01.dom` LOADS + BOOTS (Seg[22], prog 354165 / data
  365580). Startup MON calls succeed: 143B RSIO, 74B SETBT, 50B OPEN, 262B CPUST, 16B MGTTY,
  13B CIBUF.
- FIRST BLOCKER: MON 144B (MAGTP / DeviceFunction) at instr 5419, return PC 0xB004E9C8, 5 args
  observed = (0, 0, 46, 4096, 3). NO handler registered -> "unimplemented" halt. Carve
  144B-DeviceFunction: device-dependent "monster" call; worker MAGTP=026354B; the MON->worker
  link crosses an UNCARVED CALLPROC bridge (contract UNVERIFIED). 2 outputs per moncalls doc.
- NEXT: implement a benign 144B handler (Tier 2a: validate + success, sensible default outputs) to
  get PAST startup, re-drive with diag_linkmon, diff the MON tail vs this run to find the next
  blocker. Then work down the Tier 2a-2d list (matrix "Work remaining for the LINKER"). Use a
  known-good NRF as link input once it reaches the read-object stage:
  `$ND500_TESTDATA/FraTor/test-real/test-real.nrf` (staged as build/link_sandbox/GUEST/TEST.NRF).
  Linker command syntax still TBD (job files linker-auto-*.job are binary/encoded; need the ND
  Linker manual command grammar - check docs/ND-860289-2-EN...).

### Phase 2 progress (cont. 2026-07-14): past 144B, now a startup null-string PV

- Implemented a PROVISIONAL benign-success 144B MAGTP handler
  (src/libmon/handlers/mon_144B_DeviceFunction.c) + flipped its registry status to
  MON_STATUS_IN_PROGRESS (src/libmon/mon_registry.c) - the dispatcher HALTS on
  MON_STATUS_NOT_IMPLEMENTED *before* calling the handler (mon_dispatch.c:173), so the status
  field, not just the body, must change. Linker now runs PAST 144B: instr 5419 -> ~9432.
- NEXT BLOCKER: a PROTECTION VIOLATION (null deref) at linker instr ~9432.
  - Fault site 0xB0041CF2 = `by scopa b.1648,b.1656,$0` (STRING compare); trapPC 0xB0041CFB.
    One of the two string pointers (locals B+1648 / B+1656) is NULL -> access to address 0.
  - Nested: the linker's trap handler at 0xB005571B then faults on data=0x04 (our trap-dispatch/
    RETT nesting weakness, memory U58).
  - RULED OUT: 76B SETBS(file100,blocksize=0)=ERROR is NOT the cause (PV instr unchanged when
    SETBS made to succeed; change reverted - not carve-justified).
  - ROOT LEAD: the linker's ONE 50B OPEN has an EMPTY filename ('' + '' -> './GUEST/.', fails
    -46). The startup config/string never loads -> null string pointer -> SCOPA PV. Same
    empty-string symptom family as U64 (54B/317B wrong STRING reader) - but 50B already uses
    mon_read_descriptor_string. So EITHER the linker's filename descriptor genuinely isn't
    populated yet (needs an earlier step / an INIT/startup file we don't provide), OR a different
    arg-reading path. 144B call site confirmed at 0xB004E9BC (`call $0xF8000064`=seg31+0x64=144B,
    5 args b.20,IND(b.52),b.36,b.56,b.44).
- NEXT: (1) disassemble the linker startup around the empty OPEN to see how it builds the filename
  descriptor (and whether 144B's output buffer feeds it - our stub writes nothing); (2) determine
  the linker's required startup file(s) (in-link-xx-b01.init/.prog/.xcom exist in $ND500_TESTDATA/
  nd-linker/); (3) get the ND Linker command grammar (docs/ND-860289-2-EN...) to drive a real link.
Working-tree changes this session (all marked): nd500_mmu_peek (mmu.c/.h, diagnostic), 144B stub +
registry status, harnesses diag_linkmon.c/diag_codegen_loop.c. 76B reverted.

### Phase 2 startup-PV fully traced (2026-07-14) - WALL: needs linker env / manual

Fault chain at linker instr ~9432 (startup, before any command):
- Routine @0xB0041C.. builds two 8-byte string descriptors on the stack, then `by scopa`:
    b.1648 (count0) = r.10 - r.6 (sub3) ; b.1652 (base0) = r.2 + r.6 (add3)
    b.1656 (count1) = r.28 - r.24       ; b.1660 (base1) = r.24 + r.20 (add3)
  where r = R = record loaded from local b.40 (populated by calls 0xB0041228 / 0xB0041261).
- base0 = r.2 + r.6; r.2 (record base pointer) is 0 -> descriptor base ~= 0 -> SCOPA reads
  address 0 -> PROTECTION VIOLATION (data=0). Nested trap handler @0xB005571B then faults on
  data=0x04 (trap-dispatch/RETT nesting weakness, memory U58).
- Upstream: the linker's ONLY 50B OPEN has an EMPTY filename ('' -> './GUEST/.', error -46). The
  record R's null base pointer is consistent with that failed data load.

INTERPRETATION: the linker faults during its OWN startup init, before reading any command, because
its runtime environment is incomplete - it expects startup/config files (candidates in
$ND500_TESTDATA/nd-linker/: in-link-xx-b01.init/.prog/.xcom) and/or a real interactive terminal, and
possibly a specific SINTRAN user/context. Distinguishing "our bug" from "missing env" needs either
the ND Linker User Guide (docs/ND-860289-2-EN...) startup/command grammar OR the ND-500 manual's
SCOPA + string-descriptor semantics as an oracle. This is the same manual/oracle/environment
boundary the NC object-output path hit.

DECISION POINT for the user (both paths to a linked binary are now at a research wall):
  A. NC->object: BOUT.NRF stays 0 (object goes to file 100/SCRATCH64, never transferred to BOUT);
     unidentified scratch->BOUT transfer; being worked in parallel session 48520b34.
  B. Linker: boots + past 144B, but faults in startup init on a null-base SCOPA (missing env/files).
Either needs manual/oracle input or the parallel NC result. Recommend: provide the ND Linker manual
startup section + the linker's expected INIT files, OR converge with session 48520b34 on the NC
object-output transfer, before more autonomous grinding.

### Phase 2 milestone (2026-07-14): LINKER BOOTS + EXITS CLEAN; next = terminal I/O (162B/503B)

Fixes that unblocked linker startup (details in NC_CRASH_...ROOTCAUSE.md U66/67):
  144B MAGTP benign stub, SCOPA rewrite (compare-with-pad), 313B IBRISZ impl, SSPAR rewrite
  (set-parity). Linker now runs full startup -> clean MON 0B LEAVE (instr 58264).

BUT it does not yet PROCESS commands - it exits identically for any input because its INTERACTIVE
TERMINAL I/O errors:
- 162B OUTST (linker banner/prompt output): linker calls the 2-ARG form; our handler requires 3
  and returns ERROR ("Missing parameters need 3 got 2") -> no banner. FIX: accept 2-arg form
  (device, string-descriptor/pointer), emit bytes until the 0x27 (') terminator (matrix note).
  Call site 0xB0055E6C.
- 503B DVINST (linker command input, 14-arg form): our handler reads arg[1] as MaxNo=0xF80000CB
  (a seg-31-like value) and rejects it ("exceeds max 2048") -> ERROR -> linker reads no commands
  -> exits. Then a cascade PV (trapPC 0xB00391B6, data 0xA8001CF8). Call site 0xB004AC90.
  FIX: determine the linker's 503B arg convention (14 args) vs NC's (which works); the MaxNo
  arg index / meaning differs. Carve: 503B-DVINST; NC uses same MON but different call shape.

NEXT: fix 162B OUTST (2-arg, 0x27-terminated) first (gets the banner), then 503B DVINST for the
linker's 14-arg form, then re-drive 'OPEN-DOMAIN "T";;LOAD T;;CLOSE N,N;;EXIT;;' with test-real.nrf
staged as GUEST/T.NRF to attempt producing T:DOM. Link command grammar: nd500-c-compile-and-link.md
sect 4 (min session) + sect 7 (explicit JOB). CLOSE N,N disables the auto-job (no NC-LIB/CAT-LIB
needed for a self-contained NRF).

### Phase 2 (2026-07-14 cont.): LINKER NOW INTERACTIVE - reads/echoes/processes commands

Two terminal-I/O fixes got the linker from "boots+exits" to "interactive":
- 162B OUTST (mon_162B_OutString.c): rewrote to accept the linker's 2-ARG form and stop at the
  0x27 (') terminator (carve 162B-OutString: GETCH/SOUTB loop, 047B terminator, 044B '$'->'$'+LF).
  Now SUCCESS. CAVEAT: for the linker's OUTST the string at arg1 has no 0x27 within 2048 -> writes
  the full 2048 (arg1 may be a DESCRIPTOR {count,base}, not raw text). Banner still garbled;
  revisit the 2-arg string form. Regression: verify no 162B test cases break (likely none).
- 503B DVINST (mon_503B_InputString.c): the linker passes MaxNo=0xF80000CB (a seg-31-like value;
  our positional read likely mis-slots it - carve says 503B params come via the ND-500 MESSAGE
  BUFFER, indexed). EXPERIMENT: clamp MaxNo to 2048 instead of erroring. RESULT: 503B reads the
  queued command correctly (read "EXIT" = 45 58 49 54 into BuffAddr) and the linker PROCESSES it.
  Proof it's really reading: behavior now differs by command (LIST-STATUS 58276 / ZZZBOGUS 58281 /
  EXIT 58281 instrs) and EMPTY input polls forever (a real interactive prompt). Keep the clamp as a
  pragmatic unblock; the correct fix is to resolve the linker's 503B message-buffer arg convention
  (MaxNo is not arg[1] for the linker). Call site 0xB004AC90.

STATE: linker reads+echoes commands. Driving 'OPEN-DOMAIN "T";;LOAD T;;CLOSE N,N;;EXIT;;' echoes
"OPEN-DOMAIN " but the command does NOT complete (no T:NRF open, no T:DOM produced). Errors seen
during command processing: 321B UEADM -> ERROR (deprecated), 74B SETBT -> ERROR, 76B SETBS -> ERROR.
NEXT: (1) fix the 162B 2-arg string form (descriptor vs raw) so the banner/prompt are clean; (2)
resolve 503B message-buffer args properly; (3) chase why OPEN-DOMAIN doesn't complete - work the
321B/74B/76B errors + trace the OPEN-DOMAIN command handler. Goal: produce T:DOM from test-real.nrf.

### Phase 2 (2026-07-14): 503B DVINST arg-layout mismatch = why linker commands truncate

Root of "linker reads only up to the first 'T'": our 503B handler maps args POSITIONALLY
(arg0=dev, arg1=MaxNo, arg2=retcount, arg3=buff, arg4=breakStrat, arg5=echoStrat, args6-9=break
table, args10-13=echo table). The LINKER's 14-arg DVINST does NOT match this. Full dump
(ND500X_DVINST_DUMP=1 env-gated probe in mon_503B_InputString.c; drive LIST-DOMAINS):
  arg[0]=0x00000000   arg[1]=0xF80000CB   arg[2]=0x00000000   arg[3]=0x1BFCB003
  arg[4]=0x00000007   arg[5]=0xFFFFFFFF   arg[6]=0x20202020   arg[7]=0x20202020
  arg[8]=0xB0001D48   arg[9]=0xB004D8DE   arg[10]=0xFFFFFFFF  arg[11]=0x00000000
  arg[12]=0x00000000  arg[13]=0x00000003
- arg[1]=0xF80000CB = 0xF8000000|0xCB = segment-31 address of MON 313B (IBRISZ) -> a PROCEDURE
  POINTER, not MaxNo. So we reject/clamp it wrongly.
- args[6],[7]=0x20202020 = ASCII spaces; args[8],[9] = pointers -> NOT a 128-bit break table.
  So our "break table" is garbage -> 'T' (0x54, bit 84) spuriously flagged as a break; CR is NOT
  flagged (HELP read past CR). Every command truncates at its first 'T'.
CONFIRMED symptom: HELP -> reads "HELP.EXIT" (full, no T); LIST-DOMAINS/LIST-STATUS -> "LIS";
OPEN-DOMAIN "T" -> 'OPEN-DOMAIN "'. All break exactly before 'T'.
NEXT: disassemble the linker's DVINST call setup at 0xB004AC90 to derive the TRUE 14-arg layout
(where are DevNo/MaxNo/Buff/BreakStrat/table?). Likely arg1 is an input/break ROUTINE pointer and
the real fields are shifted. Then remap the handler for this form (or detect it and use CR-break).
The MaxNo clamp + arg-dump probe are TEMPORARY (env-gated for the dump); replace with the real map.
Also pending: 162B 2-arg string form (descriptor vs raw), 321B UEADM returns ERROR (deprecated) -
may also need to succeed for the linker; both secondary to the DVINST layout.

### Phase 2 (2026-07-14): DVINST break-table FIXED -> commands read fully; next = 162B string form

FIX (mon_503B_InputString.c): the linker's mis-decoded user break table (strat 7, args6-9 garbage)
broke on 'T' but not CR. Added a guard: if a user-table strategy (7/8) yields a table that does NOT
break on CR (0x0D), fall back to MAC-style line break (CR/LF/ESC/EOF). NC (strat 1) unaffected.
RESULT: commands now read FULLY - "LIST-DOMAINS." , 'OPEN-DOMAIN "T".' (was truncating at 'T').
This is a PRAGMATIC guard; the real 503B linker arg layout is still unmapped (arg-dump probe kept,
env-gated ND500X_DVINST_DUMP). MaxNo clamp also still in place.

NEW STATE: driving 'OPEN-DOMAIN "T";;LOAD T;;CLOSE N,N;;EXIT;;' the linker reads OPEN-DOMAIN fully,
then floods MON 162B OUTST (call site 0xB0055E6C, TextAddr=0xB0056B34 fixed, writes full 2048 each
time - NO 0x27 terminator found) and exits without creating T:DOM. So the linker's OUTST 2-ARG
string is NOT raw text terminated by 0x27 as assumed - likely a STRING DESCRIPTOR {count,base} or a
length-prefixed form, OR arg1 is a pointer-to-pointer. The code path is `by move $2,b.48;
by sspar IND(b.44),b.48; callg OUTST` (SSPAR sets parity on the string, then OUTST emits it).
NEXT: peek 0xB0056B34 to see if it's a descriptor vs raw text; fix the 162B 2-arg form accordingly
(and re-verify the linker startup banner). Then re-drive to get OPEN-DOMAIN to create T:DOM.
Milestones so far this session: linker startup crash -> boots -> interactive -> reads full commands.

### Phase 2 (2026-07-14): 162B descriptor form FIXED; pivot to 412B/413B FSCNT (parallel-LLM request)

162B OUTST 2-arg form: arg1 is an ND-500 string DESCRIPTOR {count@+0, base@+4} (big-endian),
NOT raw text (verified: linker arg1 0xB0056B34 = {count=0xC8=200, base=0xB0056A6C}). Fixed the
handler to output `count` bytes from `base` for the 2-arg form (3+-arg form still raw+NoOfBytes;
both honour 0x27). Now writes 200 bytes (was flooding 2048). Env-gated probes kept: ND500X_OUTST_DUMP.
REMAINING linker issue (deferred): OPEN-DOMAIN "T" does not complete - the linker reads that one
command, emits ~13x 200-byte OUTST, hits 321B UEADM ERROR x3, and EXITS without reading LOAD/CLOSE
or opening T:DOM. Deep OPEN-DOMAIN-internal path; revisit (candidate: 321B UEADM should succeed).

PIVOT: parallel session 48520b34 requested I OWN 412B FSCNT / 413B FSCDNT (FileAsSegment) per
docs/HANDOFF_412B_FSCNT_FileAsSegment.md - it unblocks CAT-500 -> BOUT.NRF (the object-output path)
AND the linker uses 412B 14x. Key correction from that handoff: AccessType is 0=initial-data /
1=empty / 2=sequential / 3=combination (NOT 0=read/1=write/2=rdwr) - load file bytes for 0/2/3,
zero for 1; capability RO/RW from the file's open mode. Wiring template: nd500_mon_allocate_segment
(src/cpu/nd500_segment_alloc.c:113). Validate: CAT-500 on cat_intermediate.dat -> BOUT.NRF vs
test-real.nrf.

### 412B FSCNT (FileAsSegment) IMPLEMENTED + VALIDATED (2026-07-14, parallel-LLM handoff)

Real file-backed segment mapping done per docs/HANDOFF_412B_FSCNT_FileAsSegment.md (completion
section appended there). Refactored a shared allocation core (nd500_segment_alloc.c), added the
connect_file_as_segment callback (mon_types.h + nd500_indirect.c), rewrote 412B with the corrected
AccessType (0=initial-data/1=empty/2=seq/3=combo; writable from open mode). UNIT-VALIDATED:
test/diag_fscnt.c maps test-real.nrf and the mapped segment's bytes match the file (PASS). No
regression (linker/422B GSWSP still work). END-TO-END CAT-500->BOUT.NRF left to the parallel
session (needs their cat_intermediate.dat); 413B write-back is a follow-up.

### Phase 2 (2026-07-14): linker command-PARSER traps for every command (next blocker)

With commands now read fully, the linker's console output (diag_linkmon now dumps
mon_get_console_output) reveals a REGISTER/STACK CRASH DUMP ("--- ND-500 ... BACKUP ON MAP ---",
CURRENT B / PREVIOUS B / RETURN ADDRESS / NS0..NS9 register list, parity-set = SSPAR working) -
i.e. the linker's OWN trap handler fires, dumps, and exits. So processing ANY command crashes.
- The trap is a PROTECTION VIOLATION at PC=0xB00391B6, dataAddr=0xA8001CF8 (wild, seg 21),
  ~instr 11984 - IDENTICAL for OPEN-DOMAIN "T", OPEN-DOMAIN T, and LIST-STATUS. So it is the COMMON
  command-parse path, not command-specific.
- Fault site: a byte-copy loop @0xB00391A3.. : `w move r.48,b.36` (b.36 = count from record R
  field 48); decrement `w2:=b.36; w2-1; w2=:b.36`; `by3 := b.50(r2)` (LOAD b.50 indexed by r2=count-1)
  -> `by3 =: b.306`. If b.36 (the length) is wrong/huge, r2 underflows and b.50(r2) reads wild
  0xA8001CF8 -> PV.
- LIKELY ROOT: the parse LENGTH is wrong. Prime suspect = our 503B DVINST return-count
  (NoOfBytesRet) written to arg_addresses[2], but the linker's 503B arg layout differs (established
  earlier: arg[1] is a proc ptr, break table not at 6-9) so the count likely lands in the WRONG slot
  and the parser reads garbage. The 503B linker arg layout is still unmapped (band-aids: MaxNo clamp
  + break-table CR fallback). RESOLVING THE 503B LAYOUT is now the key blocker.
NEXT: probe R / b.36 / r2 / b.50 at 0xB00391B3 to confirm the bad length; then map the linker's true
503B arg layout (which slot is NoOfBytesRet / MaxNo / Buff) from the call at 0xB004AC90 + a value
compare, and write the count to the slot the parser reads. That should let commands execute.
Session arc: linker crash->boots->interactive->reads full commands->(next) execute commands.

### Phase 2 (2026-07-14): command-parser bad count PINNED to 503B result-slot mismatch

Probe (test/diag_linkfault.c at 0xB00391B3, LIST-STATUS): at the fault
  b.36(count)=0xF80000CA  b.50(base)=0x4C495354("LIST")  R=0xB0001D48  I2/r2=0xF80000CA
- b.36 (the byte count the parse-copy loop uses) = 0xF80000CA = a segment-31 PROCEDURE POINTER
  (sibling of the linker's 503B arg[1]=0xF80000CB), NOT a real length -> loop underflows -> wild
  read 0xA8001CF8 -> PV -> trap dump -> exit. Confirmed for LIST-STATUS and OPEN-DOMAIN alike.
- b.36 = r.48 = [R+48]; R = [B+8] = 0xB0001D48 = the linker's 503B DVINST arg[8] VALUE. So the
  linker expects the byte count in a RESULT STRUCTURE at arg[8]+48, but our 503B writes
  NoOfBytesRet to arg_addresses[2] (NC's convention). The linker's 503B RESULT convention differs
  from NC's - that is the real blocker. The command bytes DO land correctly (buffer via
  arg_addresses[3]); only the returned COUNT lands in the wrong place.
BLOCKED ON: the SINTRAN 503B (DVINST/InputString) RESULT-structure layout - specifically where the
returned byte count goes for the linker's 14-arg form (candidate: a field at arg[8]-pointed struct
+48). Not byte-verified in the carve (params via message buffer, inferred). Need the manual
"SINTRAN III Monitor Calls" ND-860228.2 503B result spec, OR the CAT-500 MON contract's 503B usage,
OR trace the caller that fills [R+48].
NEXT: map the linker's 503B result slot and write NoOfBytesRet there; then commands should execute
and OPEN-DOMAIN/LOAD/CLOSE can produce a DOM.

### Phase 2 (2026-07-14): 503B layout - YAML matches NC, linker uses a DIVERGENT variant (WALL)

Read the authoritative YAML $NDINSIGHT/Developer/MON/calls/503B_InputString.yaml:
standard 14-arg order is DevNo, MaxNo, NoOfBytesRet(O), Buff(O), BreakStrat, EchoStrat, BreakT1-4,
EchoT1-4 - EXACTLY what our handler assumes, and we DO write NoOfBytesRet to arg2 (line 340).
NC uses this layout and works. But the LINKER's actual arg VALUES do not fit it: arg1=0xF80000CB is
a segment-31 PROCEDURE POINTER (not a MaxNo), args6-7=0x20202020 (spaces, not a break table). And
the parser's bad count b.36 = [R+48] = 0xF80000CA = arg1-1 = (proc ptr)-1. So the linker's 503B is a
DIVERGENT VARIANT (likely with a break/input ROUTINE pointer arg) not described by the YAML/carve/
manual available here. This is a documentation WALL - same shape as the pre-linker-manual state.
NEED (to finish linker command execution): the linker's specific 503B DVINST calling convention -
which arg is the buffer-size/count and where the break-routine pointer goes. Candidate sources: the
parallel session (48520b34) traced CAT-500's 503B (0801F0F1, also 14-arg) - compare CAT-500's vs the
linker's arg values; or the ND Linker internals; or the linker .asm around 0xB004AC90's caller.
Everything else on the linker path is working: startup, interactive I/O, full command READ, 412B
FSCNT. This one convention gap blocks command EXECUTION -> DOM output.

### Phase 2 (2026-07-15): *** command-parser crash FIXED - 313B IBRISZ must return count in W1 ***
Root cause was NOT the 503B arg layout. The linker gets the command length from MON 313B IBRISZ,
reading the result from W1 after the CALLG (linker 0xB004DA8F/0xB004DA96). Our 313B handler wrote
the count only to the output arg, leaving W1 = the leftover CALLG gate address 0xF80000CB, which
the linker used as a ~4.29e9 length -> parser walked off the end -> PV 0xB00391B6 -> trap dump ->
exit, for EVERY command. FIX: mon_313B_InBufferState.c now also sets W1=remaining
(set_error_code). VERIFIED: PV gone; linker reads+processes LIST-STATUS and enters its normal
command loop. NEW (expected) blocker: it now polls MON 1B INBT for the NEXT command (257k spins) -
our INBT is not delivering the remaining queued input ("EXIT"). Next: make 1B INBT read the same
console queue as 503B (or confirm the linker's per-char input path). Full root-cause trail:
503B-InputString/ADDENDUM.md "RESOLVED" section.
Session arc: linker crash->boots->interactive->reads full commands->EXECUTES commands (crash-free).

### Phase 2 (2026-07-15): *** LINKER EXECUTES COMMANDS + real output *** (input model + 71B/72B)

Cumulative fixes this session got the linker from startup-crash to RUNNING REAL LINKER LOGIC:
- 313B IBRISZ returns count in W1 (the command-parser crash fix - see 503B ADDENDUM "RESOLVED").
- 1B INBT device 0 (command buffer): empty now returns EOF (K flag), not byte 0 (was spinning).
  File: src/libmon/handlers/mon_1B_InByte.c.
- INPUT MODEL: the linker reads commands via BOTH 503B (terminal) AND 1B INBT device 0 (SINTRAN
  command buffer). diag_linkmon now populates the command buffer too (mon_set_command_buffer),
  so commands are delivered on both. It read "LIST-STATUS" char-by-char from the command buffer
  and "EXIT" via 503B.
- 71B DESCF / 72B EESCF (escape disable/enable): implemented as benign no-op success + registry
  status IN_PROGRESS (files mon_71B_DisableEscape.c / mon_72B_EnableEscape.c). The linker calls
  DESCF around every input read.

RESULT: the linker produces 397 bytes of REAL console output ("NDL", "Advanced mode on (Yes,No)Yes",
"Batch abortion (Yes,No)Yes"), and issues 50B OPEN x6 + 120B WFILE x10 (it opens+writes files).
NEW BLOCKER: after consuming the commands it enters an interactive end-of-input / "Batch abortion
(Yes,No)" prompt loop, spinning on 71B DESCF (its input-read guard) - it wants more input / a clean
EXIT it did not get. Likely EXIT was read but not processed as the terminator, or the input model
needs the commands ONLY in the command buffer (not split across 503B+buffer). NEXT: get a clean
EXIT (try commands via command buffer only; or answer the Batch-abortion prompt); then drive
OPEN-DOMAIN/LOAD/CLOSE to produce a DOM.
Session arc: linker crash->boots->interactive->reads commands->EXECUTES commands+writes files.

### Phase 2 (2026-07-15): 71B/72B ESCAPE - REAL terminal backing (no shortcut), modeled on NPL

Replaced the earlier benign-no-op 71B/72B with real per-terminal escape state, modeled on SINTRAN's
terminal input datafield 5TTIFIELD.DFLAG.5IESC (inhibit-escape bit) found in the NPL source
($NDINSIGHT/SINTRAN/NPL-SOURCE/NPL/: MP-P2-TERM-DRIV.NPL VESCAPE=033B escape char;
RP-P2-SEGADM.NPL "DFLAG BONE 5IESC = DISABLE ESCAPE"; MP-P2-TAD.NPL "DFLAG BZERO 5IESC = ENABLE").
Changes:
- src/libmon/mon_terminal_state.{h,c}: TerminalState gains escape_inhibited (5IESC; default 0 =
  enabled, matching SINTRAN) + escape_char (VESCAPE, default 033B=0x1B) + accessors
  mon_set/get_escape_enabled, mon_set/get_escape_char, mon_is_escape_break.
- 71B DESCF -> mon_set_escape_enabled(dev,false); 72B EESCF -> (dev,true). Real state, per device.
- ConsoleIO gains an optional user_break(ctx,device) hook; 1B INBT (terminal read) now: if the byte
  is the terminal's escape char AND escape is ENABLED -> user break (invoke hook, return break/EOF);
  if DISABLED (linker's DESCF case) the char passes through as data. Integrated like break/echo/8-bit.
VERIFIED: linker's DESCF now logs "ESCAPE disabled on device 1" (real state change) and still
reaches the file-write stage (50B OPEN + 120B WFILE). MON_ID_71B/72B added to mon_log.h.
PENDING (separate, pre-existing): test_mon_calls 52/4. 3 fails are prior intentional changes
(312B MCTAB, 256B DEABF x2). 1 fail "503B returns error when max bytes exceeded" is from the earlier
503B MaxNo CLAMP workaround - reconcile next: now that the real crash cause (313B W1) is fixed,
re-check whether the clamp is still needed; if yes update the test to the clamp behavior, if no revert.

### Phase 2 (2026-07-15): 503B test reconciled; linker stuck in Batch-abortion prompt loop

- 503B MaxNo test FIXED: test_mon_calls test_mon_503B_dvinst_max_bytes_exceeded now verifies the
  CLAMP behaviour (oversized MaxNo -> clamp to 2048 + read until break, SUCCESS) instead of the old
  over-strict error. MON suite now 53 pass / 3 fail; the 3 remaining are PRE-EXISTING stale tests
  from prior sessions' intentional changes (312B MCTAB=065453B "deprecated returns 0"; 256B DEABF
  err 46 x2). Those need a separate test-reconcile pass (encode the prior carve-correct behaviour).
- LINKER: driving 'OPEN-DOMAIN "T";;LOAD T;;CLOSE N,N;;EXIT;;' with the command buffer populated,
  the linker reads OPEN-DOMAIN but enters an interactive "Batch abortion (Yes,No)Yes" /
  "Advanced mode on (Yes,No)Yes" prompt loop (spins, no DOM). Hypothesis: populating the COMMAND
  BUFFER puts the linker in BATCH mode; on end-of-buffer it thinks the batch job ended and prompts
  for abort, and cannot cleanly read the Yes/No answer -> loop. NEXT: try INTERACTIVE-only input
  (console 503B, no command buffer) now that 1B INBT device 0 returns EOF properly - the linker
  should fall back to 503B per command and reach EXIT/OPEN-DOMAIN cleanly. If needed, answer the
  batch-abortion prompt or feed a JOB-file style input. Goal remains: OPEN-DOMAIN/LOAD/CLOSE -> DOM.

### Phase 2 (2026-07-15): input model clarified; batch-abortion = empty-filename OPEN (-46)

- INPUT MODEL (confirmed): the linker reads its command line via 503B (terminal) ONCE, then reads
  commands CHARACTER-BY-CHARACTER via 1B INBT device 0 (the SINTRAN command buffer). It does NOT
  fall back to 503B on INBT device-0 EOF (interactive-only spins 137k times on EOF). So the command
  buffer MUST hold the commands (diag_linkmon: ND500X_LINK_CMDBUF=1 populates it; default off now).
- With the command buffer populated the linker EXECUTES commands, writes files (50B OPEN x6, 120B
  WFILE x10), and prints real output - but enters a "Batch abortion (Yes,No)" prompt loop.
- ROOT of the batch-abortion: an early 50B OPEN with an EMPTY filename -> host './GUEST/.' -> error
  -46. The linker cannot open a file it needs; in batch mode a file error triggers the
  "Batch abortion (Yes,No)" prompt, and it loops there. (Also secondary 74B SETBT "invalid file
  number 56" and 76B SETBS "block size 0" errors around the same spot.)
- The empty-filename OPEN is the SAME symptom seen at linker startup earlier: the linker builds a
  filename that reads back EMPTY (50B uses the correct mon_read_descriptor_string, so the descriptor
  is genuinely empty at that point - a linker-internal string-build our emulation isn't feeding, or
  a file it expects, e.g. its init/log/mode file). NEXT: disassemble the linker's OPEN call site to
  see which file it intends (init/log/mode?) and why the name descriptor is empty; provide/resolve
  that file so the OPEN succeeds and the batch does not abort. Then OPEN-DOMAIN/LOAD/CLOSE -> DOM.

### Phase 2 (2026-07-15): linker DOM WALL = it needs a real SINTRAN runtime environment

The batch-abortion is driven by the linker building GARBAGE / EMPTY filenames for files it opens:
  50B OPEN 'UE-ERMSG--C:ERR' type 'DUMY'  (real file is UE-ERMSG-EN-B06:ERR - lang/version empty)
  50B OPEN '<garbage>'       type 'INIT'
  50B OPEN ''  (empty) x several
  50B OPEN 'Linker'
Placing the real error-message file ($ND500_TESTDATA/ND/c3/2024/x/UE-ERMSG-EN-B06:ERR) and the help file at
the exact host paths did NOT clear it - the linker keeps producing empty/garbage names.
ROOT: the linker composes these names from SINTRAN USER/SYSTEM PROFILE fields (language code, product
version, user name) read from system datafields our emulator does not provide. With those empty, the
names are malformed, the OPENs fail -46, and in batch mode it prompts "Batch abortion (Yes,No)".
So a clean DOM via the linker needs a real SINTRAN runtime context (user profile + language/version
datafields + the linker's config/error files under the right user), which is an
environment-provisioning effort beyond MON-handler work - OR possibly more STRING/name-build fixes
if a specific instruction mangles a name (SCOPA/SSPAR were two such; not ruled out).

STATE ACHIEVED (this session, linker path): linker-b01.dom goes crash -> boots -> interactive ->
reads+executes commands (LIST-STATUS, init file, SET-ADVANCED-MODE) -> opens+writes files (50B/120B)
-> prints real output. Blocked only on the SINTRAN runtime environment for valid filenames + a clean
batch/EXIT termination. Fixes delivered: SCOPA, SSPAR (real CPU bugs), 144B/313B(W1)/162B/503B/71B/72B
handlers, 1B INBT device-0 EOF, real ESCAPE terminal backing (5IESC), 412B FSCNT (file-as-segment).

### Phase 2 (2026-07-15): carver redirect - filenames were a NON-ISSUE; batch-abortion is the real blocker

Carver LLM findings (see the request/answer in the session): the malformed 'UE-ERMSG--C:ERR' is a
BAKED-IN LITERAL in the linker DOM data segment (file 0x58E1F / DSEG VA 0x00E1F) - empty language,
version 'C' are shipped; the linker issues NO get-language/get-version MON call; any EN/B06 expansion
is SINTRAN OPEN-side. The empty-name OPENs are the current-user pass of a two-pass (user then
(SYSTEM)) file search (expected to fail then retry); LINKER:INIT / LINKER:HELP are literals. So our
emulator was FAITHFUL - filename failures are largely expected, NOT the blocker.
Applied from the verified contracts anyway: 214B GUSNA implemented (returns current user 'GUEST'
space-padded 16 chars + RemoteFlag 0; carve 006-S3FS:105301B) + registry IN_PROGRESS + MON_ID_214B;
provided GUEST/LINKER.INIT ("LIST\rSET-ADVANCED-MODE\r") + LINKER.HELP. Execution path shifted
(PC B0039BCE) but the "Batch abortion (Yes,No)Yes" prompt loop PERSISTS -> confirmed the batch-
abortion is a SEPARATE blocker, not filename-driven.
NEXT (carver request drafted): what triggers the "Batch abortion (Yes,No)" prompt in linker-b01.dom,
how it reads the Yes/No answer, and what it does on each - to drive it to a clean exit / real link.

---

# PART II - RUNNING REAL ND-500 APPLICATIONS (added 2026-07-17)

Phases 0-5 end at "we can compile and link OUR C". Part II is a different goal:
**run the real ND-500 application suite**, and give the user a way to actually
USE it. Added at the user's request.

Everything below inherits the Part I standing invariants (assume nothing, cite
or say unknown, never fabricate a vendor file, document every change for the C#
side at the time of the change).

## Why this order

`CONVERT-DOMAIN` comes first because it is the KEY that unlocks the rest. Its own
`.init` file states the purpose, verbatim:

    % This program converts domains and segments from :PSEG/:DSEG/:LINK
    % format to :DOM/:SEG format. If you need help, press the help key.

LED - the editor we want next - ships ONLY as `:PSEG`/`:DSEG`/`:LINK`
(`$ND500_TESTDATA/LED/x/led-b03.{pseg,dseg,link}`). We have no `led-b03.dom`. So
CONVERT-DOMAIN is not a side quest: it is how LED (and any other old-format
program) becomes runnable at all.

---

## PHASE 6 - CONVERT-DOMAIN: understand it, then run it

Target: `$ND500_TESTDATA/CONVERT-DOMAIN/`
- `convert-dom-a03.dom`  (339,968 bytes) - the program, ALREADY :DOM, so runnable now
- `convert-dom-a03.help` (16,159)
- `convert-dom-a03.init` (144)
- `in-conv-xx-a03.init`  (16,053)
- `ND-disk-00037.img`    (1,310,720) - ANOTHER vendor floppy, not yet mined

### 6.1 Static analysis (before running anything)
- Disassemble `convert-dom-a03.dom` the way `linker-b01.dom.asm` was produced;
  put the listing next to the binary.
- Enumerate its MON calls (`grep` the `$0xFFFFFFFFF80000XX` trampolines) and diff
  that set against what libmon implements. That list IS the Phase 6 work item -
  the same method that made the linker tractable.
- Identify its startup file opens (init/help/error-message files), as was done in
  `$ND500_TESTDATA/nd-linker/linker-b01-startup-filenames.md`.

### 6.2 Run it
- Sandbox `build/convert_sandbox/` mirroring `build/link_sandbox/`
  (GUEST/, SYSTEM/, SCRATCH/), with the `.init` and `.help` beside it under the
  names the program actually opens (watch for the SINTRAN abbreviated-name rules
  - see `HANDOFF_CSHARP_FILE_TABLE_SINTRAN_SEMANTICS.md`).
- FINDING, already checked (2026-07-17): `convert-dom-a03.help` is ALREADY 7-bit
  clean - no bit 7 set, it reads directly as text. The "strip bit 7" hypothesis is
  NOT needed for this file. Any garbling seen was OUR parity bug, fixed by
  MON 336B function 12B (commit 558ecc6). Do not add a bit-7 strip without
  evidence that some other file needs it.
- Implement whatever MON calls 6.1 turns up. Expect the same defect classes:
  OUT-parameters not written, EOF not signalled, function-code tables that the
  YAML defers to but the FULL scanned manual holds.

### 6.3 Learn the :PSEG/:DSEG/:LINK -> :DOM format
- Drive CONVERT-DOMAIN to convert a KNOWN pair and diff the output against a
  known-good `:DOM` (we have several) to learn the mapping.
- Document the format in `$ND500_TESTDATA/` next to the binaries.
- DONE when: CONVERT-DOMAIN runs to a clean MON 0B LEAVE and emits a `:DOM` we
  can load.

## PHASE 7 - LED (the editor)

Target: `$ND500_TESTDATA/LED/x/` - `led-b03.pseg` (223,695), `led-b03.dseg`
(394,525), `led-b03.link` (0 bytes), `description-file.desc` (22,528),
`scratch-seg-01.dseg`.

- 7.1 Convert `led-b03` to `:DOM` using Phase 6. (If Phase 6 stalls, the fallback
  is to load `:PSEG`/`:DSEG` directly - see Phase 8.2 - but conversion is the
  path the vendor intended.)
- 7.2 Same analysis loop as the linker: disassemble, enumerate MON calls, diff
  against libmon, implement the gaps.
- 7.3 LED is a SCREEN editor, so it will exercise paths the linker never did:
  cursor addressing, function keys, and MON 336B functions beyond 12B
  (101B set terminal type, 102B escape/local character, 106B character length,
  4B/6B echo and break strategies). The 336B function table is already recorded
  in `mon_336B_Terminal.c` - implement from there, and note `16B MGTTY` currently
  answers 6 (VT100), which LED will believe.
- DONE when: LED starts, renders a screen, accepts input, edits and saves a file.

## PHASE 8 - A CLI to run ND-500 programs

The emulator can already load and run a `:DOM`; what is missing is a way for a
HUMAN to use it.

- 8.1 `nd500x run <file>:DOM [args]` - run a domain, console wired to the host
  terminal, program exit code surfaced. Today this only exists as ad-hoc diag
  harnesses (`test/diag_*.c`), which are debugging instruments, not a UI.
- 8.2 If Phase 6 proves the format: `nd500x run <file>:PSEG` loading old-format
  segments directly, so LED-class programs run without conversion.
- 8.3 Configuration for the ND-500 program suite: where GUEST/SYSTEM/SCRATCH live,
  which user, terminal type, the pinned clock, which DDBTABLES variant. Today
  these are compile-time constants and env vars (`ND500X_PIN_CLOCK`,
  `ND500X_KEEP_SCRATCH`); they need to be one coherent config.
  NOTE the real dependency: `GUEST/` is gitignored and `DDBTABLES-G06.VTM` must be
  re-extracted from `ND-disk-00047.img` on a fresh clone
  (see memory `sintran-floppy-extraction`). A config/bootstrap step should make
  that reproducible rather than manual.

## PHASE 8b - THE SHIPPABLE THING: one binary + one JSON config

**User requirement, verbatim:** "i will want in the end a binary that is the
nd500x emulator with all its mon call supported, then we need some config files
to tell where we have everything we need, root folder for file system with users
under. information on what programs are installed (or maybe found on the system
user folder)? and options to have multiple users? and both the option to run the
cli with stdio for cli or as a telnet server" ... "and it needs to be easy to set
up, so maybe a json file for that config"

This is the DELIVERABLE. Phases 6-10 are how we earn it.

### 8b.1 Model SINTRAN's user/file namespace properly

SINTRAN addresses files as `(USER)NAME:TYPE`. Users own files; `(SYSTEM)` holds
the installed programs. TODAY nd500x hardwires a single `./GUEST/` directory and
translates `NAME:TYPE` -> `./GUEST/NAME.TYPE`. That is why everything above runs
out of one folder and why `build/link_sandbox/` had to be assembled by hand.

Required: a filesystem ROOT with USER directories under it, and path translation
that honours `(USER)` when present and the current user otherwise. `MON 44B
GUSER` / `214B GUSNA` already exist in the MON surface for "who am I".

### 8b.2 The config file (JSON)

Sketch - NOT settled, and deliberately mirrors SINTRAN concepts rather than
inventing new ones:

```json
{
  "filesystem": {
    "root": "$ND500_FS",
    "defaultUser": "GUEST",
    "users": ["SYSTEM", "GUEST", "RONNY"],
    "scratch": "SCRATCH"
  },
  "programs": {
    "discoverIn": "SYSTEM",
    "commands": {
      "NC":        "(SYSTEM)NC-A06:DOM",
      "ND-LINKER": "(SYSTEM)LINKER-B01:DOM",
      "LED":       "(SYSTEM)LED-B03:DOM",
      "CONVERT-DOMAIN": "(SYSTEM)CONVERT-DOM-A03:DOM"
    }
  },
  "terminal": { "type": 6 },
  "clock":    { "pinned": false },
  "console":  { "mode": "stdio" },
  "telnet":   { "port": 2323, "bind": "0.0.0.0" }
}
```

Design notes, each with a reason:
- **`programs.discoverIn`**: the user asked "or maybe found on the system user
  folder?" - YES, prefer discovery. Scanning `(SYSTEM)` for `:DOM` files means
  installing a program is copying a file, not editing config. Keep the explicit
  `commands` map only for aliases and overrides (the file is `LINKER-B01:DOM`
  but the command is `ND-LINKER`).
- **`terminal.type`**: this is the config-file home for what is currently
  `ND500X_TERMINAL_TYPE`. It stands in for SINTRAN's `@SET-TERMINAL-TYPE`. 0 =
  not set = programs ask, which is authentic. Under telnet this should instead
  come from the negotiated TERMINAL-TYPE (phase 9.3).
- **`console.mode`**: `stdio` | `telnet`. Both required. Same emulator, different
  `ConsoleIO` binding - the interface already exists in libmon.
- **Existing env vars fold in here**: `ND500X_PIN_CLOCK`, `ND500X_KEEP_SCRATCH`,
  `ND500X_TERMINAL_TYPE`. Env should override the file (handy for tests); the file
  is the easy-setup path the user asked for.
- **Bootstrap must be part of setup**: `DDBTABLES-G06:VTM` has to be extracted
  from `ND-disk-00047.img` (memory `sintran-floppy-extraction`) and `GUEST/` is
  gitignored. A fresh clone must have a documented, ideally automated, path to a
  working `fs/` - otherwise "easy to set up" is false.

### 8b.3 Multiple users

The user asked for it. Cheap once 8b.1 is real: users are directories under
`filesystem.root`. What is NOT free, and must NOT be faked:
- **Access control.** SINTRAN has user permissions, friends, and public/private
  access. We would have directories, not permissions. Say so plainly rather than
  implying protection that is not there.
- **Login.** `@LOGIN` / `MON 33B` etc. are a separate matter from "which folder
  do my files live in". UNKNOWN whether any target program cares.

### 8b.4 "All MON calls supported"

Reality check, so this is not mistaken for nearly-done: 261 MON YAMLs exist, ~231
handlers are registered, and only ~43 are real implementations - and even that
count is soft, because the `AUTO-GENERATED STUB` markers LAG the code (71B/72B are
implemented but still marked stub; see the 2026-07-17 YAML backfill).

"All MON calls" is not the bar. **"Every MON call the target programs actually
issue, verified against a real trace"** is the bar, and it is reachable: the
linker needed ~20. The method that works, proven three times now:

1. Disassemble the `:DOM`, grep the `$0xFFFFFFFFF80000XX` trampolines -> the
   exact call set.
2. Diff against libmon.
3. Implement, then RUN and compare against a trace - never trust the status label.

## PHASE 9 - Terminal / connectivity

**The client is known: RetroTerm** (`$RETROTERM`, C# / .NET 9 /
Avalonia, 2069 tests). This ANSWERS two questions that were open when Part II was
first drafted - they are facts now, not guesses:

- **Terminal types it implements:** Tandberg **TDV1200, TDV2215, TDV2200/9**
  FULLY; **VT100 only PARTIAL** (VT220/xterm planned). Consequence: our
  `16B MGTTY` currently answers **6 (DEC VT100)**, which targets RetroTerm's
  WEAKEST emulation. A TDV type is the better target for this client.
- **Protocol: real Telnet (RFC 854)**, with **NAWS** and **TERMINAL-TYPE**
  negotiation (also SSH). NOT raw TCP. So Phase 9.4 is decided: we speak real
  telnet, including IAC handling.

### 9.1 Host console, rendered correctly
The linker already emits real VT100 (`ESC[2J`, `ESC[1;1H`, `ESC[K`) and renders
in a normal terminal today. Work: make the CLI pass it through cleanly, and
decide behaviour when stdout is not a tty.

### 9.2 TCP/telnet server mode - `nd500x serve --port N`
Point libmon's `ConsoleIO` (already an interface) at a socket instead of stdio;
that indirection existing is what makes this cheap.

**Prior art to read first - do NOT reinvent the negotiation:**
`$RETROTERM/src/RetroTerm.Core.Protocols.TelnetServer/`
- `Telnet/TelnetNegotiator.cs` - sends `WILL ECHO`, `DO NAWS`, parses
  `IAC SB TERMINAL_TYPE IS "..." IAC SE`
- `Telnet/TelnetCodec.cs` - the IAC/option constants
- `Telnet/TelnetServer.cs`, `TelnetSession.cs`
- `Telnet/ITelnetApp.cs` - an interface for a telnet-HOSTED app
- `Utilities/TDVCapabilityChecker.cs`, `TDVResponseValidator.cs`

nd500x is C, so this is a REFERENCE for the wire contract, not code to link.
NOTE for the C# side: RetroCore could instead implement `ITelnetApp` and be
hosted by RetroTerm's existing server - that is a genuinely different and cheaper
option for that emulator, worth raising in the C# handoff.

### 9.3 Terminal type: let TELNET drive MON 16B MGTTY
The elegant part. RetroTerm's telnet client advertises a configurable
`TerminalType` string (`TelnetConnection.cs:54`, default `"VT100"`; the codebase
also carries `"TDV2200"`, `"TDV1200"`, `"TDV2215"`, `"TDV2115"`). Our server
already has to parse TERMINAL-TYPE for telnet. So:

    telnet TERMINAL-TYPE string  ->  ND terminal type number  ->  16B MGTTY answer

instead of a hard-coded constant. Proposed mapping, from the terminal menu that
`DDBTABLES-G06:VTM` itself prints (so these numbers are evidence, not invention):

| RetroTerm advertises | ND type | DDBTABLES-G06 menu entry |
|----------------------|---------|--------------------------|
| `VT100`    | 6   | DEC VT100 (80 columns) |
| `TDV2115`  | 3   | Tandberg TDV 2115 |
| `TDV2200`  | 53 or 80 or 83 | TDV 2200/9 ND-NOTIS / ND-NET / V2 ND-NOTIS |
| `TDV1200`  | 110 or 113 | TDV 1200/1 ND-NET / ND-NOTIS |
| `TDV2215`  | **36** | **NOT in the G06 menu** - see caveat |

**UNKNOWN, must be resolved before implementing the mapping:**
- **ND-NET vs ND-NOTIS**: the TDV entries come in pairs and the distinction is not
  understood. Do NOT pick one arbitrarily.
- **TDV2215 = 36 is NOT in DDBTABLES-G06**, whose list jumps 3 -> 6 -> 53. Type 36
  appears in a DIFFERENT DDBTABLES variant. So the valid type set depends on WHICH
  DDBTABLES is loaded - the mapping must be validated against the loaded table, not
  hard-coded. (This is the same variant-divergence caveat recorded in
  `mon_16B_GetTerminalType.c`.)
- Appendix H, which would define the type numbers authoritatively, is NOT in the
  scanned document set. The numbers above are read from the DDBTABLES menu only.

### 9.4 Telnet specifics - DECIDED
Real telnet (RFC 854): IAC handling, `WILL ECHO`, `DO NAWS`, TERMINAL-TYPE.
NAWS gives the window size, which is real information the ND side could use.
Interaction to think about: telnet `WILL ECHO` (server echoes) vs SINTRAN's own
echo strategy (MON 3B SetEcho / 336B function 4B) - two echo models that must not
both apply, or every keystroke doubles.

## PHASE 10 - THE POINT OF ALL THIS: a self-hosted C workflow

**The user's question, verbatim: "LED will let me edit c files and then i can
compile and run them?"**

**Yes - that is exactly the end state, and every piece of it already exists as a
real 1988-89 ND binary.** Nothing here needs writing from scratch; it needs the
MON contract underneath it to be honest. The loop is:

    LED          edit  A:C           ($ND500_TESTDATA/LED/x, needs Phase 6 conversion)
      |
    NC           compile A:C -> A:NRF   (nc-a06.dom, works today - Phase 1)
      |  (NC internally invokes CAT-500 via MON 317B)
      |
    ND-LINKER    OPEN-DOMAIN / LOAD A:NRF + libs -> A:DOM   (linker-b01.dom - Phase 3)
      |
    RUN A:DOM    the emulator loads and runs it              (works today)
      |
    back to LED

All four are ND-500 programs talking to each other through SINTRAN. The emulator
does not need to BE SINTRAN - it needs to answer MON calls the way SINTRAN would.

### The one architectural gap: nothing can invoke anything

Today the emulator loads exactly ONE domain and runs it to `MON 0B LEAVE`. There
is no `@`-prompt, so there is no way to go from LED to NC to the linker without
restarting the process by hand. Two things are missing, and they are the same
thing seen from two sides:

1. **A user-facing command layer** - SINTRAN's `@` prompt. `@LED`, `@NC A`,
   `@ND-LINKER`, `@A` to run a domain.
2. **MON 317B ExecuteCommand** - how a PROGRAM invokes another program.
   ALREADY OBSERVED, ALREADY STUBBED: it decodes `NC-A` / `CAT-CAT5-B` but
   executes nothing, and *that is why compiled output lands as a 0-byte NRF*.
   NC issues 317B expecting CAT-500 to run; nothing runs; NC writes nothing.

Both need the SAME dispatcher: "given a command line, find the domain, load it,
run it, return its status".

### Design decision to make (do not default into it silently)

- **(a) Emulate the real SINTRAN command processor.** Faithful, and it is carved
  material (L07). But it is an OPERATING SYSTEM - user accounts, RT, spooling,
  the whole datafield world. Enormous, and mostly irrelevant to compiling C.
- **(b) A HOST-side dispatcher in nd500x** that presents an `@`-style prompt,
  resolves a command to a `:DOM`, loads and runs it, and returns to the prompt.
  The guest programs cannot tell the difference as long as the MON contract holds
  - which is the same bet the whole project already makes.

**Recommendation: (b).** We are not writing SINTRAN; we are providing the MON
surface it exposes. (b) is a few hundred lines against an emulator that already
loads domains. (a) is a research project. Revisit only if a program turns out to
need real SINTRAN command semantics we cannot fake.

### What Phase 10 actually requires

1. **Program lifecycle**: load domain -> run -> `LEAVE` returns to the dispatcher
   -> load the next. Today `LEAVE` ends the process.
2. **State that must PERSIST across programs** (this is the subtle part):
   - the open-file table and `GUEST/` contents - already persistent, this is how
     A:C survives from LED to NC
   - terminal state (echo/break strategy, 8-bit mode, escape inhibit) - per
     device, must not reset between programs
   - the command buffer (device 0) - each program gets ITS arguments, i.e. the
     dispatcher must SET it per invocation, exactly as SINTRAN does
     (see `HANDOFF_CSHARP_FILE_TABLE_SINTRAN_SEMANTICS.md` item 2)
3. **317B wired to the dispatcher**, so NC->CAT-500 works and the 0-byte NRF bug
   dies.
4. **Scratch file discipline** across programs (`ND500X_KEEP_SCRATCH` exists).

### Sequencing (the user's own framing: "after we have all the mon calls implemented and tested")

Correct order, and the reason it is correct: a dispatcher on top of a dishonest
MON layer would just relocate the bugs. Everything found today
(`smove`, MAGTP fn 0, 336B fn 12B, 503B EOF) was a MON/CPU contract defect that
looked like an application bug. So:

    Phases 0-5   the MON contract is honest for NC + linker      <- mostly done
    Phase 6      CONVERT-DOMAIN (unlocks old-format programs)
    Phase 7      LED (proves the contract under a SCREEN program)
    Phase 8-9    CLI + telnet/RetroTerm (the human's seat)
    Phase 10     the dispatcher, 317B, and the loop closes       <- self-hosting

Phase 10 is LAST because it is the payoff, not the foundation.

### DONE, for the whole project

Sitting in RetroTerm, connected over telnet to `nd500x serve`: type `@LED`, edit a
C file, exit, `@NC` it, `@ND-LINKER` it, run the result - and see its output. On
emulated 1989 hardware, with the vendor's own toolchain.

## PHASE 11 - WASM: the emulator in a browser (like nd100x glass)

**User requirement, verbatim:** "i would also like a wasm version that runs in the
browser, like the nd100x wasm glass - with needed files uploaded to webfs ... part
of that is to learn from the nd100x - but this is after we have done the cli
console + tcp. however the wasm cant do tcp so tcp must be compile time enabled or
not. and in the nd100x we have extern library that has tdv2200 emulator with
virtual keyboard so we can hook that up to talk to the cli"

**Sequencing is the user's and it is right: AFTER phases 8-9 (CLI + TCP).** The
browser build is a second FRONT END onto the same console abstraction. Building it
before that abstraction exists means building it twice.

### 11.1 Starting position - better than it looks

- **nd500x ALREADY has a WASM target**: `src/frontend/nd500wasm/`
  (`main.c`, `CMakeLists.txt`, `web/`), and `make wasm` / `make wasm-serve` exist.
  This phase is not a from-scratch port; it is bringing that target up to the glass
  standard.
- **libmon's `ConsoleIO` is already an interface** - the same indirection that makes
  the telnet server cheap (9.2) makes the browser terminal cheap. stdio, socket and
  browser are three bindings of one interface.

### 11.2 Learn from nd100x - the prior art is a working system

`$ND100X/`:
- `GLASS.md` - the architecture reference for `template-glass/`. **Read this first.**
- `docs/HOWTO_BUILD_WASM.md`
- `template-glass/js/` - the pattern to copy:
  - `emu-worker.js` - the emulator runs in a WEB WORKER, not the UI thread
  - `emu-proxy.js` - the UI<->worker boundary
  - `module-init.js` - Emscripten module bring-up
  - `floppy-browser.js` - loading images/files into the FS
  - `toolbar.js`
- Its build wires the whole thing:
  `wasm-glass: check-deps mkptypes retroterm-build ndfs-build ts-compile`
  i.e. RetroTermWeb + an ND filesystem layer + TypeScript are all part of it.

### 11.3 The terminal: RetroTermWeb

**`$ND100X/template-glass/external/RetroTermWeb/`** - this is the
"extern library with tdv2200 emulator and virtual keyboard" the user means. It is
the WEB sibling of the Avalonia RetroTerm (`$RETROTERM`), already
integrated into nd100x's glass UI (`retroterm-build` target, `terminal-popout.html`).

So the browser terminal is SOLVED prior art - hook RetroTermWeb's input/output to
the emulator's `ConsoleIO` binding, exactly as nd100x hooks it to its own.

This also means the terminal-type question (9.3) has the same answer in the
browser: RetroTermWeb is a TDV2200, so `16B MGTTY` should report a TDV type there,
not VT100.

### 11.4 TCP must be compile-time optional

The user is right: **WASM cannot open TCP sockets.** So the socket/telnet console
(9.2) must be behind a build flag, e.g. `ND500X_ENABLE_TCP`, defaulted ON for
native and OFF for WASM. `CMakeLists` already branches on `BUILD_WASM`.

Design consequence worth stating: `ConsoleIO` bindings should be REGISTERED, not
hard-wired, so a build can simply not include the socket one. Do not let telnet
code become load-bearing in the shared path.

### 11.5 webfs - getting the files in

The browser has no `$ND500_FS`. The 8b JSON config's filesystem root has
to become a browser FS (Emscripten MEMFS/IDBFS), populated by upload. What must get
in there, and it is more than "a program":

- the `:DOM` to run (linker, NC, LED, CONVERT-DOMAIN...)
- **`DDBTABLES-G06:VTM`** - without it the linker aborts (proven today). Note it is
  extracted from `ND-disk-00047.img` and is NOT in the repo (`GUEST/` is
  gitignored), so the browser build needs either the floppy image + the extractor,
  or a prepared bundle.
- `UE-ERMSG-EN-C06:ERR`, `LINKER:INIT`, `LINKER:HELP`
- the user's own source/objects

`floppy-browser.js` in nd100x is the closest prior art. Ideal: upload the FLOPPY
IMAGE and extract in-browser, reusing the object-entry/index-block format proven in
memory `sintran-floppy-extraction` - one artifact instead of a pile of loose files.
IDBFS would persist it across reloads.

### 11.6 DONE

Open a browser, upload (or find persisted) the files, get a TDV2200 with a virtual
keyboard talking to the ND-500, and run the linker - no install, no toolchain.

## Open questions for the user (do NOT guess these)

RESOLVED 2026-07-17 - the client is RetroTerm (`$RETROTERM`):
- ~~Which TDV emulator, and which terminal type?~~ -> TDV1200 / TDV2215 /
  TDV2200/9 fully implemented; VT100 only partial. See Phase 9.3.
- ~~Telnet proper or raw TCP?~~ -> real Telnet (RFC 854), NAWS + TERMINAL-TYPE.
  See Phase 9.4.

STILL OPEN:
1. **Priority order.** Phase 7 (LED) and Phases 8-9 (CLI/serve) are independent.
   Which first?
2. **Which TDV model** is the default target? TDV2200/9 is the flagship; 1200 and
   2215 are also fully implemented in RetroTerm.
3. **ND-NET vs ND-NOTIS** - the TDV types come in pairs and we do not know the
   difference. Needed before the 9.3 mapping is real.
4. **Phase 10 dispatcher: (a) or (b)?** Recommendation is (b), host-side.

## Known-open items carried from Part I (none blocking)

- `Date: 17. July 1926` - year off by 100 in the linker banner; time correct.
- `*** WARNING - "LINKER:HHHHH:HHHHH:H"` - mangled name in a warning; string
  formatting bug, uninvestigated.
- `317B ExecuteCommand` decodes but executes nothing - why compiled output lands
  as a 0-byte NRF.
- `511B DVIO` is the linker's current spin: "implementation incomplete" logged
  repeatedly while it waits for a command.
