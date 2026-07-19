# ND linker `LOAD <obj>` — error 52 investigation (blocker #1, LOAD object load)

Full path: `/home/ronny/repos/nd500x/docs/LINKER-LOAD-ERROR52-INVESTIGATION.md`
Date: 2026-07-18
Run from: `/home/ronny/repos/nd500x/build/link_sandbox` (NOT nc_sandbox)
Driver: `../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom 'OPEN-DOMAIN "A-TEST";;LOAD B:NRF;;EXIT;;'`
(delete `GUEST/A-TEST.DOM` between runs — quoted OPEN-DOMAIN create errors -62 if it already exists)

## Status
- FIXED this session: the LOAD **field re-prompt loop** and `EXITB:NRF` input
  concatenation. Root cause: DEABF success did not set `W1/i1 = 0`, so `i1` kept
  the MON trampoline's leftover call-target `0xF80000AE` (0xF8000000 + 0xAE;
  0xAE = 256B octal = DEABF's own routine number), which the linker read as a
  nonzero error code. Fix: `ctx->set_i1(ctx->cpu, 0)` on success in
  `/home/ronny/repos/nd500x/src/libmon/handlers/mon_256B_FullFileName.c`.
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

- LOADER/LINKER MANUAL LOCATED: `/mnt/e/Dev/Ronny/NDInsight/Reference-Manuals/
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
Checked the ND-5000 microcode directly (`/mnt/e/Dev/Ronny/ND5000UC/microcode/
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

## Reproduce
```
cd /home/ronny/repos/nd500x/build/link_sandbox && rm -f GUEST/A-TEST.DOM
ND500X_PIN_CLOCK=1 ../bin/diag_linkdrive /mnt/d/ND/500/nd-linker/linker-b01.dom \
  'OPEN-DOMAIN "A-TEST";;LOAD B:NRF;;EXIT;;' 2>&1 | grep -iE 'DEABF|\(-677|round='
# BREAK at the gate:
ND500X_PIN_CLOCK=1 ND500X_NOLOG=1 ND500X_BREAK_PC=0xB0040D75 \
  ND500X_BREAK_DUMP_BREL=0x44 ND500X_BREAK_DUMPLEN=16 ../bin/diag_linkdrive ... 
```
