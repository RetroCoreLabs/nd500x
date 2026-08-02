# ND linker `LOAD <obj>` — error 52 investigation (blocker #1, LOAD object load)

## *** SOLVED 2026-07-19: root cause = DEABF returned a VERSION-LESS name ***
The linker's `LOAD` rejects a resolved object name that lacks a SINTRAN **version**.
Our `256B DEABF/FullFileName` returned `B:NRF` (no version); the Monitor Calls manual
(ND-860228, 256B FULLFILENAME) states DEABF returns "the directory, the user, the file
name, the file type, AND the version." Appending the default version `;1` to FOUND
files (`B:NRF;1`) makes the linker **open the object** (`50B OPEN ./GUEST/B.NRF` as file
102) and proceed — the days-old `(-677:52)` is gone.

- Fix: `src/libmon/handlers/mon_256B_FullFileName.c` — the four
  found-file `snprintf` branches now append `;1`. Only FOUND files get it (a not-yet-
  created file still returns not-found/err-46, so NC's create-if-missing is unchanged).
- Verified: NC gate `dom_nc_compile` 4/4 still pass. `mon_calls` unit test fails but that
  is PRE-EXISTING (fails identically with the fix reverted — its DEABF fixture file is
  absent in the test env, hitting the not-found path; plus an unrelated 312B check).
- How it was found: the earlier "gate B0040D75 / b.0x49 / K" framing was a dead end. A
  MON-level trace showed that right after DEABF success the linker does NO file open — it
  jumps straight to reading the error-message file and printing `(-677:52)`. So the reject
  is a check on DEABF's OUTPUT. The manual named the missing field (version); a one-line
  experiment (`;1`) confirmed it by making `50B OPEN` fire.

### NEW frontier (next blocker, separate): "no current domain" — CHARACTERIZED 2026-07-19
MON-level trace of the full run (deterministic) pins the behaviour:
- **OPEN-DOMAIN "A-TEST"** (quoted create): `50B OPEN` create A-TEST.DOM (file 101) ->
  `120B WFILE` 4096-byte empty header at block 0 -> `256B DEABF` 'A-TEST:DOM;1' ->
  `43B CLOSE` file 101. It then OPENs the ERROR-MESSAGE file (UE-ERMSG) and RFILEs it to
  format the `(0000:00)` success line. It NEVER reopens A-TEST.DOM. So OPEN-DOMAIN
  persists an empty domain file + prints success, but leaves NO open/current domain and
  (apparently) does not set the linker's in-memory current-domain pointer.
- **LOAD B:NRF** (with the version fix): `50B OPEN` B.NRF (file 102) -> `43B CLOSE` ->
  current-domain check FAILS -> `*** ERROR - Command not valid when no current domain or
  segment exists. (0054:67)`. Object handling is now fully correct; the fault is purely
  the missing current-domain state.

Manual truth (ND-860289 ND Linker): the workflow IS `OPEN-DOMAIN "X"` then `LOAD`
(p.197-200), and LOAD loads into "the current domain" (p.958). OPEN-DOMAIN must make the
new domain current. `0xB0048CC8` (=1) is NOT that state (already ruled out).

NEXT PROBE: find the linker's in-memory current-domain variable (the one LOAD's (0054:67)
gate reads) and determine why OPEN-DOMAIN's quoted-create path does not set it — candidates:
(a) the create path diverges from the open-existing path before the "set current" store;
(b) OPEN-DOMAIN expects to read back a VALID domain header (ours is 4096 zeros) to build
the descriptor and skips "set current" when the header is empty/invalid;
(c) a MON call in the create path returns a value our emulation gets wrong. Approach: watch
memory writes during OPEN-DOMAIN, then find which the LOAD gate reads as zero.

### (historical) NEW frontier note (superseded by the characterization above): "no current domain"
With the object now opening, `LOAD B:NRF` hits:
`*** ERROR - Command not valid when no current domain or segment exists. (0054:67)`
`OPEN-DOMAIN "A-TEST"` DOES create the `.DOM` file (`50B OPEN` file 101 + `120B WFILE`
4096-byte header), but it does not leave the linker's internal **current-domain/segment**
context set for the subsequent `LOAD`. Next step: trace what `OPEN-DOMAIN` must set (an
in-memory current-domain pointer/flag) that our emulation leaves unset, or whether
`OPEN-DOMAIN` needs a follow-up step. NOT caused by the `;1` fix (reproduces from a clean
domain create). The `0xB0048CC8` global (=1) is not this state.

---
### (historical) original error-52 investigation follows


