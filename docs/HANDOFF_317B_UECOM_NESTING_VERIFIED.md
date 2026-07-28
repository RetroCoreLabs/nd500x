# HANDOFF: MON 317B UECOM nesting verified end-to-end (NC compile -> real .NRF)

Date: 2026-07-27
Builds on: `/home/ronny/repos/nd500x/docs/CAT500_UECOM_CSHARP_HANDOFF.md` (2026-07-14
design doc, Part A/B) and `/home/ronny/repos/nd500x/docs/HANDOFF_412B_SEGMENT_WRITEBACK.md`
(2026-07-20).

This is a VERIFICATION handoff, not a fix - the 317B UECOM nesting implementation
already existed as uncommitted WIP in `/home/ronny/repos/nd500x/src/frontend/nd500x/nd500x_shell.c`
(`shell_execute_command`, registered via `mon_set_execute_command`) plus the `MODE`
shell command (`cmd_mode`, SINTRAN `@MODE` script runner) and a segment-allocator
state save/restore pair (`nd500_segment_alloc_state_save`/`_restore` in
`/home/ronny/repos/nd500x/src/cpu/nd500_segment_alloc.c`) needed to isolate a nested
program's allocations from its caller. This session exercised it for the first time
against the real vendor NC/CAT-500 pipeline and confirmed it works.

## What was tested

Two pre-existing MODE scripts in the sandbox
(`/home/ronny/ND500USERS/GUEST/COMPILE-HELLO.MODE`,
`/home/ronny/ND500USERS/GUEST/LINK-HELLO.MODE`):

```
@CC compile HELLO:C  ->  HELLO:NRF        (MODE compile-hello)
CREATE-FILE HELLO:CAT
CREATE-FILE HELLO:LIST
CREATE-FILE HELLO:NRF
NC-A06
CHECK HELLO,HELLO,HELLO
GENERATE-CODE HELLO,HELLO
EXIT
```
```
@CC link HELLO:NRF  ->  HELLO:DOM         (MODE link-hello)
LINKER-B01
OPEN-DOMAIN "HELLO"
LOAD HELLO
SPECIAL-LOAD USLIB3,LIBRARY
SPECIAL-LOAD NC-LIB,LIBRARY
SPECIAL-LOAD CAT-LIB,LIBRARY
CLOSE
EXIT
```

Run via the shell's `--script` front door (login, then `MODE <name>`, one shell
process per MODE file):

```
printf 'LOGIN GUEST\nMODE COMPILE-HELLO\nEXIT\n' > /tmp/compile.script
./build/bin/nd500x --monitor --sintran-root /home/ronny/ND500USERS --user GUEST \
    --script /tmp/compile.script

printf 'LOGIN GUEST\nMODE LINK-HELLO\nEXIT\n' > /tmp/link.script
./build/bin/nd500x --monitor --sintran-root /home/ronny/ND500USERS --user GUEST \
    --script /tmp/link.script
```

(`GUEST/HELLO.NRF` and `GUEST/HELLO.CAT`/`HELLO.LIST` must be empty placeholder
files before the compile run - SINTRAN's unquoted-name OPEN is lookup-only and
never creates a missing file; `CREATE-FILE` in the MODE script itself handles this.)

## Result

`ND500X_MONLOG=1`-traced compile run (`MODE COMPILE-HELLO`), key lines:

```
-- NC-A06 placed (domain 1, start 0x08000004) --
...
MON 317B [UECOM/ExecuteCommand]: IN: Command='NC-A'          <- self-recurse (CHECK pass)
UECOM: nested 'NC-A' ran 112460 instrs (domain 4)
MON 317B [UECOM/ExecuteCommand]: nested 'NC-A' completed
MON 317B [UECOM/ExecuteCommand]: IN: Command='CAT-CAT5-B'    <- the real code generator
UECOM: nested 'CAT-CAT5-B' ran 843431 instrs (domain 3)
MON 317B [UECOM/ExecuteCommand]: nested 'CAT-CAT5-B' completed
MON 317B [UECOM/ExecuteCommand]: IN: Command='NC-A'          <- self-recurse again
UECOM: nested 'NC-A' ran 106678 instrs (domain 3)
UECOM: nested 'NC-A' ran 187912 instrs (domain 2)
-- program exited (1253922 instructions) --
```

| | before (UECOM stubbed) | after (this WIP) |
|---|---|---|
| `GUEST/HELLO.NRF` | 0 bytes (per `docs/CAT500_UECOM_CSHARP_HANDOFF.md` Part A) | 933 bytes, real CAT-500 codegen output |
| `317B` calls | logged and stubbed (no-op success) | nested domain actually loaded and run to its own `MON 0B LEAVE` |

`MODE LINK-HELLO` then linked the real `HELLO.NRF` into a proper
`GUEST/HELLO.DOM` (6,313,424 bytes, populated header - see
`HANDOFF_412B_SEGMENT_WRITEBACK.md` for what a populated header looks like).
Loaded and stepped in the debugger:

```
Entry:      0x08000004
[STOP] MON halt: Program exit (MON 0B LEAVE)
```

Clean exit, no traps. This closes exactly the gap
`docs/CAT500_UECOM_CSHARP_HANDOFF.md` Part A described ("BOUT.NRF was always 0
bytes ... because it was a MISSING BACK-END"): the NC front end's internal
`GENERATE-CODE` step now genuinely invokes CAT-500 as a nested SINTRAN
sub-process, sharing the file table with the caller, exactly per that doc's
Part B design.

## What this does NOT yet prove

- Only tested with `HELLO.C` (the trivial `printf("Hello world\n")` program).
  Not yet re-tried against `B.C` (`x = 42;`, no I/O) or anything larger/with
  more complex control flow through NC's own CHECK pass.
- The nesting depth cap is `UECOM_MAX_NEST = 4`
  (`/home/ronny/repos/nd500x/src/frontend/nd500x/nd500x_shell.c`). Not stress-tested
  near that limit.
- A plain `--script` feed (no `MODE`) does NOT exercise this path correctly: NC
  needs interactive follow-up input (`CHECK ...`, `GENERATE-CODE ...`) after its
  initial launch command, and plain `--script` does not keep feeding a
  still-running domain the way `MODE` does. Driving NC via `--script` alone
  with just an initial `NC COMPILE B,B,B`-style argument line hits
  `console EOF on device 0 - suspend (batch end)` after ~45k instructions and
  exits without ever reaching UECOM. Use a `MODE` file for anything beyond a
  single-shot program launch.

## Session context (unrelated build hazard found and fixed while testing)

`/home/ronny/repos/nd500x/test/diag_linkdrive.c` is built via a manual `gcc`
command documented in its own header comment, NOT a CMake target - `make`
silently never relinks it. Earlier in this session a stale
`build/bin/diag_linkdrive` (last linked 2026-07-25 23:26) produced a false
"domain header is all-zero, general regression" scare that fully evaporated
once the tool was relinked with the current libraries. If `diag_linkdrive`
behaves inconsistently with fresh code changes, relink it first:

```
gcc -O2 -o build/bin/diag_linkdrive test/diag_linkdrive.c \
  -Iexternal/ndmonlib/include \
  -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
  build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
  build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
```
