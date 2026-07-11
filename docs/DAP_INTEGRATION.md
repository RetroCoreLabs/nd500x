# DAP (Debug Adapter Protocol) integration in nd500x

**Full path of this document:** `/home/ronny/repos/nd500x/docs/DAP_INTEGRATION.md`

nd500x has a working DAP server so the emulator can be driven from VS Code or
any DAP client (including the dap-debugger MCP server). The integration
mirrors the sibling nd100x emulator.

**Default port: 4500** (nd100x uses 4711; the ND-500 emulator uses 4500 so
both can run side by side).

## Starting the server

```
# Headless: load a DOM and serve DAP until the client disconnects
./build/bin/nd500x --dom program.dom --dap          # port 4500
./build/bin/nd500x --dom program.dom --dap 4600     # explicit port

# From the interactive REPL
./build/bin/nd500x --debug
nd500> dap          # port 4500
nd500> dap 4600     # explicit port
```

With `--dap` the CPU does not start running until the client sends
`continue` (or `launch` without `stopAtEntry`).

## Architecture

| Piece | Full path |
|---|---|
| DAP adapter (all command callbacks) | `/home/ronny/repos/nd500x/src/debugger/dap_adapter.c` |
| Protocol library (vendored submodule) | `/home/ronny/repos/nd500x/external/libdap/` |
| Breakpoint/watchpoint engine (shared with CLI) | `/home/ronny/repos/nd500x/src/machine/breakpoints.c` |
| Register-name table (shared with CLI `set`) | `/home/ronny/repos/nd500x/src/machine/debug_api.c` |
| Unit tests | `/home/ronny/repos/nd500x/test/test_dap_adapter.c` |

- libdap owns the TCP transport, JSON parsing and response serialization.
  The adapter only implements `static int cmd_xxx(DAPServer*)` callbacks that
  read parsed arguments from `server->current_command.context.<cmd>` and
  write results back into the same struct.
- The DAP server runs on its own pthread pumping `dap_server_run()` every
  10 ms. Callbacks never execute the CPU; continue/step ask the machine
  run-thread to run (`nd500_dbg_run`) or perform one synchronous step.
- Stop reporting: the CPU sets `m->run_flag = 0` and `m->stop_reason` when a
  breakpoint, watchpoint or trap hits (`src/cpu/cpu.c`, `src/machine/io.c`).
  The `DAP_CHECK_CPU_EVENTS` hook polls this every server loop iteration and
  sends the DAP `stopped` event (reasons: `breakpoint`, `data breakpoint`,
  `step`, `pause`, `exception`).
- Breakpoints and watchpoints go into the same `BreakpointManager`
  (`m->bp_mgr`) that the CLI `bp`/`wp` commands use. The adapter tracks which
  entries it created so DAP replace-all semantics never remove CLI-created
  breakpoints.
- Debugger memory access uses `nd500_dbg_mem_read_raw`/`write_raw`, which
  bypass the bus layer so DAP reads and writes never trigger watchpoints.
- When no DAP server has been started there is no additional per-instruction
  cost: no thread exists and the only hot-path checks (breakpoint manager
  consults) predate the DAP work.

## Address spaces

Memory references accept the nd100x prefixes:

- `phys:` / `P:` - raw physical address
- `dspace:` / `D:` (and no prefix) - virtual address, translated through the
  data page tables when the MMU is enabled
- `ispace:` / `I:` - virtual address, translated through the program page
  tables

Translation happens page by page (2 KB ND-500 pages) via
`nd500_mmu_translate`. Translations from the DAP thread never leave a CPU
trap pending (any trap raised by a debugger-initiated translation is
cleared immediately).

## Data breakpoints (watchpoints)

`dataBreakpointInfo` resolves a symbol or address expression and returns a
`dataId` of the form:

```
V:0xADDRESS:LENGTH    virtual address (translated via MMU at set time)
P:0xADDRESS:LENGTH    physical address (used as-is)
```

`setDataBreakpoints` parses the dataId and calls `wp_add()` with
`WP_TYPE_READ` and/or `WP_TYPE_WRITE` (accessType `readWrite` creates both).
The watchpoint engine observes physical bus addresses, so virtual watch
addresses are translated when the watchpoint is set.

## Registers as variables

Eight scopes mirror the CLI `regs` command's sections (the canonical ND-500
register report):

- **Core** (1001): PC, FLAGS, ST1, ST2, plus the Flags ASCII summary
  (PDZSCKO, uppercase=set) with the individual flags nested under it
- **Integer Registers** (1002): I1-I4 (W/H/BY/BI aliases in the description)
- **Float Registers** (1003): A1-A4, E1-E4, plus computed read-only D1-D4
  (64-bit E:A converted to IEEE754)
- **Addressing** (1004): L, B, R
- **Special** (1005): TOS, LL, HL, THA
- **MMU / Domain** (1200): CED, CAD, PS, PSTP, DITBASE
- **Trap Control** (1006): OTE1/2, CTE1/2, MTE1/2, TEMM1/2
- **Status Flags** (1100): P, D, Z, S, C, K, O decoded from ST1

Register watchpoints (break on change) are available for PC, I1-I4, L, B, R
via `dataBreakpointInfo`/`setDataBreakpoints` with a register name (dataId
`R:<NAME>`) - the same engine the CLI `wp reg <name>` uses, now checked in
the CPU step loop.

`setVariable` writes registers by name; `evaluate` accepts a register name,
a symbol, or a numeric literal. The name-to-storage mapping lives in one
table in `/home/ronny/repos/nd500x/src/machine/debug_api.c`
(`nd500_dbg_reg_set_by_name` etc.), shared with the CLI `set` command.

## Console I/O (custom commands, nd100x convention)

- `consoleEnable` switches the SINTRAN MON console to the captured queue
  implementation; program output is then streamed to the client as DAP
  `output` events (category `stdout`).
- `consoleWrite` queues keyboard input. Backslash escapes `\r`, `\n`, `\t`,
  `\\` are translated; `hex:` input mode sends raw bytes.

## Building and testing

```
make                    # DAP built automatically when external/libdap exists
make without-dap        # SKIP_LIBDAP=ON, adapter compiled out
cd build && ctest       # includes the dap_adapter unit test
./build/bin/test_dap_adapter    # run the adapter unit tests directly
```

The unit tests (`test/test_dap_adapter.c`) drive the registered callbacks
through a transport-less `DAPServer` via `nd500_dap_bind()` and verify
machine-side effects (BreakpointManager contents, memory, registers).

End-to-end testing was done with the dap-debugger MCP client against
`nc-a06.dom`: instruction breakpoints, stopped events, stepping, pause,
register/memory inspection, virtual/physical reads, data breakpoints,
register watchpoints and console I/O, including catching the known NC
compiler crash at PC=0x08023EA4 (protection violation, wild pointer in a
heap linked list) as a live `stopped` event with reason `exception`.

The complete per-command test matrix (usage variants and results) is in
`/home/ronny/repos/nd500x/docs/DAP_COMMAND_TEST_MATRIX.md`. Behaviour bug
reports from client-side sessions go to
`/home/ronny/repos/nd500x/docs/DAP_BUG_REPORTS.md`.
