# CONVERT-DOMAIN (convert-dom-a03) MON-call analysis under nd500x

Program under test: `/mnt/d/ND/500/CONVERT-DOMAIN/convert-dom-a03.dom`
(same binary staged in SYSTEM as `/home/ronny/ND500USERS/SYSTEM/CONVERT-DOM-A03.DOM`).
Purpose (from `/mnt/d/ND/500/CONVERT-DOMAIN/convert-dom-a03.init` and `.help`):
convert old-format domains (`:PSEG` / `:DSEG` / `:LINK` + a `:DESC` description file)
into new-format `:DOM` / `:SEG` files.

Emulator: `/home/ronny/repos/nd500x/build/bin/nd500x` (used as-is, NOT rebuilt).
MON handlers audited: `/home/ronny/repos/nd500x/external/ndmonlib/src/handlers/*.c`
Registry / status: `/home/ronny/repos/nd500x/external/ndmonlib/src/core/mon_registry.c`
Dispatch trap: `/home/ronny/repos/nd500x/external/ndmonlib/src/core/mon_dispatch.c:173`

--------------------------------------------------------------------------------
## 1. Headline result

- The LED `:PSEG/:DSEG` -> `:DOM` conversion does NOT complete under nd500x.
- The blocker is NOT a missing MON call. Across every dynamic run, ZERO
  unimplemented-MON traps fired (`grep -c "UNIMPLEMENTED MON CALL"` = 0).
- The actual blocker is a file-name / file-type TRUNCATION bug in the emulator
  SINTRAN path layer, surfaced through MON 256B DEABF (FullFileName):
  - `SINTRAN_MAX_NAME = 16` holds only 15 chars, truncating the 16-char name
    `DESCRIPTION-FILE` to `DESCRIPTION-FIL`.
  - `SINTRAN_MAX_TYPE = 4` holds only 3 chars, truncating the 4-char type
    `DESC` to `DES` (this also truncates `PSEG/DSEG/LINK`).
  - Defined at `/home/ronny/repos/nd500x/external/ndmonlib/include/ndmon/mon_path.h:19-21`.
- The set of MON calls that WOULD still need implementing (referenced in the
  binary, currently unimplemented) is listed in section 3, but none of them is
  reached before the truncation bug aborts the run.

--------------------------------------------------------------------------------
## 2. Static disassembly method + MON-call enumeration

### Method (verified, reproducible)

1. The emulator loads the DOM and reports the code (PROG) segment file range:
   `nd500x --dom <dom> --disasm 64` prints
   `DOM Load: Seg[1] PROG: file=0x00001000..0x0002E493 size=185492`.
   So the PROG image is file offset `0x1000`, length 185492, mapped to
   virtual base `0x08000000` (kernel-mode DOM: PROG VA base 0x08000000).
2. Extract raw PROG bytes:
   `dd if=<dom> of=prog.bin bs=1 skip=$((0x1000)) count=185492`
   -> `work/prog.bin` (session scratch, not preserved)
   (prog.bin offset N == virtual address 0x08000000 + N).
3. A SINTRAN MON call on ND-500 is a CALL into the "SINTRAN window" at absolute
   address `0xF8000000 + N`, where N is the MON number. Two encodings occur in
   this binary:
   - `C3 F8 00 hh ll ...`  (CALL-with-immediate, opcode 0xC3) -- the compiler's
     own MON thunks, e.g. `C3 F8 00 00 63 04 ...` = `call $0xF8000063,$4,...`
     which is MON 0x63 = 143B.
   - `CF F8 00 hh ll ...`  (opcode 0xCF form) -- used inside the linked-in
     ND-SHELL / runtime, e.g. `CF F8 00 01 43 ...` = MON 0x143 = 503B.
   N = `(hh<<8) | ll`, so MON numbers >= 0x100 (400B+) use `hh = 0x01`.
   (My first pass matched only `C3 F8 00 00 xx` and therefore MISSED every
   call >= 400B and the CF-form calls; the corrected scan searches for the
   trampoline address `F8 00 {00,01} ll` regardless of the preceding opcode.)