Full path: `docs/LINKER-LOAD-ERROR52-INVESTIGATION.md`
Date: 2026-07-18
Run from: `build/link_sandbox` (NOT nc_sandbox)
Driver: `../bin/diag_linkdrive $ND500_TESTDATA/nd-linker/linker-b01.dom 'OPEN-DOMAIN "A-TEST";;LOAD B:NRF;;EXIT;;'`
(delete `GUEST/A-TEST.DOM` between runs — quoted OPEN-DOMAIN create errors -62 if it already exists)

## Status
- FIXED this session: the LOAD **field re-prompt loop** and `EXITB:NRF` input
  concatenation. Root cause: DEABF success did not set `W1/i1 = 0`, so `i1` kept
  the MON trampoline's leftover call-target `0xF80000AE` (0xF8000000 + 0xAE;
  0xAE = 256B octal = DEABF's own routine number), which the linker read as a
  nonzero error code. Fix: `ctx->set_i1(ctx->cpu, 0)` on success in
  `src/libmon/handlers/mon_256B_FullFileName.c`.
  Verified: OPEN-DOMAIN still creates+reads the domain; EXIT exits cleanly
  (MON 0B LEAVE); no infinite re-prompt.
- STILL BLOCKED: `LOAD B:NRF` never `50B OPEN`s `B.NRF`. After DEABF resolves the
  name, the linker raises **error 52** (`"(-677:52)"`, blank message) and returns
  to the command prompt. The object is never opened/read/relocated.

## The error-52 site (byte-level, verified by BREAK/disasm)
Routine `B0040C3C` (ents $0x100) is the LOAD command's name handler. It:
1. Parses the command-line string into a **stage counter `b.0x49`** by scanning
   for delimiter chars (before calling DEABF):
   - `;` (0x3B) -> `b.0x49 = 2`  (B0040C79 / B0040C7E)
   - `)` (0x29) -> `b.0x49 = 3`  (B0040C86 / B0040C8B)
   - `:` (0x3A) -> sets `b.0x4C` flag, does NOT touch b.0x49 (B0040CA9..)
   - `.` (0x2E) -> `b.0x49 = 4`  (B0040CBC / B0040CC1)
   The parsed string is the WHOLE command line at `b.0x18` (e.g. "LOAD B:NRF",
   dumped at 0xB0048FEC = `4C 4F 41 44 20 42 3A 4E 52 46` + spaces). "B:NRF" has
   a `:` but no `.`, so `b.0x49` stays 1.
2. `B0040D44`: sets `b.0xD8 = 0xAE` and `call B004D4F4` (the universal MON-invoke
   dispatcher) -> runs DEABF (256B). DEABF now returns success (i1=0, K=1).
3. `B0040D75`: `if -k go B0040D7D`.
   - K SET  -> `call B0040C44` = PROCEED (open/load the object).
   - K CLEAR -> `B0040D7D`: `by comp2 b.0x49,$4; if >< go B0040D85(ERROR-52)`.
     So K-clear only survives if `b.0x49 == 4`.
   Observed at B0040D75 (LOAD round): `ST1=0x62` (K CLEAR), `b.0x49 = 1`,
   `I1 = 0` -> falls to B0040D7D -> b.0x49 != 4 -> **error 52**.

## Why K is clear at B0040D75 (the crux, UNRESOLVED)
The dispatcher `B004D4F4` runs the callg (DEABF) at `B004DA08`, then
`B004DA13 if k go B004DBF1` (K set) / `B004DBD0` (K clear). BOTH result paths
converge at `B004DBF6` and end in a plain **`ret`** (`B004DBFA`), which CLEARS K
(RET clears K; RETK sets it — confirmed by the Ifkret work, commit 20a9082). So
the dispatcher structurally returns K CLEAR regardless of the MON's K. Therefore
at B0040D75 K is ALWAYS clear, and success MUST come via `b.0x49 == 4`.

But `b.0x49` is parsed from the raw command line "LOAD B:NRF", which has no `.`,
so it is 1. Feeding `LOAD B:NRF.` (explicit period) makes the parser reach
`b.0x49 = 4` and suppresses error 52 — BUT it then takes an early branch that
**skips DEABF and the load entirely** (no open, no read), so the period is a red
herring, not a fix.

