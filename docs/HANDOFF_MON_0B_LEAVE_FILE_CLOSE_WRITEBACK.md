# HANDOFF: MON 0B LEAVE (and 43B CLOSE -1/-2) never triggered segment write-back

Date: 2026-07-25
Follow-up to `/home/ronny/repos/nd500x/docs/HANDOFF_412B_SEGMENT_WRITEBACK.md` (2026-07-20).
That handoff implemented `nd500_segment_writeback()` and wired it into two call
sites: `413B FSCDNT` (explicit disconnect) and `43B CLOSE` on a single named
file number. This handoff adds the two call sites that were still missing.

Files changed:
- `/home/ronny/repos/nd500x/external/ndmonlib/include/ndmon/mon_file_table.h`
- `/home/ronny/repos/nd500x/external/ndmonlib/src/support/mon_file_table.c`
- `/home/ronny/repos/nd500x/external/ndmonlib/src/handlers/mon_0B_ExitFromProgram.c`
- `/home/ronny/repos/nd500x/external/ndmonlib/src/handlers/mon_43B_CloseFile.c`

## Symptom

Linking a trivial C program (`HELLO.C`: `int x; main() { x = 1; }`) via the
verified recipe in memory `compile-link-goal` / `docs/HANDOFF_412B_SEGMENT_WRITEBACK.md`,
using `diag_linkdrive` against `LINKER-B01.DOM`, ending the command script with
`CLOSE;;EXIT` (not an explicit `43B CLOSE` on the domain file number), produced
a `HELLO.DOM` that was still an empty 4096-byte stub - the exact same symptom
the 2026-07-20 handoff describes as already fixed.

## Root cause

`mon_0B_ExitFromProgram.c`'s own file header comment already stated the
SINTRAN-documented behavior:

> Background programs close all files not set permanently open.

but the code only requested a CPU halt - it never actually closed anything.
Since segment write-back only happens *inside* `43B CLOSE`'s single-file path,
a domain file that is still open (mapped as a segment, never explicitly
closed by name) when the program hits `MON 0B LEAVE` had its in-memory
segment content silently discarded on halt.

Separately, `43B CLOSE`'s own `FileNumber=-1`/`-2` ("close all") branches
called `mon_file_table_reset()` directly - bypassing the single-file
write-back logic a few lines below in the same file. So even an explicit
"close everything" from the guest skipped write-back too.

Confirmed via `ND500X_MONLOG=1`-traced `diag_linkdrive` runs against
`/home/ronny/ND500USERS/SYSTEM/LINKER-B01.DOM`: `HELLO.DOM` (file 65, host file
number 101) is opened once by `OPEN-DOMAIN` and connected as segment 3 via
`412B FSCNT`, but **no `43B CLOSE` call ever targets file 65/101** anywhere in
the run - the linker's own `CLOSE`/`EXIT` command sequence relies entirely on
the SINTRAN-documented close-on-exit behavior, which was unimplemented.

## Fix