4. Every candidate was confirmed to be a real, instruction-aligned CALL by
   disassembling at its address with
   `nd500x --dom <dom> --addr <VA> --disasm 12` and reading back the
   `call $0xF80000xx,$argc,...` line.

### Distinct MON numbers referenced in the PROG image (static, 44)

Plus 2 more (41B, 313B) seen only dynamically -> total distinct used = 46.
"opcode" = the byte preceding the trampoline address (C3 = compiler thunk,
CF = runtime/shell form).

MON numbers (octal) referenced statically:
`0B 1B 2B 3B 4B 11B 13B 16B 17B 30B 43B 50B 62B 71B 72B 73B 74B 76B 104B
113B 117B 120B 143B 144B 162B 251B 254B 256B 257B 262B 263B 312B 320B 321B
334B 336B 412B 503B 504B 505B 511B 512B 513B 514B`

Example call-site addresses (first occurrence, virtual):
- 143B RSIO  @ `0x08024CD7`  (`C3 F8 00 00 63 04 ...`)
- 503B DVINST@ `0x08023113`  (`C3 F8 00 01 43 ...`) and `0x08025B6E` (CF form)
- 511B DVIO  @ `0x08023142`  (`C3 F8 00 01 49 ...`)
- 512B XMSGCallA @ `0x0802...` (CF form, 1 ref)
- 513B XMSGCallB @ `0x08009A6A` (CF form, 14 refs)
- 412B FSCNT @ `0x0802C07B`
- 320B UELogin @ `0x0802CB59`

--------------------------------------------------------------------------------
## 3. Gap table (MON number, name, implemented?, evidence)

Status source: `mon_registry.c`. Meaning (per nd500-mon skill and
`mon_dispatch.c:173`): only `MON_STATUS_NOT_IMPLEMENTED` and "not in registry"
are actually UNREACHABLE (the dispatcher logs "UNIMPLEMENTED MON CALL" and never
calls the handler). `IN_PROGRESS` and `VALIDATED` both run.