## The open contradiction (needs carve/manual truth)
On real hardware `LOAD B:NRF` + CR must load the object. Yet:
- the dispatcher returns K clear (by `ret`), and
- the command-line parse of "LOAD B:NRF" yields `b.0x49 = 1` (no `.`).
So neither B0040D75 success route is satisfied by our emulation. Something about
the real command-buffer contents, the command terminator, or the dispatcher's
K/return handling differs from ours. Candidates to verify against the carve /
ND-500 linker + SINTRAN command-processor docs:
1. **Command terminator**: what byte terminates a linker command line in the
   buffer the parser scans? Our DVINST stores CR (0x0D) and the linker's buffer
   ends up space-padded with NO terminator; the parser wants a specific
   delimiter. Does SINTRAN's terminal input deliver/convert to `.` (0x2E), CR,
   or something the parser treats as end-of-command that also advances/【bypasses】
   the b.0x49 gate?
2. **Dispatcher K contract**: does `B004D4F4` really return K clear on MON
   success, and is B0040D75 therefore meant to pass via `b.0x49 == 4` — meaning
   the parsed buffer on real hardware DOES contain the delimiter that yields 4?
3. **What is linker error 52?** (context "-677"). Its message slot is blank in
   `UE-ERMSG-EN-C06.ERR`; it is a linker-internal code, not a SINTRAN file error.
   Knowing its meaning would confirm whether this is a syntax/parse error or a
   load-step error.

