# DAP against a domain started from the SINTRAN shell (`--monitor`)

Date: 2026-08-11
Branch: `fix/deabf-i1-success-and-load-investigation`

Paths in this document are relative to the repository root.

## Symptom as reported

`nd500x --dap <port>` worked for the direct-load path (`--aout` / `--pseg` / `--dom`),
but against a domain typed at the SINTRAN `@` prompt (`--monitor`) every DAP query
came back empty: PC and all registers `0x00000000`, every memory read in the
domain's real range (`0xB0000xxx`) "unreadable" with any of the `phys:` / `ispace:`
/ `dspace:` prefixes, and instruction breakpoints at addresses known to be hit
never fired.

## What was actually wrong

Two defects, in this order. Both were found by reading the code and then proven by
running the emulator - neither is inferred.

### 1. With `--monitor --dap`, the shell never started

`src/frontend/nd500x/nd500x.c` starts the DAP server, and then, because
`--monitor` does not set `debug`, took the headless branch

```c
if (!debug) {
    while (nd500_dap_is_active()) { nanosleep(...); }   /* never returns */
    ...
}
```

The `monitor_mode` branch that calls `nd500x_shell_run()` is *after* that loop, so
it was unreachable. Proven: the same command line without `--dap` prints the
banner and the `@` prompt immediately; with `--dap` it printed nothing at all and
had to be killed. There was therefore no `@` prompt, no `LOGIN`, no domain, and no
CPU that had ever run - which is exactly what the DAP client was reporting.

The report that opened this investigation attributed the symptoms to a data race.
That was wrong for the reported repro: the shell thread did not exist. The race
below is real, but it is a hazard that only becomes reachable once defect 1 is
fixed.

### 2. Nothing serialized the shell's CPU stepping against the DAP thread

A domain started from the `@` prompt is stepped by `run_domain()` in
`src/frontend/nd500x/nd500x_shell.c`, which calls `nd500_cpu_step()` directly on
the shell's own thread. It never goes through `nd500_dbg_run()`, whose background
`run_thread` is what the DAP adapter was written around. Both touch the same
`Nd500Cpu` (`&cpu` in `main`, published to `machine->cpu` at `src/cpu/cpu.c:65`,
and held as `g_cpu` by the shell). A DAP register or memory read taken while the
shell thread is mid-instruction is a data race, and the value means nothing.

### 3. (Found during verification) a breakpoint killed the domain

`cpu.c` clears `machine->run_flag` when a breakpoint fires. `run_domain`'s loop
condition is `while (g_machine->run_flag)`, so it read that as "the program is
finished", printed `-- program exited --` and tore the domain down. The DAP client
got its `stopped` event, but there was nothing left to continue.

## The fix

### CPU lock - `src/machine/machine.c`, prototypes in `src/machine/machine_protos.h`

- `nd500_cpu_lock()` / `nd500_cpu_unlock()` - recursive mutex. Held by whoever is
  advancing the CPU, taken by the DAP adapter around every read.
- `nd500_cpu_lock_suspend()` / `nd500_cpu_lock_resume(n)` - drop **every** level
  this thread holds and put them back. Needed because the shell nests: MON 317B
  UECOM runs `shell_execute_command`'s step loop from inside `run_domain`'s, so
  the loop that wants to release the CPU is not always the one that took it.
  Releasing one level there would leave the outer level held, and a DAP read would
  wait for as long as the guest waits for a line - forever, at a prompt.
- `nd500_cpu_lock_yield()` - suspend, `sched_yield()`, resume. What a run loop
  calls periodically so a waiting reader gets a turn. Dropping the lock alone is
  not enough: the same thread normally retakes it before the waiter is scheduled.
- All five are no-ops under `__EMSCRIPTEN__` (one thread, nothing to serialize).

`run_thread` now holds the lock and hands it over every 4K instructions, and
suspends it across its periodic `nanosleep`.

### External driver flag - `src/machine/machine.c`

`nd500_cpu_set_external_driver(int)` / `nd500_cpu_has_external_driver(void)`.
While set, `nd500_dbg_run()` raises `run_flag` and returns **without** spawning
`run_thread`, so a DAP continue resumes the loop that already owns the CPU.
Without this, continue would have started a second thread stepping the same
`Nd500Cpu` next to the shell's loop - every instruction executed twice.

### Shell - `src/frontend/nd500x/nd500x_shell.c`