| MON (oct) | Name (handler)              | Registry status  | Reachable? | Handler file present |
|-----------|-----------------------------|------------------|------------|----------------------|
| 0B   | ExitFromProgram (LEAVE)      | VALIDATED        | yes | yes |
| 1B   | InByte (INBT)                | VALIDATED        | yes | yes |
| 2B   | OutByte (OUTBT)              | VALIDATED        | yes | yes |
| 3B   | SetEcho                      | VALIDATED        | yes | yes |
| 4B   | SetBreak (BRKM)              | VALIDATED        | yes | yes |
| 11B  | GetBasicTime                 | VALIDATED        | yes | yes |
| 13B  | ClearInBuffer (CIBUF)        | VALIDATED        | yes | yes |
| 16B  | GetTerminalType (MGTTY)      | VALIDATED        | yes | yes |
| 17B  | SetTerminalType (MSTTY)      | VALIDATED        | yes | yes |
| 30B  | GetOwnRTAddress              | VALIDATED        | yes | yes |
| 41B  | ReadObjectEntry (ROBJE)      | VALIDATED        | yes | yes (dynamic-only ref) |
| 43B  | CloseFile (CLOSE)            | VALIDATED        | yes | yes |
| 50B  | OpenFile (OPEN)              | VALIDATED        | yes | yes |
| 62B  | GetBytesInFile               | VALIDATED        | yes | yes |
| 71B  | DisableEscape (DESCF)        | IN_PROGRESS      | yes | yes |
| 72B  | EnableEscape (EESCF)         | IN_PROGRESS      | yes | yes |
| 73B  | SetMaxBytes                  | VALIDATED        | yes | yes |
| 74B  | SetStartByte (SETBT)         | VALIDATED        | yes | yes |
| 76B  | SetBlockSize (SETBS)         | VALIDATED        | yes | yes |
| **104B** | **SuspendProgram (HOLD)** | **NOT_IMPLEMENTED** | **NO** | file exists, registered NOT_IMPLEMENTED |
| 113B | GetCurrentTime (CLOCK)       | VALIDATED        | yes | yes |
| 117B | ReadFromFile (RFILE)         | VALIDATED        | yes | yes |
| 120B | WriteToFile (WFILE)          | VALIDATED        | yes | yes |
| 143B | ExecutionInfo (RSIO)         | VALIDATED        | yes | yes |
| 144B | DeviceFunction (MAGTP)       | IN_PROGRESS      | yes | yes |
| 162B | OutString                    | IN_PROGRESS      | yes | yes |
| **251B** | **CopyPage**              | **NOT_IMPLEMENTED** | **NO** | file exists, registered NOT_IMPLEMENTED |
| **254B** | **GetErrorDevice**        | **NOT_IMPLEMENTED** | **NO** | file exists, registered NOT_IMPLEMENTED |
| 256B | FullFileName (DEABF)         | VALIDATED        | yes | yes (see section 5 - buggy) |
| 257B | OpenFileInfo                 | IN_PROGRESS      | yes | yes |
| 262B | GetSystemInfo (CPUST)        | VALIDATED        | yes | yes |
| 263B | GetDeviceType                | IN_PROGRESS      | yes | yes |
| 312B | CheckMonCall                 | VALIDATED        | yes | yes |
| 313B | InBufferState (IBRISZ)       | IN_PROGRESS      | yes | yes (dynamic-only ref) |
| **320B** | **UELogin**               | **NOT IN REGISTRY** | **NO** | **no handler file** |
| 321B | UEAdministrator (UEADM)      | VALIDATED        | yes | yes |
| **334B** | **GetErrorMessage**       | **NOT_IMPLEMENTED** | **NO** | file exists, registered NOT_IMPLEMENTED |
| 336B | Terminal (IOMTY)             | IN_PROGRESS      | yes | yes |
| 412B | FileAsSegment (FSCNT)        | VALIDATED        | yes | yes |
| 503B | InputString (DVINST)         | VALIDATED        | yes | yes |
| 504B | OutputString (DVOUTS)        | VALIDATED        | yes | yes |
| **505B** | **GetTrapReason**         | **NOT_IMPLEMENTED** | **NO** | file exists, registered NOT_IMPLEMENTED |
| 511B | DVIO                         | IN_PROGRESS      | yes | yes |
| **512B** | **XMSGCallA**             | **NOT IN REGISTRY** | **NO** | **no handler file** |
| **513B** | **XMSGCallB**             | **NOT IN REGISTRY** | **NO** | **no handler file** (14 static refs) |
| **514B** | **ND500TimeOut**          | **NOT_IMPLEMENTED** | **NO** | file exists, registered NOT_IMPLEMENTED |

Nine referenced MON calls are effectively UNIMPLEMENTED (would trap):
**104B, 251B, 254B, 320B, 334B, 505B, 512B, 513B, 514B.**

Note on 41B / 313B: these fired at real program PCs (`0x08025DD1`, `0x08025E6A`)
in the dynamic trace but their trampoline reference was NOT found as an inline
constant in the PROG byte-scan. INFERRED: their target address is formed at
runtime (register/computed CALL) rather than an inline `F8 00 00 xx` literal.
This is unverified; it does not affect the result (both are implemented and
returned SUCCESS). Confirming experiment: single-step the CALL at those PCs in
the DAP/CLI debugger and read the effective target register.

--------------------------------------------------------------------------------
## 4. Dynamic run

### Sandbox (never touches /home/ronny/ND500USERS)

Root: `sandbox` (session scratch, not preserved)
- `SYSTEM/` = copy of `/home/ronny/ND500USERS/SYSTEM` (holds `CONVERT-DOM-A03.DOM`,
  `.HELP`, `.INIT`, `LED-B03.PSEG/.DSEG`, VTM/ERR support).
- `GUEST/` = the old LED domain to convert, copied from `/mnt/d/ND/500/LED/x/`:
  `LED-B03.PSEG`, `LED-B03.DSEG`, `LED-B03.LINK`, `DESCRIPTION-FILE.DESC`.
- `nd500x.ini` -> `sintran-root = <sandbox>`, `user = GUEST`.

### Command used (command-line-argument form, cleanest)

```
printf 'CONVERT-DOM-A03 LED-NEW LED-B03 NO YES\nEXIT\n' \
  | ND500X_MONLOG=1 /home/ronny/repos/nd500x/build/bin/nd500x \
      --monitor --config ./nd500x.ini > run3.out.txt 2> run3.monlog.txt
```
Args: destination `LED-NEW`, source `LED-B03`, include-linked `NO`,
progress `YES`. Passing parameters on the command line bypasses the interactive
ND-SHELL (per `convert-dom-a03.help`, section CONVERT-DOMAIN / SHELL).

