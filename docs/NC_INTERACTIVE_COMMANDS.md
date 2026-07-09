# NC (Norsk Data C, A06) Interactive Command Interface - Probe Results

Date: 2026-07-09
Binary: /mnt/d/ND/500/FraTor/nc/nc-a06.dom (Version A06, 1989-01-10)
Driven under: nd500x emulator, via test_dom_integration --input and the
debugger `input` command (queued console -> MON 503B DVINST / 504B DVOUTS).

## 1. Interaction model (how NC talks to the terminal)

1. On start NC prints its banner via MON 504B DVOUTS to device 1:
   `Norsk Data C - Version: A06 - 1989-01-10`
2. It then reads via MON 503B DVINST, ONE character at a time
   (MaxNo=1), initially from DEVICE 0 (the SINTRAN command buffer /
   initial argument line).
3. There is NO `NC:` prompt until the first CR. When DVINST receives a
   CR (0x0D) on device 0, NC emits `0D 0A 4E 43 3A 20` = `\r\nNC: ` and
   SWITCHES its input to DEVICE 1 (the interactive terminal). From then
   on every command is read char-by-char from device 1.
4. Two ways to drive it:
   - One-liner via device 0 (the command buffer): queue
     `COMPILE name,name,name\r` BEFORE the banner - NC consumes it as the
     initial command line (this is what the ctest dom_nc_compile_* tests
     do).
   - Interactive: queue `\r` first (yields the `NC: ` prompt), then the
     command text, e.g. `\rcompile B,B,BOUT\r`.

Answer to "does it give an NC: prompt": only after you press Enter once.
The banner line is all you get until the first CR.

## 2. Full command reference (from the built-in `help`)

Typing `help` then Enter (empty sub-argument) dumps the whole table.
`<x: >` = a value NC will prompt for; `[...]` = optional; `...` = repeatable.

| Command | Arguments / prompts |
|---------|--------------------|
| `cc` | (no args; silent, returns to NC:) |
| `help` | `<command: >` - help on one command (blank = list all) |
| `exit` | (none) - leave NC |
| `preprocess` | `<source file: >,[<list file: >],[<output file: >]` |
| `check` | `<source file: >,[<list file: >],[<CAT file: >]` |
| `generate-code` | `<CAT file: >,<object file: >` |
| `compile` | `<source file: >,<list file: >,<object file: >` |
| `link` | `<source file: >,<program: >` |
| `cross` | `<source file: >,<cross reference file: >,<lines per page: >` |
| `format` | `<source file: >,<new source file: >` |
| `value` | `<definitions / options / libraries: >` |
| `define` | `[<macro identifier [(identifier,...)]: >],[<value: >]` |
| `undef` | `[<macro identifier: >]` |
| `directory` | `[<include directory/user: >]` |
| `options` | `<option: >...` (repeatable) |
| `page-length` | `[<lines: >]` |
| `library` | `<library file: >...` (repeatable) |
| `initialize-compile-parameters` | `[<initialization file: >]` |
| `save-compile-parameters` | `[<initialization file: >]` |
| `clear` | (none) - reset compile parameters |
| `@<SINTRAN-command>` | pass a command to SINTRAN |

## 3. Per-command probe outcomes

All runs used source GUEST/B.C (`#define VALUE 42 ... x = VALUE;`).

### File / phase commands

- **preprocess B,B,BPRE** -> writes `BPRE.PP` (type :PP), 36 bytes =
  macro-expanded source. Prints "no rewrite" / " terminated", then
  MON 0B LEAVE (whole program exits).
- **check B,B,BCHK** -> prints "preprocessing", writes `BCHK.CAT`
  (type :CAT), 30 bytes = preprocessed source only (NOT a parsed
  intermediate). Then "no rewrite" / " terminated" -> exit.
- **generate-code CAT,obj** -> never reached: any preceding phase command
  terminates the whole program, so a chained `generate-code` does not run.
  Standalone it would take a :CAT and emit `<object>:NRF`.
- **compile B,B,BOUT** -> opens `BOUT:NRF`, `B:LIST`, `B:C`; produces
  `BOUT.NRF` = 30 bytes preprocessed source. "no rewrite" / " terminated".
- **link B,BPROG** -> opens `SCRATCH-00001:MODE`, prints "preprocessing",
  then "no rewrite" / " terminated" -> exit. (This is the phase that would
  invoke the linkage step.)
- **cross B,BXREF** -> prompts `lines per page: ` then would emit an xref.
- **format B,BNEW** -> reformats source to `<new source>`; terminated in
  this probe (likely needs its second file answered).

### Parameter / state commands (return to the NC: prompt, no exit)

- **cc** - silent, returns to NC:. (Likely "compile current"/repeat.)
- **clear** - silent, resets compile parameters, returns to NC:.
- **options** - prompts `option: ` repeatedly (repeatable list).
- **define** - prompts `macro identifier : ` then `value : `.
- **undef** - prompts `macro identifier : `.
- **directory** - prompts `include directory/user : `.
- **library** - prompts `library file: `.
- **value** - prompts `definitions / options / libraries: `; an empty/bad
  value prints `illegal parameter`.
- **page-length** - takes `[<lines: >]` (optional).
- **initialize-compile-parameters** - tries to READ NC-A:INIT; with no such
  file prints `can't read from NC-A:INIT`.
- **save-compile-parameters** - WRITES the current parameters to
  `NC-A:INIT` (see section 5).
- **exit** - leaves NC.

## 4. The "no rewrite / terminated" blocker