- Both run loops (`run_domain`, `shell_execute_command`) hold the CPU lock while
  stepping, `nd500_cpu_lock_yield()` every 4K instructions, and
  `nd500_cpu_lock_suspend()`/`resume` around `shell_wait_input_resume()`, which
  can block indefinitely at a guest prompt.
- `run_domain` also holds the lock across `ndlib_dom_load_to_machine()` and across
  the MMU/segment teardown, since both rewrite registers and page tables.
- `run_domain` calls `nd500_cpu_set_external_driver(1)` before its loop and `(0)`
  after it.
- `shell_park_for_debugger()` + `shell_is_debugger_stop()`: on `STOP_BREAKPOINT`
  or any `STOP_WATCHPOINT_*`, release every lock level and poll for `run_flag` to
  come back up instead of ending the run. ESCAPE still aborts out of the park.
  With no DAP client attached it returns 0 and the old end-the-run behavior
  stands, so a stray breakpoint cannot hang the `@` prompt with no way out.

### Frontend - `src/frontend/nd500x/nd500x.c`

The headless wait loop is now guarded by `if (!debug && !monitor_mode)`, so
`--monitor --dap` starts the DAP server and then falls through to the shell.

### DAP adapter - `src/debugger/dap_adapter.c`

`DAP_CMD_LOCKED(name) { ... }` defines the body as `name_unlocked` plus a `name`
that takes the CPU lock around it, so no body has to remember to unlock on each
return path and the registration table is unchanged. Applied to the 20 callbacks
that touch CPU registers, MMU state or machine memory: `cmd_check_cpu_events`,
`cmd_launch`, `cmd_attach`, `cmd_restart`, `cmd_continue`, `cmd_step_common`,
`cmd_pause`, `cmd_set_breakpoints`, `cmd_set_instruction_breakpoints`,
`cmd_data_breakpoint_info`, `cmd_set_data_breakpoints`, `cmd_stack_trace`,
`cmd_scopes`, `cmd_variables`, `cmd_set_variable`, `cmd_evaluate`,
`cmd_read_memory`, `cmd_write_memory`, `cmd_disassemble`, `cmd_symbol_list`.

## Verification (live, not inferred)

`LINKAGE-LOAD-H02` started from the `@` prompt under user `FLOPPY-USER`, DAP
client attached over TCP:

| Query | Before | After |
|---|---|---|
| shell startup | no `@` prompt, process hung | `-- LINKAGE-LOAD-H02 placed (domain 1, start 0xB0000DD1) --` |
| PC | `0x00000000` | `0xB001CAF3` (parked at the guest's `Nll:` prompt) |
| `ispace:0xB0000DD1`, 32 bytes | unreadable | `DC B0 00 26 E4 CF 00 00 00 1C CE 20 00 C3 B0 00 ...`, 0 unreadable |
| breakpoint `0xB0000649` | never fired | `stopped` / `breakpoint`, PC `0xB0000649`, domain still alive |
| step (instruction) | n/a | `Stepped to PC=0xB0000659` |
| continue | n/a | domain ran on, executed its `HELP` command, printed `Command:`, parked again |

The plain (dspace) read of `0xB0000DD1` returns zeros while the `ispace:` read
returns instruction bytes. That is correct separate-I/D behavior, not a failure.

`make` clean; `ctest -R "dap|mon"` - `dap_adapter`, `mon_handlers`, `mon_calls`
all pass.

## Repro that was used

```
./build/bin/nd500x --monitor --user FLOPPY-USER --sintran-root <sintran-root> --dap 4717
```
fed from a file kept open with `tail -f` (a closed stdin ends the guest's input
wait and the domain exits):
```
LOGIN FLOPPY-USER
LINKAGE-LOAD-H02
```

## Not carried to the C# side

Nothing here is CPU, MMU, trap or MON semantics - it is host threading and
command-line ordering in this emulator's frontend and DAP adapter. No
`docs/SYNC-BACKLOG.md` line and no item in the shared RetroCore rolling file.

## Known limits

- `nd500_dbg_step` called from DAP while a shell run loop is parked steps the CPU
  from the DAP thread. It holds the CPU lock, so it is safe, but the "who drives
  the CPU" split is still two mechanisms rather than one.
- The park polls `run_flag` every 10 ms rather than waiting on a condition
  variable. Fine for a debugger stop; it would not be for anything hot.
- Only the `run_domain` path registers itself as the external driver. A domain
  entered some other way (a future shell command that steps the CPU itself) would
  have to do the same or a DAP continue would spawn a second stepping thread.