Evidence files (all under the sandbox dir above):
- `run1.out.txt` / `run1.monlog.txt` - bare start, confirms banner + `.INIT` read.
- `run2.out.txt` / `run2.monlog.txt` - interactive `CONVERT-DOMAIN ...` fed on
  stdin. The ND-SHELL full-screen (VTM) editor mangled the line
  (`CONVERT-DOMAINLED-NEWLED-B03 NO YES`), giving `Sintran Error: (0000:56)`.
  This path also produced 100x `43B CLOSE -> ERROR` and `503B DVINST -> ERROR`
  from the shell field-input layer; it is a poor driver for these screen-editor
  programs and is NOT the authoritative test.
- `run3.out.txt` / `run3.monlog.txt` - command-line-arg form (authoritative).
- `run4.out.txt` / `run4.monlog.txt` - the section-5 workaround experiment.

### What actually fired (run2, the longest natural run) - all SUCCESS or benign

Distinct MON exits observed:
`0B 1B 2B 4B 13B 16B 17B 41B 43B 50B 71B 72B 74B 76B 113B 143B 144B 256B 262B
313B 336B 503B 504B 511B`. No `UNIMPLEMENTED MON CALL` line anywhere.

### Decisive lines (run3, command-line form) - the abort

From `sandbox/run3.monlog.txt` (session scratch, not preserved):

```
[MON:INFO ] CALL 256B DEABF (3 args) at PC=0x08005F85
[MON:DEBUG] mon_read_descriptor_string: result len=21 str='DESCRIPTION-FILE:DESC'
[MON:DEBUG] Path translation: 'DESCRIPTION-FIL' + 'DES' -> '<sandbox>/GUEST/DESCRIPTION-FIL.DES'
[MON:DEBUG] MON 256B [DEABF/FullFileName]: 'DESCRIPTION-FILE:DESC' (host '<sandbox>/GUEST/DESCRIPTION-FIL.DES') not found -> NO SUCH FILE NAME
[MON:INFO ] EXIT 256B DEABF -> ERROR
[MON:INFO ] CALL 504B DVOUTS (3 args) at PC=0x080099F0
[MON:DEBUG] MON 504B [DVOUTS/OutputString]: Content: "..(56B = 46D): Sintran Error: "
[MON:INFO ] CALL 0B LEAVE (0 args) at PC=0x08025DB4
```

The program asks DEABF to resolve `DESCRIPTION-FILE:DESC` (the old-domain
description file). DEABF truncates the name to `DESCRIPTION-FIL` and the type to
`DES`, fails to find the (correctly named) file the sandbox contains, returns
error 56 (`056B` = 46 decimal = NO SUCH FILE NAME), and the program prints
`Sintran Error` and exits after ~6500 instructions.

### Output produced

NONE. No `LED-NEW:DOM`, no `:SEG` file was created (`ls GUEST | grep -i LED-NEW`
= empty).

--------------------------------------------------------------------------------
## 5. Reconcile static vs dynamic - which "missing" MON calls block conversion?

NONE of the nine unimplemented calls (104B/251B/254B/320B/334B/505B/512B/513B/
514B) is what blocks the LED conversion. The run aborts far earlier, inside MON
256B DEABF (which IS implemented and "VALIDATED"), because of the name/type
truncation described above.

### Confirming experiment (run4)

To prove the truncation is the sole near-term blocker, I placed copies of the
inputs under exactly the truncated names the emulator looks for
(`GUEST/DESCRIPTION-FIL.DES`, `LED-B03.PSE/.DSE/.LIN`) and reran the same
command-line invocation:

`sandbox/run4.monlog.txt` (session scratch, not preserved):
```
[MON:DEBUG] MON 256B [DEABF/FullFileName]: OUT: FullName='DESCRIPTION-FIL:DES;1'
[MON:INFO ] EXIT 256B DEABF -> SUCCESS
```
DEABF now succeeds, but it returns a TRUNCATED expanded name
`DESCRIPTION-FIL:DES;1`. The program compares that against what it asked for,
its own runtime consistency check fails, and it aborts with its internal
assertion (console `run4.out.txt`):
```
EASSERT VIOLATION AT 01000022731[ENTF] Using fixed data area at B=0x08024120 ...
```
Still ZERO unimplemented-MON traps. This confirms: the truncation bug both
(a) hides the file on lookup and (b) corrupts the returned full name -> the
conversion cannot proceed regardless of how the inputs are named.

--------------------------------------------------------------------------------
## 6. Prioritized fix list

1. FIX (blocks everything): widen the SINTRAN name/type buffers in
   `/home/ronny/repos/nd500x/external/ndmonlib/include/ndmon/mon_path.h`:
   - `SINTRAN_MAX_NAME 16` -> at least `17` (SINTRAN III allows 16-char names;
     buffer must hold name + NUL).
   - `SINTRAN_MAX_TYPE 4`  -> at least `5` (4-char types DESC/PSEG/DSEG/LINK +
     NUL).
   Then re-audit every consumer of `mon_parse_sintran_name` (grep hits:
   `mon_path.c`, `mon_file_table.c`, `mon_221B_CreateFile.c`,
   `mon_256B_FullFileName.c`) so the widened names flow through path translation
   and DEABF returns the full untruncated `(DIR:USER)NAME:TYPE;VERSION`.
   Verification: rerun the run3 command against a sandbox whose GUEST holds the
   normally-named `DESCRIPTION-FILE.DESC` / `LED-B03.PSEG/.DSEG/.LINK`; DEABF
   must return `DESCRIPTION-FILE:DESC;1` and the program must proceed past it.

2. MON calls to implement, in the order the conversion is likely to demand them
   once fix (1) lands (all currently trap; re-trace after each to find the next):
   - 412B FileAsSegment (FSCNT) is already VALIDATED - good, the converter maps
     the PSEG/DSEG as segments through it.
   - Likely next on the real convert path: **254B GetErrorDevice**,
     **334B GetErrorMessage** (error reporting), then **104B SuspendProgram**.
   - **320B UELogin**, **505B GetTrapReason**, **514B ND500TimeOut** - referenced
     but probably only on RT/timeout/login paths a plain conversion avoids.
   - **512B XMSGCallA / 513B XMSGCallB** (XMSG IPC) - 513B has 14 static refs but
     lives in the runtime/SHELL XMSG wrapper; only reached on message/SIBAS
     paths, not a local `:PSEG`->`:DOM` conversion. Lowest priority.
   - **251B CopyPage** - low priority; likely a bulk-copy fast path with a
     fall-back.
   Because none of these was reached before the truncation abort, their true
   priority can only be fixed by re-tracing AFTER fix (1); this ordering is
   INFERRED from the manual roles, not from a trace.

--------------------------------------------------------------------------------
## 7. Reproduction summary (paths)

- DOM: `/mnt/d/ND/500/CONVERT-DOMAIN/convert-dom-a03.dom`
- Extracted PROG for static scan:
  `work/prog.bin` (session scratch, not preserved)
- Sandbox root:
  `sandbox` (session scratch, not preserved)
- Run logs: `<sandbox>/run{1,2,3,4}.out.txt` and `<sandbox>/run{1,2,3,4}.monlog.txt`
- Handlers: `/home/ronny/repos/nd500x/external/ndmonlib/src/handlers/`
- Registry: `/home/ronny/repos/nd500x/external/ndmonlib/src/core/mon_registry.c`
- Dispatch trap: `/home/ronny/repos/nd500x/external/ndmonlib/src/core/mon_dispatch.c:173`
- Name-length limits (the bug): `/home/ronny/repos/nd500x/external/ndmonlib/include/ndmon/mon_path.h:19-21`

--------------------------------------------------------------------------------
## 8. UPDATE 2026-07-28: fix (1) LANDED - truncation gone; next blocker = EASSERT via ENTF

### Where the truncation actually lived (corrects section 1's attribution)