Every code-producing phase (preprocess / check / compile / link)
terminates the WHOLE program (MON 0B LEAVE) right after the preprocessing
sub-phase, printing:

    <newline> no rewrite <newline> terminated <newline>

Findings:
- It is NOT a MON error. A full MON trace of `check` shows every call
  (OPEN 50B, RFILE 117B, WFILE 120B, SETBS 76B, SMAX 73B, RMAX 62B,
  CLOSE 43B) returning SUCCESS. NC reads the source and writes the CAT,
  then decides "no rewrite" in its own logic and quits.
- "no rewrite" is not a literal string in the .asm; it is assembled at
  run time (MON 32B MSG from a descriptor). The message is emitted from
  PC 0x0802D199 (distinct from the 0x0802D1CC newline-MSG site).
- On start NC always fails to open NC-A:INIT (error -46, no such file);
  pre-placing a valid NC-A.INIT makes that open SUCCEED but does NOT
  change the "no rewrite" outcome - so the missing init file is not the
  (sole) cause.

Full human-facing console message for `compile B,B,BOUT`:

    Norsk Data C - Version: A06 - 1989-01-10
    NC:
    preprocessing
    <blank>
    no rewrite
    <blank>
     terminated
       0:00:01

So "no rewrite" = the object file was never rewritten with real code;
compilation terminated after the preprocessing pass.

Hard findings from tracing the compile (1,113,964 instructions):
- ALL compiler code is loaded and exercised. Segment 1 PROG is 191,678
  bytes (0x08000000..~0x0802EC00); the highest PC executed is 0x0802E506,
  i.e. the top of the code. There is NO missing/never-entered code
  generator segment - the back end is present and reachable.
- Preprocessing does real work: 871k of the 1.11M instructions (87%) run
  in the 0x0800Dxxxx region (the lexer/preprocessor), and it produces the
  CORRECT macro-expanded output (VALUE -> 42). So the front end works.
- After preprocessing the driver (code region 0x0802Cxxx-0x0802Exxx)
  emits "no rewrite"/"terminated" via the print routine at 0x0802D171
  WITHOUT invoking the code generator. The status word tested at the
  report site (0x0801D7BC) is 0 - this is message-machinery state, not
  the upstream compile decision.
- NOT caused by: the missing NC-A:INIT (pre-placing a valid one changes
  nothing), the object option (o+ tested), or any MON error (every MON
  call in the trace returns SUCCESS).

Remaining hypotheses (unresolved):
(a) This nc-a06.dom / this invocation is intentionally preprocess-only
    and expects a separate driver or a different command path to reach
    codegen (e.g. a real two-step check -> generate-code with a proper
    :CAT intermediate that this front end does not itself produce).
(b) A subtle MON file-I/O semantics mismatch (the classic ND-500 BYTE vs
    ND-100 WORD count divergence in RFILE/WFILE, flagged in the MON plan)
    makes the driver's post-preprocess check take the terminate branch.
    Note the preprocessing round-trip does WFILE then RFILE on the
    intermediate; if the returned byte/word count is off, the driver may
    conclude it cannot proceed. The small-case data path is byte-correct,
    but the COUNT semantics were not independently verified here.

## 5. NC-A:INIT - the compile-parameter file

`save-compile-parameters` writes `NC-A:INIT` (host: GUEST/NC-A.INIT,
77 bytes). Its content is the default option string:

    options m2  a4  f-  r4  l+  d+  n+  s-  p-  i-  o-  pr- ic+ lm+ t-  a-  lo+

Each token is `<flag><+|-|digit>`. Decoding is not yet confirmed against
the NC manual, but note `o-` (candidate: object output OFF) and `l+`
(list ON).

TESTED: driving `options` -> `o+` -> (blank ends the list) -> `compile
B,B,BOUT` with correct prompt sync still yields the 30-byte preprocessed
output and "no rewrite". So `o+` is either not the object-enable flag or
the "no rewrite" decision is made upstream of the option flags. The
blocker is NOT the option string.

## 6. How to reproduce

Scratch harnesses (session scratchpad):
- nc_probe.sh '<input>' [steps]  - prints ordered console dialogue
- nc_run.sh   '<input>' [steps]  - console + file ops + resulting files

Queued input uses `\r` for CR. Always send a leading `\r` to reach the
`NC: ` prompt before an interactive command. Example:

    nc_run.sh '\rcompile B,B,BOUT\r'

## 7. Open questions / next steps

1. Resolve "no rewrite" - the option string is NOT the cause (o+ tested).
   The decisive next step is to set an execution breakpoint at the MSG
   call site PC 0x0802D199 (which emits "no rewrite") and single-step
   BACKWARD from there in a trace: find the branch/condition that routed
   the compile into the "no rewrite" path instead of the code generator.
2. The loaded nc-a06.dom may be a front-end-only image (preprocess/check
   only) with the code generator in a separate pass/segment. Verify
   against the DOM segment map, and test the standalone
   "generate-code <CAT>,<object>" path with a genuine :CAT intermediate
   (not the preprocessed-source CAT this front end writes).
3. Consult the NC / ND-500 C compiler manual for the option semantics
   (the m/a/f/r/l/d/n/s/p/i/o/pr/ic/lm/t/a/lo flags) and the intended
   compile driver - our all-in-one COMPILE may not be the path that
   invokes codegen.
4. Golden reference for the eventual binary NRF:
   /mnt/d/ND/500/FraTor/test-real/test-real.nrf (real ND-produced, 1991).
