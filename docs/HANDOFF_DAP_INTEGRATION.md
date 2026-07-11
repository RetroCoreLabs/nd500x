# Handoff: Add libdap (Debug Adapter Protocol) support to nd500x

**Full path of this document:** `/home/ronny/repos/nd500x/docs/HANDOFF_DAP_INTEGRATION.md`

You are picking up the task of giving the **nd500x** ND-500 emulator a working
DAP server, so it can be driven from VS Code / any DAP client for source-level
debugging, exactly as the sibling **nd100x** emulator already can.

Constraints (non-negotiable, from the repo's CLAUDE.md):

- C11 only for the emulator core. No C++. No Python/JS in the core.
- **Never use Unicode** in code comments or strings (1980s ND toolchain).
- Never mention AI assistants in commit messages or documents.
- Always give full pathnames when you describe files you create.
- Do not assume - check against the reference (nd100x) and the libdap headers.
- Never replace files from git without asking.

---

## 1. What already exists in nd500x (and what is wrong with it)

There IS a stub, but it is **broken and mostly non-functional**. Treat it as a
sketch, not a foundation.

| Item | Full path | State |
|---|---|---|
| DAP adapter stub | `/home/ronny/repos/nd500x/src/debugger/dap_adapter.c` | 216 lines, **does not compile as C** |
| REPL launch hook | `/home/ronny/repos/nd500x/src/debugger/debugger.c:261-273` | `dap [port]` command; calls `nd500_dap_start` |
| Vendored library | `/home/ronny/repos/nd500x/external/libdap/` | present (CMake subdir, `dap_objects` target) |
| Top-level CMake wiring | `/home/ronny/repos/nd500x/CMakeLists.txt:66-71` | defines `HAVE_LIBDAP`, adds subdir, links cJSON |
| Debugger CMake wiring | `/home/ronny/repos/nd500x/src/debugger/CMakeLists.txt:12-16` | links `dap_objects` when `HAVE_LIBDAP` |
| Build variants | `/home/ronny/repos/nd500x/Makefile` | `make with-dap`, `without-dap`, `dap-sanitizer` |

**Concrete defects in the current stub you must fix:**

1. **It uses C++ lambdas in a `.c` file.** Lines ~163-215 register callbacks as
   `(DAPCommandCallback)[](DAPServer* s){ ... }`. That is C++, not C11 - it will
   not compile once `HAVE_LIBDAP` is actually on. Every one of these must become
   a named `static int cmd_xxx(DAPServer* server)` function. This is almost
   certainly why DAP has never actually been exercised: it only "builds" because
   the file is dead code under the current default build.
2. **Breakpoints are accepted but never connected to the CPU.** The
   `DAP_CMD_SET_BREAKPOINTS` handler calls `dap_server_add_breakpoint` (libdap's
   own list) but never calls `bp_add()` on the nd500 `BreakpointManager`, so the
   CPU never actually stops. Same for data breakpoints/watchpoints (not handled
   at all).
3. **No stop-event reporting.** When the CPU hits a breakpoint/watchpoint there
   is no path that sends a DAP `stopped` event back to the client.
4. **scopes / variables / stackTrace / setVariable / evaluate are no-op stubs**
   returning 0. These are the whole point of a source debugger.
5. `read_memory` returns a raw hex string into `base64_data`. Confirm against
   nd100x whether libdap expects real base64 or hex here (nd100x is the truth).

---

## 2. The reference implementation to mirror: nd100x

nd100x has a **mature, working** DAP integration. Read it first; copy its shape.

| Item | Full path |
|---|---|
| The entire DAP adapter (all callbacks live here) | `/home/ronny/repos/nd100x/src/debugger/debugger.c` |
| CMake DAP wiring (`DAP_ENABLED`, link `dap_objects`+`cjson_objects`) | `/home/ronny/repos/nd100x/src/debugger/CMakeLists.txt:80-115` |
| Frontend launch / port flag | `/home/ronny/repos/nd100x/src/frontend/nd100x/nd100x.c:213,485` |
| CPU stop-on-breakpoint hook (`CPU_BREAKPOINT` state) | `/home/ronny/repos/nd100x/src/cpu/cpu.c:429` |
| Integration guide (design notes) | `/home/ronny/repos/nd100x/docs/DAP_INTEGRATION_GUIDE.md` |
| Implementation summary | `/home/ronny/repos/nd100x/docs/DAP_IMPLEMENTATION_SUMMARY.md` |
| Recent update notes | `/home/ronny/repos/nd100x/docs/DAP_DEBUGGER_UPDATE_2026-07.md` |
| Known-bug writeups (learn from these) | `/home/ronny/repos/nd100x/BUG-dap-continue.md`, `/home/ronny/repos/nd100x/dap-analysis.md` |

**Key facts learned from nd100x that shape the port:**

- It registers **44** command callbacks, implemented as ~30 **named**
  `static int cmd_xxx(DAPServer* server)` functions in `debugger.c`. Grep for
  `dap_server_register_command_callback` there to see the full list.
- The functions you MUST implement fully (not stub) to get a usable debugger:
  `cmd_continue`, `cmd_next`, `cmd_step_in`, `cmd_step_out`, `cmd_pause`,
  `cmd_set_breakpoints`, `cmd_set_instruction_breakpoints`,
  `cmd_set_data_breakpoints`, `cmd_data_breakpoint_info`, `cmd_stack_trace`,
  `cmd_scopes`, `cmd_variables`, `cmd_read_memory`, `cmd_write_memory`,
  `cmd_disassemble`, `cmd_launch_callback`, `cmd_configuration_done`,
  `cmd_disconnect`, `cmd_evaluate`.
- It defines `DAP_ENABLED=1` (target compile definition). nd500x currently uses
  `HAVE_LIBDAP` (global) + `WITH_DEBUGGER` (file guard). Pick one convention and
  make it consistent; recommend matching nd100x's `DAP_ENABLED` on the debugger
  target and dropping the C++-only stub guard confusion.
- The server runs on a background thread over TCP; the CPU run-loop polls a
  stop flag. There is also a WASM in-process variant (`debugger.c:323`) - ignore
  for nd500x native, but keep the seam clean.

---

## 3. The nd500x machine/CPU API you will bind DAP to

**Do not reinvent breakpoints or watchpoints.** nd500x already has a full
engine; the CLI debugger already drives it (`bp`, `bp cond`, `wp`). DAP must call
the same functions. Reusing this is the single most important design decision -
it is how you avoid duplicate code (a hard repo rule).

Breakpoint / watchpoint engine - `/home/ronny/repos/nd500x/src/machine/breakpoints.h`:

```
void bp_mgr_init(BreakpointManager* mgr);
int  bp_add(BreakpointManager* mgr, uint32_t address, bool one_shot);
int  bp_add_conditional(BreakpointManager* mgr, uint32_t address, const char* condition, bool one_shot);
int  bp_delete/bp_enable/bp_disable(...);
bool bp_should_break_at(BreakpointManager* mgr, uint32_t pc);

int  wp_add(BreakpointManager* mgr, uint32_t address, uint32_t length, WatchpointType type);
int  wp_add_register(BreakpointManager* mgr, const char* reg_name, uint32_t reg_index);
bool wp_should_break_on_read(BreakpointManager* mgr, uint32_t addr);
bool wp_should_break_on_write(BreakpointManager* mgr, uint32_t addr, uint32_t value);
bool wp_should_break_on_register_change(BreakpointManager* mgr, uint32_t reg_index, uint32_t value);
/* WatchpointType: WP_TYPE_READ=1, WP_TYPE_WRITE=2, WP_TYPE_CHANGE=3, WP_TYPE_REGISTER=4 */
```

The manager lives on the machine as `m->bp_mgr` (see `commands.c:2104`,
`cmd_wp`/`cmd_bp` for exactly how the CLI calls these - copy that usage).

Execution / inspection API - `/home/ronny/repos/nd500x/src/machine/machine_protos.h`:

```
void   nd500_dbg_run (Nd500Machine* m);              /* run until stop; machine.c:28 */
void   nd500_dbg_step(Nd500Machine* m, uint32_t count);
size_t nd500_dbg_mem_dump(Nd500Machine* m, uint32_t addr, uint32_t len, uint8_t* out, size_t out_cap);
void   nd500_dbg_regs(struct Nd500Cpu* cpu, Nd500Regs* out_regs);
```

Registers, disassembly, symbols:
- Register set + aliases: `/home/ronny/repos/nd500x/src/cpu/cpu.c` (PC, I1-I4,
  A1-A4, E1-E4, L, B, R, TOS, FLAGS/ST1/ST2). The DAP `variables`/`scopes`
  response should expose these; see `docs/DEBUGGER_COMMAND_REFERENCE.md` and
  `cmd_regs` in `commands.c` for the canonical list and formatting.
- Disassembler: `/home/ronny/repos/nd500x/src/disasm/nd500_disasm.h` (the stub
  already uses it in `cmd_disassemble_cb`).
- Symbols: `ndlib_symbols_name_for_addr()` in `src/ndlib/ndlib_symbols.c`.
- Unified debug API: `/home/ronny/repos/nd500x/src/machine/debug_api.c`.

**Integration point for stop events:** `nd500_dbg_run()` in
`/home/ronny/repos/nd500x/src/machine/machine.c:28` is the run loop. It must, on
each step, consult `bp_should_break_at` / `wp_should_break_on_*` and, when a DAP
session is attached, break out and cause a DAP `stopped` event (reason:
`breakpoint` / `data breakpoint` / `step` / `pause`). Mirror how nd100x's run
loop signals `CPU_BREAKPOINT` (`nd100x/src/cpu/cpu.c:429`).

---

## 4. libdap public API (already vendored)

Headers: `/home/ronny/repos/nd500x/external/libdap/libdap/include/`
(`dap_server.h`, `dap_server_cmds.h`, `dap_types.h`, `dap_protocol.h`, ...).

Essentials from `dap_server.h`:
- `typedef int (*DAPCommandCallback)(struct DAPServer *server);` - every callback
  has THIS signature (no lambdas).
- `DAPServer *dap_server_create(const DAPServerConfig*)`, `dap_server_init`,
  `dap_server_start`, `dap_server_cleanup`, `dap_server_free`.
- `dap_server_register_command_callback(server, DAP_CMD_xxx, cb)`.
- Per-command arguments arrive in `server->current_command.context.<cmd>` (e.g.
  `context.read_memory`, `context.breakpoint`, `context.launch`). Results are
  written back into that same context struct and/or `server->debugger_state`.
- Output to the client's console: `dap_server_send_output_category(server,
  DAP_OUTPUT_*, msg)` (see nd100x `debugger.c:3527`).
- The mock server under `/home/ronny/repos/nd500x/external/libdap/src/dap_mock_server`
  is a minimal working example of the callback contract.

---

## 5. Recommended phased plan

**Phase 0 - make it actually compile and connect.**
1. Rewrite `src/debugger/dap_adapter.c`: replace every C++ lambda with a named
   `static int cmd_xxx(DAPServer*)`. Keep the existing continue/step/read/write/
   disasm/launch logic but as real functions.
2. Reconcile the build guard: settle on `DAP_ENABLED` (per nd100x) for the
   debugger target; ensure `make with-dap` compiles the adapter and links
   `dap_objects`+`cjson_objects`. Verify `make without-dap` still builds.
3. Confirm `dap <port>` in the REPL starts a server a VS Code client can attach
   to (handshake: initialize -> launch -> configurationDone).

**Phase 1 - execution control + breakpoints (the minimum useful debugger).**
4. Wire `cmd_set_breakpoints` / `cmd_set_instruction_breakpoints` to
   `bp_add`/`bp_delete` on `m->bp_mgr` (clear-and-reapply per request, like the
   CLI). Return verified breakpoint objects.
5. Make `nd500_dbg_run` honor the bp_mgr when a DAP session is attached and emit
   a `stopped` event on hit. Implement `cmd_continue/next/step_in/step_out/pause`
   against it.
6. Implement `cmd_stack_trace` (at minimum a single frame: PC + symbol), and
   `cmd_scopes`+`cmd_variables` to expose the ND-500 register file.

**Phase 2 - memory + data watchpoints (this unblocks a real investigation, see 6).**
7. Implement `cmd_data_breakpoint_info` + `cmd_set_data_breakpoints` against
   `wp_add(..., WP_TYPE_WRITE)` etc., with `stopped` reason `data breakpoint`.
8. Verify `cmd_read_memory`/`cmd_write_memory` encoding matches what libdap/VS
   Code expect (base64 vs hex - check nd100x `cmd_read_memory` at
   `debugger.c:3984`, which is the known-good version).

**Phase 3 - polish.**
9. `cmd_disassemble` (stub already does most of it), `cmd_source`,
   `cmd_evaluate`, `cmd_set_variable`, `cmd_symbol_list`. Frontend `--dap [port]`
   CLI flag on `nd500x` (mirror `nd100x.c`) so it can be launched headless.

---

## 6. Why this is wanted right now - the motivating use case

There is an active bug hunt that DAP would accelerate: the ND-500 C compiler
(`/mnt/d/ND/500/FraTor/nc/nc-a06.dom`) crashes mid-code-generation at
**PC 0x08023EA4** after ~1.5M instructions, dereferencing a corrupted pointer.
Both the C emulator (nd500x) and the C# emulator crash at the identical PC. The
faulting instruction is a linked-list walk:

```
08023E9C: w1 :=  b.0x14          ; load chain head
08023E9E: w1 :=  r1.(0x2)        ; R := R->next  (offset +2 words)
08023EA1: by test r1.(0x0)       ; dereference R  <-- faults on wild pointer
```

The bug is a **corrupted `next` link written into a heap node** (same family as
a GETB heap bug already fixed). Finding it needs a **write watchpoint on the
node's +8 field** to catch the instruction that wrote the bad link. That is
exactly a DAP *data breakpoint* -> so **prioritize Phase 2** (data breakpoints +
memory read + stop events). Note: the CLI debugger can already do this via
`wp <addr> 4 write`; DAP is the nicer instrument but not the only one, so do not
block the bug hunt on DAP - the value here is repeatable, IDE-driven tracing.

---

## 7. Build & test

```
cd /home/ronny/repos/nd500x
make with-dap                 # must compile the adapter for real now
make without-dap              # must still build
cd build && ctest             # all existing tests must stay green
```

Manual smoke test: start `./build/bin/nd500x --dom /mnt/d/ND/500/FraTor/nc/nc-a06.dom --debug`,
type `dap 4711`, attach a DAP client to `localhost:4711`, set a breakpoint,
continue, confirm a `stopped` event and register/memory inspection.

Do NOT commit until `with-dap`, `without-dap`, and `ctest` are all green, and
confirm the commit message does not mention any AI assistant.

## 8. Open questions to resolve by reading, not guessing

- Exact base64-vs-hex contract for read/write memory (answer: whatever nd100x
  `cmd_read_memory`/`cmd_write_memory` do).
- Whether nd500x's `nd500_dbg_run` currently checks the bp_mgr at all, or only
  the CLI single-step path does - determines how much run-loop surgery Phase 1
  needs. Read `src/machine/machine.c` around line 28.
- ND-500 has domains (CED/CAD) and an MMU; a DAP address may be virtual or
  physical. Decide how `read_memory`/breakpoint addresses map (the CLI has
  `m`, `mp`, `m!` for data/program/physical - see `docs/DEBUGGER_COMMAND_REFERENCE.md`).