## Round-2 confirmations (2026-07-18) — the contradiction is airtight
Every emulator-side assumption was verified; none is a bug:
- **`RET` correctly clears K** (`src/cpu/instructions/CALL/Ret.c:43`; matches the
  manual and the linker's RET=success / RETK=error convention). So the dispatcher
  `B004D4F4` ending in `ret` (B004DBFA) returning K-clear is CORRECT.
- **`B004D4F4` returns K-clear on every path**: entry does `jumpg B005482C`
  (arg-count jump table); all arg-count slots (0..N args, callgs at
  B004D9D9/E6/F6/DA08) converge to `B004DBF6 -> ret`. Confirmed by disasm.
- **`:` is the correct SINTRAN type separator** (not `.`): `LOAD B.NRF` makes
  DEABF resolve name `B.NRF` + type `NRF` -> host `./GUEST/B.NRF.NRF` (double,
  not found). `LOAD B:NRF` correctly resolves `B:NRF` -> `./GUEST/B.NRF` (found).
- **`LINKER.INIT` uses CR (0x0D) line terminators**, not `.` — and `LIST` /
  `SET-ADVANCED-MODE` work CR-terminated. So `.` is NOT a universal command
  terminator; only the LOAD routine `B0040C3C` demands `b.0x49==4`.
- **The parse runs exactly once** during LOAD (single `B0040C59` hit), on the
  valid name `B:NRF` (the `:` handler fires), correctly yielding `b.0x49=1`.

Net: K is correctly clear AND b.0x49 is correctly 1 for a valid `LOAD B:NRF`, so
BOTH success routes at B0040D75 are unavailable -> error 52 — yet the real linker
loads `LOAD B:NRF`. The resolution is therefore in the linker's LOAD command
contract, NOT in any single MON/CPU behaviour we can see from here. This needs
the ND-500 linker (ND-500 Loader/Linker manual) and/or the S3FS carve — see the
three carve questions above, plus:
4. What is the FULL control flow of the LOAD command handler between reading the
   command line and opening the object file? Specifically: what must be true at
   B0040D75 (routine B0040C3C) for LOAD to proceed to `call B0040C44` — is it K,
   `b.0x49==4`, or is B0040C3C a pre-parse whose error-52 is caught/retried by a
   caller we have not traced? Where is the `50B OPEN` of the object `.NRF`?

## Session 2026-07-19 — independent re-derivation + Loader/Linker manual located
Re-verified the whole chain from scratch (fresh disasm of the loaded
`linker-b01.dom`, virtual addresses via `nd500x --debug` `d 0x...`); every step
matches the round-2 analysis above. Nothing new is broken; the contradiction is
confirmed a THIRD independent way.

- LOAD handler dispatcher call + gate, disassembled directly:
  ```
  B0040D44: w move   #174,b.216            ; b.216 = 0xAE = 256B = DEABF routine no.
  B0040D5C: call     B004D4F4,$10,b.216,IND(b.196),...   ; universal MON-invoke helper
  B0040D75: if-kgo   $8   -> B0040D7D      ; if NOT K -> b.0x49 check
  B0040D77: call     B0040C44,$0           ; K SET -> PROCEED (load the object)
  B0040D7D: by comp2 b.73(=0x49),$4        ; K CLEAR -> need b.0x49 == 4, else err 52
  ```
  There is NO instruction between the call (D5C) and the gate (D75) that could set
  K from the returned W1. The gate reads K exactly as the dispatcher leaves it.
- Dispatcher `B004D4F4` slot for this callg is `B004DA08`; `B004DA13 if k go
  B004DBF1 / B004DBD0`, both converge to `B004DBF6: w1:=b.12; w1=:r.12; ret` (K
  clear). The K-set arm (B004DBF1) returns b.68 (DEABF's W1) as an error code; the
  K-clear arm (B004DBED `w stz b.12`) returns 0. So with our DEABF returning K=1 +
  W1=0 the dispatcher returns 0/K-clear regardless — i.e. the DEABF K flag alone
  does NOT change the error-52 outcome (both polarities give K-clear at D75).
  => The DEABF success K polarity is a red herring for error 52; the gate needs
     `b.0x49 == 4`, which is set ONLY by a `.` in the pre-DEABF command-line scan.

- LOADER/LINKER MANUAL LOCATED: `$NDINSIGHT/Reference-Manuals/
  ND-60.136.04A ND-500 Loader Monitor.md`. It documents the OLD Linkage-Loader
  (NLL: `SET-DOMAIN`, `LOAD-SEGMENT`). Our binary is the NEW NDL (`- ND LINKER,
  Version B01  10. January 1989 -`, internal version 66.251), whose verbs are
  `OPEN-DOMAIN` / `LOAD` (confirmed in this binary's own
  `build/link_sandbox/GUEST/LINKER.HELP`; the manual lists `LOAD-SEGMENT` as the
  "Related old Linkage-Loader command"). The manual does NOT document the NDL's
  internal `b.0x49` command parser, so it does not resolve the gate. A successful
  LOAD is documented (LINKER.HELP LOAD example) to print
  `Program:.....B Pxx  Data:.....B Dxx`.

- OPEN-DOMAIN behaviour fully traced (fresh domain, stale A-TEST.DOM removed):
  `50B OPEN "A-TEST":DOM` (create, quoted) -> `120B WFILE` 4096-byte header page
  -> `256B DEABF` echo -> `43B CLOSE`. Domain state is kept internally; the .DOM
  file is closed between commands. This all SUCCEEDS. Note: our quoted-create
  returns 076B "already exists" if `A-TEST.DOM` is present, but the NDL
  OPEN-DOMAIN spec (LINKER.HELP) says an existing domain is ERASED — a separate,
  lower-priority correctness gap (only bites on re-link of an existing domain).

- LOAD path (fresh domain): reads `LOAD B` via 511B DVIO -> `256B DEABF` resolves
  `B`->`B:NRF` (K=1, i1=0, host `./GUEST/B.NRF` found) -> error `(-677:52)`. It
  never `50B OPEN`s `B.NRF`. The `117B RFILE FileNo=65 block 4` seen next is the
  linker reading the still-open error-message file `UE-ERMSG-EN-C.ERR` to format
  the (blank) error text, NOT reading B.NRF.

### What `b.0x49 == 4` MEANS (new, ruled a dead end)
Tested `LOAD B.NRF` (period) fresh. The `(-677:52)` is GONE — the period sets
`b.0x49 = 4` and the gate passes — but the linker then prints
`*** ERROR - Remote file-name ...`. So the `.` is NOT a type separator: the NDL
reads `B.NRF` as ND COSMOS **remote-file syntax** (remote system `B`, file
`NRF`). Confirmed by planting `GUEST/B.NRF.NRF` so our (colon-based) DEABF
resolves `B.NRF`->`B.NRF:NRF`: the linker still rejects it as a remote name.
=> `b.0x49 == 4` is the REMOTE-FILE branch, irrelevant to a local object. `:` is
the correct type separator; `LOAD B` / `LOAD B:NRF` are the correct forms. The
period is definitively NOT a fix.

### Microcode check — the K-flag hypothesis is CLOSED (ND-5000 microcode)
Checked the ND-5000 microcode directly (`$ND5000UC/microcode/
MICRO-5800-A30.md`, opcode table):
```
000701 RET   | ... D,SC14 K,ZRO ... |  -> RET  clears K   (micro-op K,ZRO)
000702 RETK  | ... D,SC14 K,ONE ... |  -> RETK sets   K   (micro-op K,ONE)
000706 RETB  | ... K,ZRO ...        |  -> clears K
000707 RETBK | ... K,ONE ...        |  -> sets   K
000774 SETK=K,ONE  000775 CLRK=K,ZRO
```
So `RET` genuinely CLEARS K on real hardware — our `Ret.c` is correct, and the
dispatcher `B004D4F4` really does return K-clear on every arm on real HW too.
=> K being clear at B0040D75 is NORMAL, not a divergence. The whole "why is K set
on real HW" question is a DEAD END: it is not set, and is not supposed to be.

### Re-read of the gate with K ruled out (corrects the earlier framing)
With K-clear being correct, re-disassembling the branch targets shows B0040D75/
B0040D7D is NOT the error-52 emitter:
```
B0040D75: if-kgo -> B0040D7D     ; K-clear (normal) path
B0040D77: call B0040C44          ; K-set only: entd; l=:b.184; w1=:b.12; ret  (setup, then falls to D7D)
B0040D7D: by comp2 b.73,$4       ; b.0x49 == 4 (REMOTE) -> B0040D83 bi1 clr; ret (early return)
B0040D81: if>< -> B0040D85       ; b.0x49 != 4 (NORMAL) -> CONTINUES into the parser stage-machine
```
B0040D85+ is the command-line PARSER continuation (scans `;`=59, `:`=58, `)`=41,
`'`=39 via `jumpg b.73`), i.e. the same stage machine as B0040C3C. So for a normal
`LOAD B:NRF` the handler PROCEEDS past B0040D75; error 52 is emitted somewhere
DOWNSTREAM of the parser, not at this gate. The prior "B0040D75 gate => error 52"
framing is therefore imprecise: the gate is passed; the fault is later.

### Corrected next step
Runtime-trace the ACTUAL executed instruction path from the DEABF return through
to where error code 52 is produced (the diag BREAK/trace harness can do this,
non-destructively) instead of static branch-reading. The K-flag and both manuals
are exhausted; the remaining work is a linear trace of the parser/load
continuation to the exact instruction that sets error 52.

### (superseded) earlier framing: the single K question
The `b.0x49 == 4` route is now RULED OUT (it is the remote-file branch, see
above). So on real hardware, for a local `LOAD B` / `LOAD B:NRF`, the ONLY way
past B0040D75 is **K SET**. The universal MON dispatcher `B004D4F4` (which invokes
DEABF at slot `B004DA08`) structurally ends in `ret` (K clear) on every arm, so
in our model K is always clear at the gate. The whole blocker therefore reduces
to one question for the S3FS carve / a real-HW trace:

> After `call B004D4F4` returns to the LOAD handler at B0040D75, why is K SET on
> real hardware for a successful DEABF? Does the dispatcher's success arm actually
> `retk` (not `ret`)? Does the LOAD handler set K from the returned W1 in a way we
> mis-decoded? Or does DEABF itself return via a path that leaves K set through
> the dispatcher?

Concretely, a real-HW trace need only report, at `B0040D75` for `LOAD B:NRF`:
`ST1` (is K set?), `b.0x49`, and `I1`. Everything reachable from the emulator +
both ND manuals (Loader/Monitor ND-60.136.04A and Monitor Calls ND-860228.2) is
now exhausted.

## Session 2026-07-19 (part 2) — runtime trace executed (the "corrected next step")
Ran the non-destructive KWATCH / CALLTRACE / FTRACE harness over the LOAD round
(instr window ~191530..194000). New, verified facts:

- **The float-subsystem rebase does NOT affect this blocker**: `LOAD B:NRF` still
  errors `(-677:52)` at the same place, byte-identical. Confirms the two are unrelated.
- **Post-DEABF control flow is now fully traced** (CALLTRACE):
  ```
  191530 B004DA11 (DEABF callg returns, K set, I1=0)   <- DEABF success for B:NRF
  191540 -> B0040D75  (the gate; PASSED, as the round-2 static analysis predicted)
  191553..193088  scan loop B0040D89->DAA->DCE->DDA->B0040E53(loopi b.56,#127)->D89
                  i.e. a FIXED 127-iteration scan of the command buffer
  193088 B0040E6D: RET -> returns to caller at B003D0E8
  193088+ tail: B003D0E8 -> B003CFDA -> B0034AF1 / B0036ACD / B0040711 ->
                 B0049903.. (B0049xxx/B004Axxx)  = error MESSAGE FORMATTER
  ```
- **The LOAD line handler (B0040C3C/D75/127-byte scan) is a SUBROUTINE**; B0040E6D is
  its `ret`. Its caller is at **B003D0E8**, which then dispatches via `jumpg b.532`
  (routine B003CFDA) into the tail.
- **"code 32" was a RED HERRING**: the earlier KSET `I1=0x20 @B003F864` is `by1 sfill
  b.40` at B003F861 space-filling a 156-byte line buffer (`w move #156,b.40`) before
  writing the error text. B003F855+ is the error-line FORMATTER, not the decision.
- **52 (0x34) is NOT held in any I-reg** across the tail window (FTRACE_VAL=0x34, no
  hit), so `(-677:52)` is computed/formatted, not passed as a bare register code.

### Corrected next step (narrowed)
The error DECISION is upstream of the B003F855 formatter, in the tail
`B003D0E8 -> B003CFDA(jumpg b.532) -> B0034AF1 / B0036ACD / B0040711 -> B0049903`.
Next probe: CALLTRACE/step that chain to find the routine that PRODUCES the -677/52
pair (watch the memory word the formatter reads, or the arg passed into B0049903),
and determine what "-677:52" denotes (SINTRAN error record vs linker-internal). The
127-fixed-count scan (`loopi b.56,#127`) is also worth validating against the real
command-buffer length semantics.

### Additional RULED-OUT items (2026-07-19 part 3)
- **Global `0xB0048CC8` is NOT the reject.** B0036ACD does `comp2 [0xB0048CC8],$1;
  if< go` (a domain/mode flag check). BREAK at B0036AD3 during the LOAD round
  (instr 193113) dumps `0xB0048CC8 = 0x00000001`, so `1 < 1` is FALSE -> the check
  PASSES and B0036ACD proceeds. The flag is correctly set by OPEN-DOMAIN
  (WATCH shows 1->FFFFFFFF->1 during startup, settled at 1). Not the cause.
- **B0049903 / B004A9xx / B004A69C are STRING/name utilities**, not the error
  reporter; they feed the B003F816/B003F855 line formatter (entered instr 193963
  `B003F7AA -> B003F816`). B0036ACD returns early W1=1 only if the length routine
  B0040711 yields `b.28-b.24+1 <= 0`; in the LOAD round B0040711 returned W1=0
  (valid), so that early-out did NOT fire either.
- Net: gate, "code 32", the 0xB0048CC8 domain flag, and the B0040711 length early-out
  are all RULED OUT. The `-677:52` decision is still unpinned within the
  B003D0E8/B0032Bxx/B003A2xx/B00401FC tail. This needs a patient single-step of the
  ONE conditional that routes to the reporter (or the carve meaning of -677:52) --
  a dedicated session; breadth-first call-tree mapping is not converging.

## Reproduce
```
cd build/link_sandbox && rm -f GUEST/A-TEST.DOM
ND500X_PIN_CLOCK=1 ../bin/diag_linkdrive $ND500_TESTDATA/nd-linker/linker-b01.dom \
  'OPEN-DOMAIN "A-TEST";;LOAD B:NRF;;EXIT;;' 2>&1 | grep -iE 'DEABF|\(-677|round='
# BREAK at the gate:
ND500X_PIN_CLOCK=1 ND500X_NOLOG=1 ND500X_BREAK_PC=0xB0040D75 \
  ND500X_BREAK_DUMP_BREL=0x44 ND500X_BREAK_DUMPLEN=16 ../bin/diag_linkdrive ... 
```

---

## UPDATE 2026-07-19b: 257B FOPEN ruled OUT as the blocker; gate + control flow pinned

Investigated the "no current domain" blocker fresh with the diag_linkdrive
disassembler (new ND500X_DISASM_AT/BASE/LEN feature) and calltrace.

### Real, carve-grounded side-finding (LANDED, see SYNC-MON-257B-FOPEN-PRESENT-IN-SINTRAN-L.md)
During OPEN-DOMAIN, after creating the .DOM the linker does `312B MOINF` on
**257B** and, if present, CALLS `257B FOPEN`. Carve PROVED 257B IS present in
SINTRAN L: `MCTAB[257B] @ 044-S3IDPIT (L-VSX-500) = 0x928A (0111212B), non-zero`.
We were reporting it absent (STUB). Implemented FOPEN + flipped status + MOINF
now reports present. 40061/40061 + nc 4/4 pass. BUT this is the error-message-file
lookup path (FOPEN queries UE-ERMSG-EN-C:ERR), and LOAD STILL fails (0054:67).
So FOPEN is ORTHOGONAL to current-domain. Ruled out.

### The current-domain check is a PURE IN-LINKER MEMORY VARIABLE (no MON call)
Extracted the MON calls the LOAD command makes: `256B DEABF` (B:NRF;1) ->
`50B OPEN` B.NRF (file 102) -> `263B GDEVT` -> `76B SETBS 4000` -> `43B CLOSE`
file 102 -> `71B DESCF` -> error. LOAD opens+validates+closes the object, then
checks current-domain and errors BEFORE loading. NO MON call queries or sets
current-domain. So OPEN-DOMAIN must set an in-linker memory variable that LOAD
reads, and our run leaves it zero/unset.

### Gate mechanism pinned (disasm)
Command dispatcher region (disasm of B003D0A0..):
```
B003D0E2: call  0xB0040C3C   ; LOAD command handler
B003D0E8: if-kgo +8          ; if K SET -> skip error (go B003D0F2)
B003D0EA: call  0xB003CFDA   ; ELSE -> error handler (K clear == failure)
```
So LOAD errors iff the handler path returns K clear. B0040C3C is the command-line
PARSER (ents #256; scans for ';' 59, ')' 41, ':' 58, '.' 46). It returns fine;
execution then flows PARSER -> `B0040D5C call B004D4F4` (universal MON dispatcher,
runs the DEABF/OPEN/SETBS/CLOSE above) -> `B0040D75` -> scanner loop
`B0040DAA->B0040DCE->B0040E53(127-byte scan)->B0040D89` (~5 passes) ->
`B0040E6D ret -> B003D0E8` -> `B003D100 ret -> B0034AF1` (error region) ->
`B0034AF1 -> B0036ACD -> B0040711 -> B0049903` (error-message FORMATTER, fills a
156-byte line). Confirms the doc's earlier tail mapping.

### NEXT PROBE (for a fresh session - this is a multi-layer disasm job)
The current-domain FAILURE is decided somewhere in the B0040D75 / B004D4F4
region BEFORE the scanner unwinds to B0034AF1. Two concrete tasks:
1. Disassemble the OPEN-DOMAIN command handler's SUCCESS tail (the create path,
   around B004CAxx after the create/WFILE/CLOSE) and find the STORE to the
   current-domain variable - confirm whether that store executes in our run and
   what value it writes. Leading hypothesis (candidate b): the linker derives the
   current-domain id/pointer from the domain header, and our header is 4096 zeros
   -> derived id 0 -> LOAD's "!= 0" gate fails. A real create assigns a non-zero
   domain number even for empty content.
2. Disassemble the LOAD-side current-domain test (in B0040D75/B004D4F4 region) to
   get the EXACT data address it reads, then diff that address between
   "OPEN-DOMAIN;LOAD" and bare-"LOAD" runs. If identical (zero) in both,
   OPEN-DOMAIN's set-current never executed -> find why (early branch / value
   from the empty header).
Tooling: diag_linkdrive now has ND500X_DISASM_AT=<pc> ND500X_DISASM_BASE=<pc>
ND500X_DISASM_LEN=<n> (one-shot disasm on first PC hit, after feeds) and
ND500X_CALLTRACE_LO/HI (prints PC discontinuities in an instr window). Rebuild
diag_linkdrive with gcc (NOT make) against build/lib/*.a.

---

## UPDATE 2026-07-19c: banner formatter is SHARED (success+error); decision is upstream in the frame

Kept hunting the current-domain variable with the diag_linkdrive disassembler +
BREAK_PC frame/global dumps. New concrete facts:

### The command executive B0034AF1 and the SHARED banner formatter
After a command handler returns, control reaches the command executive at
**B0034AF1**. Relevant disasm:
```
B0034AFC: w1 =: <abs 0xB0048F60>     ; store W1 to a linker global
B0034B02: test W1 ; if!=0 go +311
B0034B07: comp2 <abs 0xB0048CE0>, $11 ; if==go ...
B0034B10: comp2 <abs 0xB0048CE0>, $13 ; if!=go +135
B0034B1A: test b.144 ; if=go +59 -> B0034B57
B0034B57: bmove b.80,r.20 ; call 0xB0036ACD   ; <-- SINTRAN result banner formatter
```
CRITICAL: BREAK_PC=B0034B57 fires on BOTH commands - OPEN-DOMAIN (instr 123040,
prints "(0000:00)") AND LOAD (instr 191660, prints "(0054:67)"). So B0034B57 ->
B0036ACD is the SHARED "(NNNN:NN)" banner formatter, NOT an error-only path. The
earlier assumption that reaching B0036ACD == error was WRONG.

### The success/error distinction is in the FRAME/regs, not the globals
Dumped the linker global control block 0xB0048C80..0xB0048DC0 at BOTH B0034B57
hits: BYTE-IDENTICAL between the success (OPEN-DOMAIN) and error (LOAD) runs.
So the "(0000:00)" vs "(0054:67)" code is carried per-call, not in a global:
  - OPEN-DOMAIN: B=B0001BD4  b.140=0x0C(12)  I2=0x12(18) I4=0x0C(12)
  - LOAD:        B=B0002274  b.140=0x05(5)   I2=0x04(4)  I4=0x03(3)
The "no current domain" (0054:67) status is therefore DECIDED inside the LOAD
command handler (upstream of B0034AF1) and passed down as the status code the
banner formats. 0054B=44dec, 67B=55dec - not yet found verbatim in a reg/frame,
so it is looked-up/computed, not a raw field.

### Linker global control block map (0xB0048Cxx, byte-verified)
```
0xB0048CA0: "ERROR-CODE:VAR" ... "LANGUAGE:VAR"   (config-variable name strings)
0xB0048CE0: 0x0000000D (=13)   command code (comp'd to 11/13 in the executive)
0xB0048CEC: 0x00000001
0xB0048CFC: 0x0000001F (31)  0xB0048CF3: 0x00000028 (40)
0xB0048D68: ptr 0xB0048D18   0xB0048D74: ptr 0xB0048D18   (list head/tail, cnt 0x4F)
```
These are the SAME in both runs (OPEN-DOMAIN did persist some state, but none of
it is the success/error discriminator).

### NEXT (fresh session): find the conditional that sets status 67 inside LOAD
The current-domain test + status assignment is inside the LOAD handler between
its entry and the return to the executive - i.e. in the B0040C3C-parse ->
B004D4F4-MON-dispatch -> B0040D75-scanner flow, or a sub-call thereof. Probe:
BREAK at the LOAD-handler return and walk backwards, or watch for the first write
of 0x37(55)/0x2C(44) or an error-index into the frame during the LOAD window
[173830,191660]. The current-domain "handle" the handler reads is the real
target; OPEN-DOMAIN must set it and our create path does not.

---

## UPDATE 2026-07-19d: error code computed downstream; OPEN-DOMAIN handler call-chain located

Continued the current-domain-handle hunt. Ruled out the downstream layers and
located the OPEN-DOMAIN handler body for next session.

### The (0054:67) code is computed in the formatter, NOT stored in a global
- Executive `b.144` (`test b.144; if=go B0034B57`) is NOT the error decider - it
  is the "run post-command autojob" flag:
  `b.144 = ((W1==13)|b.440) & ((cmd==11)|(cmd==13)) & (global[0xB0048CC0]==1)`
  (disasm B0034A80..B0034AD5). Both success and error reach the shared banner.
- b.80 passed to the banner formatter B0036ACD is a descriptor
  `{ptr=0xB0048FEC, n1, n2}`: OPEN-DOMAIN {12,19}, LOAD {5,9}. 0xB0048FEC is just
  the ECHOED COMMAND TEXT buffer ("OPEN-DOMAIN \"A-TEST\"" / "LOAD B:NRF"), not
  the code.
- Dumped 0xB0048F60..0xB0049000 (linker status block) at BOTH banners:
  byte-identical except the echoed command. So (0054:67) is computed by the
  message path from the error condition, carried in regs/frame - the DECISION is
  upstream in the LOAD handler, already made before the banner.

### OPEN-DOMAIN "create" MON sequence + handler location (for the set-current store)
`OPEN-DOMAIN "A-TEST"` create (instr window [104184,173830]):
  50B OPEN "A-TEST":DOM (create, file 101) @ handler-called wrapper B004E80F/E874,
  the create OPEN itself at instr 132725; then 76B SETBS 4000, 120B WFILE 4096
  bytes block0 (the on-disk header), 256B DEABF, 43B CLOSE.
Call chain INTO the create (calltrace): ... B0020487 -> B0007195 -> B0002D92 ->
B000489B -> B0006D0C ... B0022688 ... B00226C5 -> B000061B -> B00026A2 -> ...
B000064C -> **B004E80F** (OPEN wrapper). The OPEN-DOMAIN command HANDLER lives up
this chain in the B0020487 / B0022688 / B0006Dxx region. NEXT SESSION: disassemble
that handler's body from its dispatch entry through its tail; find (a) the store
of the current-domain handle/pointer to a linker global and whether it executes,
and (b) the matching read on the LOAD side. Leading hypothesis unchanged: the
handle is derived from the domain and ends up 0 (our create writes a 4096-zero
header; check whether the WFILE buffer @0xB0013130 is structured on real HW).

Harness: diag_linkdrive now also supports ND500X_BREAK_AFTER=<instr> to gate
ND500X_BREAK_PC past startup (the create OPEN is only the A-TEST one after 104184).