The `mon_path.h` macros themselves were NOT the live bug on the current
`ndix-mon600` branch: every parse buffer is declared `[SINTRAN_MAX_* + 1]`
(17/17/5 bytes), which already holds full 16-char names and 4-char types (the
DEABF-side `+1` fix landed in ndmonlib commit `b6fe237`). The remaining real
truncation was in the FILE-TABLE OBJECT-ENTRY storage in
`/home/ronny/repos/nd500x/external/ndmonlib/src/support/mon_file_table.c`:

- `mon_populate_object_entry_from_host()` capped the name at 15 chars + 0x27
  and the type at 3 chars + 0x27.
- `object_entry_init_terminal()` / `object_entry_init_file()` did
  `strncpy(...,15)` + forced NUL at `[15]`, and type `strncpy(...,3)` + NUL at
  `[3]`.

The `ObjectEntry` struct fields (`object_name[16]` at offset 2, `type[4]` at
offset 18, `/home/ronny/repos/nd500x/external/ndmonlib/include/ndmon/mon_file_table.h:78-79`)
are the GUEST-VISIBLE 64-byte SINTRAN object-entry layout and must NOT be
widened. Per that layout a full-length name fills all 16 bytes with NO
terminator; the 0x27 terminator appears only when the name is shorter.

### The fix (in ndmonlib, branch ndix-mon600)

- `mon_populate_object_entry_from_host()`: name up to 16 chars, type up to 4,
  0x27 terminator only when shorter than the field.
- `object_entry_init_terminal()` / `object_entry_init_file()`: same pattern.
- `write_sintran_string()`: bounded scan stopping at NUL or 0x27 instead of
  `strlen()` (the source may now legitimately be a full, unterminated field).
- `mon_41B_ReadObjectEntry.c` log line: `%.16s`/`%.4s` instead of `%s`.
- Audited consumers: `mon_257B_OpenFileInfo.c` `name_eq()` and the debugger
  `files` display (`/home/ronny/repos/nd500x/src/debugger/commands.c:4793`)
  were already bounded and 0x27-aware; no other C-string consumers found.

### Verified result (fresh sandbox run, same recipe as section 4)

```
[MON:DEBUG] MON 256B [DEABF/FullFileName]: IN: AbbrevName='DESCRIPTION-FILE:DESC', FileType='(none)'
[MON:DEBUG] Path translation: 'DESCRIPTION-FILE' + 'DESC' -> '<sandbox>/GUEST/DESCRIPTION-FILE.DESC'
[MON:DEBUG] MON 256B [DEABF/FullFileName]: OUT: FullName='DESCRIPTION-FILE:DESC;1'
[MON:INFO ] EXIT 256B DEABF -> SUCCESS
```

The section-4 abort (error 56, no output) is GONE.

### Regression checks (all green)

- Full C pipeline re-verified after the change: `MODE COMPILE-HELLO` produces a
  fresh 933-byte `HELLO.NRF` (mtime-verified), zero traps; `@HELLO` prints
  "Hello world".
- `stack_overflow` test passes.
- `mon_calls` test fails IDENTICALLY with the change stashed and restored
  (A/B-verified): its failure is the pre-existing "MON 413B FSCDNT ... could
  not open scratch file", unrelated to this fix.

### NEW blocker: EASSERT VIOLATION right after DEABF succeeds

With the name resolved, CONVERT-DOM-A03 immediately prints
`EASSERT VIOLATION AT 01000022731` and LEAVEs after only 2,835 instructions.
Octal `01000022731` = 0x080025D9, which is exactly the `ret=` address in the
emulator's own workaround line printed at that moment:

```
[ENTF] Using fixed data area at B=0x08024120, args=0, ret=0x080025D9
```

So the assert is tied to the emulator's ENTF "fixed data area" fallback path
(an instruction-handling gap, NOT a MON/file issue). Next step: investigate the
ENTF implementation (grep `Using fixed data area` in
`/home/ronny/repos/nd500x/src/cpu/`) against the ENTF spec in
`/home/ronny/repos/nd500x/docs/instructions/asm/` before implementing any of
the section-6 MON calls - none of them was reached.

Repro sandbox for this update (session scratchpad, regenerate as needed):
`cdsandbox/` (session scratch, not preserved)
(`run.out.txt`, `run.monlog.txt`; staged from `/mnt/d/ND/500/LED/x/` and
`/home/ronny/ND500USERS/SYSTEM`).

