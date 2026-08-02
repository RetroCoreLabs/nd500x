# nd500x DAP command test matrix

**Full path of this document:** `docs/DAP_COMMAND_TEST_MATRIX.md`

Every DAP command supported by the nd500x adapter, its usage (following the
nd100x conventions), and the live test result. All live tests were run against
the NC compiler DOM:

```
cd build/nc_sandbox
../bin/nd500x --dom $ND500_TESTDATA/FraTor/nc/nc-a06.dom --dap 4500
```

driven by the dap-debugger MCP client, on 2026-07-11. Unit-level coverage is
in `test/test_dap_adapter.c` (ctest name
`dap_adapter`, 103 checks).

## Session control

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| initialize | debug_connect | connect to host:port | PASS - full capabilities returned (data breakpoints, read/write memory, disassemble, instruction breakpoints, stepping granularity, setVariable, evaluate, restart, terminate) |
| attach | debug_attach | attach to already-loaded machine (DOM workflow) | PASS - attached; CPU stays stopped until continue |
| launch | debug_launch | program = a.out path; stopOnEntry true/false | PASS (a.out load path); launching a .dom returns an error response - DOMs must be loaded with `--dom` + attach (details are sent as a stderr output event; response text is generic "Command failed") |
| configurationDone | (sent by client during attach/launch) | - | PASS |
| disconnect | debug_disconnect | terminate true/false | PASS - server recycles its transport; a new client can reconnect to the same session |
| restart | (no MCP tool; VS Code sends it) | resets PC to the captured entry point | Implemented; covered by adapter code + capabilities. Not drivable from the MCP client |
| terminate | (via debug_disconnect terminate=true) | stops the CPU, sends terminated event | PASS |
| threads | debug_threads | - | PASS - one thread ("CPU thread", id 1) |

## Execution control

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| continue | debug_continue | resume; also first start after attach | PASS - runs at full speed; stopped event delivered on breakpoint/watchpoint/exception. Negative: continue before attach returns "Debugger not running or attached" |
| next | debug_step_over | granularity statement/line/instruction (all step one instruction on nd500x) | PASS - stopped reason "step"; stepping off a breakpoint the CPU is parked on executes the instruction (resume-skip) instead of re-breaking |
| stepIn | debug_step_in | same as next | PASS |
| stepOut | debug_step_out | same as next (no frame unwinding yet) | PASS |
| pause | debug_pause | while running | PASS - stopped reason "pause". Negative: pause while stopped returns "Debugger already paused" |

## Breakpoints

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| setBreakpoints (source) | debug_set_breakpoints | file+lines; resolves via loaded symbol/source maps | PASS - with no source map loaded returns verified:false, message "No address known for this source line" (a.out workflow with .map/.s loads maps) |
| setInstructionBreakpoints | debug_set_instruction_breakpoints | hex addresses (virtual, PC-space); replace-all per request | PASS - verified:true, breakpoint hit at 0x08023E9C with stopped reason "breakpoint". CLI-created `bp` entries survive DAP replace-all |
| setFunctionBreakpoints | debug_set_function_breakpoints | function names | NOT IMPLEMENTED - clean error "Function breakpoints callback failed" (needs symbol tables; candidate for a.out workflow) |
| setExceptionBreakpoints | (client-driven) | filters all/uncaught | Accepted (no-op: nd500x always reports CPU traps as exception stops) |
| dataBreakpointInfo | (used internally by the MCP data-bp tools) | name = `0xADDR`, `phys:0xADDR`, `dspace:`/`ispace:` prefix, symbol name, or register name (PC, I1-I4, L, B, R) | PASS - returns dataId `V:0xADDR:len`, `P:0xADDR:len`, or `R:NAME` |
| setDataBreakpoints | debug_set_data_breakpoints | accessType read/write/readWrite; address_space virtual/physical; replace-all per request | PASS - virtual addresses translated through the MMU at set time; readWrite installs read+write entries; watchpoint fires with stopped reason "data breakpoint" |
| (register watch) | debug_watch_register | register name, e.g. I1; break on change | PASS (live) - continue stopped with reason "data breakpoint", description "watchpoint (register) at PC=0x08023EA9" right after the instruction that wrote I1 |