Added `mon_file_table_close_all_for_exit(MonContext* ctx)` to
`mon_file_table.c`: walks every open file, and for each one still
`mapped_as_segment`, calls `ctx->writeback_file_segment` + `release_file_segment`
(the exact same calls `43B CLOSE`'s single-file path already made), then falls
through to the existing `mon_file_table_reset()`.

Wired into:
- `mon_0B_ExitFromProgram` (`MON 0B` / LEAVE) - unconditionally, for every
  program. RT-vs-background file retention isn't distinguished anywhere in
  this codebase yet, and every program this emulator currently runs (NC,
  CAT-500, the LINKER) is a background/batch program, so always
  closing-with-write-back is correct for the cases that matter today. If an
  RT-mode program is ever driven through here, this will need a real
  RT/background distinction instead of the unconditional call.
- `43B CLOSE`'s `FileNumber=-1`/`-2` bulk-close branches, replacing the bare
  `mon_file_table_reset()` calls.

## Verification

Sandbox `/home/ronny/ND500USERS` (see memory `nd500x-ini-reconstructed`), full
recipe (as ONE shell argument - see the footgun note below):
```
rm -f GUEST/HELLO.DOM
../repos/nd500x/build/bin/diag_linkdrive /home/ronny/ND500USERS/SYSTEM/LINKER-B01.DOM \
  'OPEN-DOMAIN "HELLO";;LOAD HELLO;;LOAD NC-LIB;;LOAD CAT-LIB;;DEFINE-ENTRY stack-space,400000,d;;DEFINE-ENTRY heap-space,400000,d;;REFER-ENTRY stack-space,rts_stack_size,d,d;;REFER-ENTRY heap-space,rts_heap_size,d,d;;CLOSE;;EXIT'
```

| | before this fix | after this fix |
|---|---|---|
| `HELLO.DOM` size | 4096 bytes (stub) | 6,311,936 bytes |
| non-zero bytes | 8 | 15,863 |
| MON log | no `43B CLOSE` on file 65 ever | `mon_file_table_close_all_for_exit: segment 3 written back to './GUEST/HELLO.DOM'` |
| content location | none | real bytes starting at file offset `0x402004`, matching the PROG-segment offset the 2026-07-20 handoff verified for a different program (`B`) |

## Still open - matches the 2026-07-20 handoff's own "Still open" section

The domain **header** (file offset 0x0-0xFF, including the start-address field
at `0xD8`) is still all-zero in `HELLO.DOM`, even though the file IS connected
as segment 3 at `LogSegmentNo=0` (i.e. segment 3 page 0 should map to file
offset 0). The write-back correctly leaves untouched pages as holes rather
than fabricating content, so this means segment 3's page 0 was simply never
written through the memory-mapped path during this run - the header write
(whatever sets the start-address field) either happens through a different
segment, or doesn't happen at all in this configuration.

This is the SAME class of gap the original handoff flagged as unresolved for
program `B` ("the linked program executes its entry stub and then calls
address 0 ... UNVERIFIED whether that is a remaining linker-side relocation
issue, a gap in what our write-back captures, or an nd500x DOM-loader
question"). Nobody has picked that back up since 2026-07-20. Recommend
investigating both together, starting from: what segment number(s) is the
domain's header/catalog actually connected as (may differ from the PROG/DATA
segment), and does `LOGSEG 0`/`412B FSCNT`'s `bytes=4096` initial connection
size correspond to a header region that's supposed to be patched by a LATER,
separate `412B` reconnection this trace didn't capture.

## Two footguns hit while reproducing this (not emulator bugs, but worth recording)

1. **Bash line-continuation inside single quotes is NOT stripped.** Writing
   the `diag_linkdrive` script argument across multiple lines with a trailing
   `\` for readability, inside single quotes, embeds a literal backslash +
   newline + leading whitespace into the actual command text sent to the
   guest. The linker's field editor faithfully read and acted on those
   garbage characters as real input between commands, corrupting the
   `DEFINE-ENTRY`/`REFER-ENTRY`/`CLOSE` sequence in ways that looked exactly
   like an emulator bug (extremely slow rounds, "Unrecognized parameter",
   `CLOSE` seemingly not registering) until the script was rewritten as one
   clean line. If a `diag_linkdrive` run behaves strangely from mid-script
   onward, check the script string for embedded backslash-newlines FIRST.
2. **`populate-system.sh`-staged libraries keep vendor version suffixes**
   (`NC-LIB-A06.NRF`, `CAT-LIB-B06.NRF`), but the LINKER's `LOAD NC-LIB` does
   an EXACT filename lookup (`256B DEABF` -> `./SYSTEM/NC-LIB.NRF`, no fuzzy
   matching). Plain-named copies were added to `/home/ronny/ND500USERS/SYSTEM/`
   (`NC-LIB.NRF`, `CAT-LIB.NRF`) alongside the versioned originals. This
   matches what memory `compile-link-goal`'s already-verified recipe did (it
   staged them as `GUEST/NC-LIB.NRF` for a different sandbox) - not a new
   discovery, just a fact `populate-system.sh` didn't carry over.

## Also fixed in this session (unrelated bug, found while debugging the above)

`external/ndmonlib/src/support/mon_file_table.c`'s queued-console output
buffer (`QUEUED_CONSOLE_MAX`) was 4096 bytes and silently DROPPED all output
past that with no warning - `queued_console_write_char()` just stopped
writing. A full-screen VT100 program's (the LINKER's) per-keystroke
cursor-position/redraw chatter blows past 4KB within the first few commands,
so any diagnostic reading `mon_get_console_output()` for a multi-command
session was blind past that point with no indication the log was incomplete.
This directly caused a long false trail while debugging the write-back issue
above (see the "Unrecognized parameter" investigation that turned out to
actually be the bash line-continuation footgun, not visible because the real
console content had already silently truncated). Fixed: bumped to 1MB and
added a one-time stderr warning if it ever fills again.

## Also found, NOT fixed (separate, cosmetic-looking but real bug)

Driving the `--monitor` SINTRAN shell (not `diag_linkdrive`) over piped stdin:
the character immediately after a nested domain program exits back to the `@`
prompt is reliably eaten - `EXIT` arrived at the shell as `XIT` ("NO SUCH
COMMAND OR DOMAIN"), and separately `LIN` arrived as `IN`, in two independent
runs. Not investigated further; flagging so it isn't mistaken for a typo next
time. Likely a framing issue in the handoff between a nested domain's raw
stdio console and the shell's own `readline`/`fgets` loop in
`src/frontend/nd500x/nd500x_shell.c`.