--------------------------------------------------------------------------------
## 9. UPDATE 2026-07-28 (later): CONVERSION WORKS - LED-B03 :PSEG/:DSEG -> runnable LED-NEW:DOM

`CONVERT-DOM-A03 LED-NEW LED-B03 NO YES` now runs to `>> Finished <<` and
produces a real 625,949-byte `LED-NEW.DOM` (header + 223,695-byte PSEG +
394,525-byte DSEG), which LOADS AND RUNS in the shell (executes 1.25M
instructions into LED's screen setup). Four fixes, in the order the blockers
appeared; each was verified by rerunning the conversion:

1. **DEABF must return the FULLY QUALIFIED name** `(DIR:USER)NAME:TYPE;VERSION`
   even for an unqualified input
   (`/home/ronny/repos/nd500x/external/ndmonlib/src/handlers/mon_256B_FullFileName.c`).
   The section-8 EASSERT was CONVERT-DOM-A03 scanning DEABF's reply for the
   `(DIR:USER)` prefix (check at 0x080025B6..D1, assert call at 0x080025D3) -
   proven by dumping the scanned string at assert time: it was exactly DEABF's
   unqualified reply. Directory name used: `PACK-ONE` (no directory level
   exists in the host mapping); the USER is the one the lookup resolved to.
2. **Path parser: `(DIR:USER)` split + `;VERSION` strip**
   (`/home/ronny/repos/nd500x/external/ndmonlib/src/support/mon_path.c`).
   Both forms are real SINTRAN syntax that the qualified names produce; before
   this, OPEN of a DEABF-returned name translated to a bogus host path ending
   `.DOM;`.
3. **CPU: `BI WCONV/HCONV/BYCONV/FCONV/DCONV` read their source as a BYTE and
   took bit 0, ignoring the decoded bit position**
   (5 files in `/home/ronny/repos/nd500x/src/cpu/instructions/FLOAT_MATH/`).
   CONVERT-DOM-A03's "is segment present" check (subroutine 0x08002CE0) tests
   domain-entry segment bitmaps via `bi wconv IND(...)(rN)`; with the bitmap
   byte 0x02 and bit position 1 the buggy read returned 0 for EVERY segment ->
   "IKonvDom: No segments on source domain!". Fixed to read ND500_DTYPE_BIT.
   All 16k+ instruction-validation tests still pass.
4. **`nd500_segment_release()` now clears the domain DATA CAPABILITY and the
   PST entry, and resolves the 0xFF current-domain sentinel**
   (`/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c`; callback
   signature gained the cpu pointer in
   `external/ndmonlib/include/ndmon/mon_types.h` + the three call sites).
   CONVERT-DOM-A03 closes and re-connects `LED-B03:LINK` to the SAME logical
   segment mid-conversion; the stale capability made the second FSCNT fail
   371B -> Sintran error 174B. (The unresolved 0xFF also meant release never
   matched the registry slot at all - every disconnect leaked its slot.)

Sandbox layout that converts successfully (per the description file, the
segment files must be staged under the FLOPPY-USER directory it names):

```
<root>/SYSTEM/CONVERT-DOM-A03.DOM (+ .INIT/.HELP), DDBTABLES-G06.VTM
<root>/GUEST/DESCRIPTION-FILE.DESC          (from /mnt/d/ND/500/LED/x/)
<root>/FLOPPY-USER/LED-B03.PSEG/.DSEG/.LINK (from /mnt/d/ND/500/LED/x/)
```

### Knock-on: LINKER-AUTO-FORT:JOB now actually runs (site config edited)

With DEABF's reply correctly qualified, the ND Linker's auto-job name check
passes (it previously failed by accident and the job was skipped - the old
`"B" does not match "LINKER-AUTO-FORT:JOB" exact` warning). The job then
demanded `(SYSTEM)FORTRAN-LIB`, which exists nowhere on the disks, and its
interactive prompt swallowed the MODE script's EXIT - the link ended with a
4,096-byte stub DOM. Per the job file's own header ("the file should, if
used, be edited to reflect the wanted environment"), the two live
`SPECIAL-LOAD (SYSTEM)FORTRAN-LIB/EXCEPT-LIB LIBRARY` lines in
`/home/ronny/ND500USERS/SYSTEM/LINKER-AUTO-FORT.JOB` were commented out
(0xA5 '%' prefix, parity encoding preserved); the untouched original is kept
as `LINKER-AUTO-FORT.JOB.ORIG-PRE-SITE-EDIT` alongside it.

### Regression status (all green)

- NC compile: fresh `HELLO.NRF`, now 947 bytes (was 933 - the BI-conversion
  CPU fix changes NC's codegen output; the result links and runs).
- Link: `HELLO.DOM` 6,313,424 bytes; `@HELLO` prints "Hello world".
- `test_instruction_validation --continue`: ALL TESTS PASSED (16k+).
- `stack_overflow` test green.

### Still open

`LED-NEW.DOM` runs into a CHAIN-instruction loop on a zero link at
PC=0x08035621 after ~1.25M instructions, after loading `DDBTABLES-G06.VTM`
staged as `DDBTABLES-E:VTM` (LED-B03 asks for revision D or E tables; only
G06 exists on the disks). Unknown whether this is a table-revision mismatch
or an emulator gap - next investigation.

---

## 10. UPDATE 2026-07-29: LED RUNS - CHAIN zero-link trap semantics + MON 52B TERMO

The open CHAIN-loop issue from section 9 is FIXED. It was never a DDBTABLES
revision problem: staging the real `DDBTABLES-D11/E11.VTM` (found in
`F:\RC\RonnyTest\HDLC3\vdm`, copied into the sandbox SYSTEM) changed nothing.
The real causes were two CPU trap bugs plus one missing MON call:

1. **CHAIN zero-link must raise IOV (bit 16, ignorable), not IOS (bit 34).**
   ND-05.009.4 section 15.7: zero link terminates the operation, Wn keeps the
   last element, K is set, and an *illegal operand value* trap condition is
   raised. LED walks a possibly-empty list with levels=0x7FFFFFFF and uses
   exactly this to find the end. File:
   `/home/ronny/repos/nd500x/src/cpu/instructions/CALL/Chain.c` (traversal
   prints now gated behind env `ND500X_CHAINDBG`).

2. **After-class traps resume at the NEXT instruction.** ND-05.009.4 page 79
   + Table 10: traps classed "A" complete the instruction; the trap frame's
   saved P (arg2, B+24 - what RETT returns to) points to the next
   instruction, while Trapping P (arg1, B+20) names the trapping one. We
   always wrote arg2 = trapping P, so LED's handler RETTed back into the same
   CHAIN forever ("TRAP. Trap no: 42B" storm, runaway-loop halt at
   PC=0x08035621). New `TRAP_AFTER_MASK` (0x80C1EFFA00) in
   `/home/ronny/repos/nd500x/src/cpu/cpu_protos.h`; `trap_resume_PC` set in
   `invoke_trap_handler` (`/home/ronny/repos/nd500x/src/cpu/cpu.c`) and
   written as arg2 by `/home/ronny/repos/nd500x/src/cpu/instructions/CALL/Entt.c`.

3. **MON 52B TERMO implemented** (was a dying stub): stores mode bits per
   device, returns success. Files:
   `/home/ronny/repos/nd500x/external/ndmonlib/src/handlers/mon_52B_TerminalMode.c`,
   `/home/ronny/repos/nd500x/external/ndmonlib/src/support/mon_terminal_state.c` (+ header, registry, mon_log.h).

**Result:** `LED-NEW` (converted from LED-B03 :PSEG/:DSEG by CONVERT-DOM-A03)
now boots its full VT100 UI - "Main" title bar, box-drawing frame, "LED:"
prompt, "August 24, 1988." - 507,595 instructions, no traps, then exits on
stdin EOF (interactive editor, no input supplied). Regression: HELLO.DOM
still prints "Hello world" (16,579 instructions) and the 46k
instruction-validation results are byte-identical before/after the trap
change (the double-register failures visible there are pre-existing from the
concurrent Dn work, verified by stash-rebuild-compare).

C# handoff: item 8 in the shared file (see section 9 note for its location).