## Inspection

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| stackTrace | debug_stack_trace | single frame (PC + symbol + source if mapped) | PASS - frame with instruction_pointer; symbol/line appear when maps are loaded (DOM has none) |
| scopes | (via debug_variables) | 8 scopes mirroring the CLI `regs` sections | PASS - Core, Integer Registers, Float Registers, Addressing, Special, MMU / Domain, Trap Control, Status Flags |
| variables | debug_variables | per scope or all | PASS - full ND-500 register file: PC/FLAGS/ST1/ST2 + Flags ASCII (PDZSCKO, nested flag breakdown), I1-I4, A1-A4/E1-E4 + computed D1-D4 doubles, L/B/R, TOS/LL/HL/THA, CED/CAD/PS/PSTP/DITBASE, OTE/CTE/MTE/TEMM pairs |
| setVariable | (unit-tested; VS Code drives it) | write any register by name in any register scope | PASS (unit test) - writes the register; computed variables (Flags, D1-D4) and unknown names rejected |
| evaluate | debug_evaluate / debug_add_watch / debug_evaluate_watches / debug_remove_watch / debug_clear_watches | register name, symbol, numeric literal (0x/octal/decimal) | PASS - e.g. `R` -> "0x100002E4 (268436196)"; watch list evaluates in one call; garbage yields type "error" |

## Memory and code

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| readMemory | debug_read_memory | `0xADDR` (virtual, MMU-translated per 2KB page), `phys:`, `dspace:`, `ispace:` prefixes; base64 payload | PASS - all address spaces verified; out-of-range tail reported via unreadableBytes; debugger reads never trigger watchpoints |
| writeMemory | debug_write_memory | same prefixes; base64 data | PASS - physical write + readback roundtrip verified (DE AD BE EF); never triggers watchpoints |
| disassemble | debug_disassemble | address (virtual), count, optional symbol resolution | PASS - crash-site listing matches the reference (`w1 := b.20` / `w1 := r1.2` / `test r1.0` / `if=go`) at 0x08023E9C |
| source | (VS Code) | - | NOT IMPLEMENTED (returns error; source content comes from local files via source paths) |
| symbolList (custom) | debug_symbol_list | filter/paging client-side | PASS - returns loaded a.out symbols; empty for a bare DOM (no symbol table) |

## Console I/O (custom, nd100x convention)

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| consoleEnable | debug_console_enable | switches MON console to capture queue | PASS |
| consoleWrite | debug_console_write | text with `\r` `\n` `\t` `\\` escapes, or `hex:0D...` raw bytes | PASS - `COMPILE A,A,A\r` drove the NC compiler; output streamed back |
| (console output) | debug_console_read | reads output events (category stdout) | PASS - compiler banner, echo and progress streamed live |

## CPU tracing (custom, RetroCore)

| DAP command | MCP tool | Usage / variants | Live result |
|---|---|---|---|
| setCpuTracing | debug_set_cpu_tracing | enable + ring capacity + pc filter | NOT IMPLEMENTED - clean error "setCpuTracing not implemented by this server" (nd500x has file-based tracing via the CLI `show trace`; a ring buffer is future work) |
| getCpuTraceRing | debug_get_cpu_trace_ring | read back ring | NOT IMPLEMENTED - clean error |

## Lifecycle hooks (internal, not client-visible)

| Hook | Purpose | Status |
|---|---|---|
| DAP_WAIT_FOR_DEBUGGER | pre-command CPU access | no-op (memory is a plain byte array; register reads are atomic enough for inspection) |
| DAP_RELEASE_DEBUGGER | post-command release | no-op |
| DAP_CHECK_CPU_EVENTS | poll stop state, send stopped events, stream console output | PASS - all stopped events in the live tests were delivered through this path |

## Stopped-event reasons observed live

| Reason | Trigger |
|---|---|
| breakpoint | instruction breakpoint at 0x08023E9C |
| step | next/stepIn/stepOut |
| pause | debug_pause while running |
| data breakpoint | register watchpoint on I1 (memory watchpoints unit-tested) |
| exception | the NC compiler crash: protection violation at PC=0x08023EA4 (wild pointer 0xA1B8A1A8, instruction 1,492,947) |
| entry | launch with stopOnEntry |

## Known limitations

- stepOut / step granularity line/statement degrade to single-instruction
  steps (no frame-chain unwinding on the ND-500 yet); stackTrace reports one
  frame.
- setFunctionBreakpoints, source, setCpuTracing, getCpuTraceRing are not
  implemented (clean error responses).
- launch loads a.out files only; DOM files use the `--dom` + attach workflow.
- Instruction breakpoint conditions are accepted but not evaluated
  (bp_add_conditional exists in the engine; wiring is future work).
- The MCP client accumulates instruction breakpoints client-side; to clear
  them, disconnect/reconnect (server-side replace-all works, verified by
  unit test).
