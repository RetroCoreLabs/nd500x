/*
 * Unit tests for the nd500x DAP adapter (src/debugger/dap_adapter.c).
 *
 * Drives the registered command callbacks directly through a
 * transport-less DAPServer instance: fills current_command.context
 * the way libdap's protocol handlers would, invokes the callback,
 * and checks both the context results and the machine-side effects
 * (BreakpointManager entries, memory contents, registers).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../src/machine/machine_protos.h"
#include "../src/machine/breakpoints.h"
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/debugger/debugger.h"
#include "../external/libdap/libdap/include/dap_server.h"
#include "../external/libdap/libdap/include/dap_server_cmds.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond, msg) do { \
	g_tests_run++; \
	if (!(cond)) { \
		g_tests_failed++; \
		printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, msg); \
	} \
} while (0)

static Nd500Machine g_m;
static Nd500Cpu g_cpu;
static DAPServer* g_srv;

static int call_cb(DAPCommandType cmd) {
	DAPCommandCallback cb = g_srv->command_callbacks[cmd];
	if (!cb) return -1000;
	g_srv->current_command.type = cmd;
	return cb(g_srv);
}

/* ── Instruction breakpoints ───────────────────────────────────── */

static void test_instruction_breakpoints(void) {
	InstructionBreakpointCommandContext* ctx =
		&g_srv->current_command.context.instruction_breakpoint;

	/* Pre-existing CLI breakpoint must survive DAP replace-all */
	int cli_id = bp_add(g_m.bp_mgr, 0x00001111, false);
	CHECK(cli_id >= 0, "CLI breakpoint added");
	int base_count = g_m.bp_mgr->bp_count;

	uint32_t addrs[2] = { 0x08023E9C, 0x08001000 };
	int offsets[2] = { 0, 0 };
	DAPBreakpoint results[2];
	memset(results, 0, sizeof(results));
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 2;
	ctx->addresses = addrs;
	ctx->offsets = offsets;
	ctx->breakpoints = results;

	CHECK(call_cb(DAP_CMD_SET_INSTRUCTION_BREAKPOINTS) == 0, "set 2 instr bps");
	CHECK(results[0].verified && results[1].verified, "both verified");
	CHECK(g_m.bp_mgr->bp_count == base_count + 2, "two bps added to manager");
	CHECK(bp_should_break_at(g_m.bp_mgr, 0x08023E9C), "bp active at 0x08023E9C");

	/* Replace with one different breakpoint: old DAP bps removed,
	 * CLI bp untouched */
	uint32_t addrs2[1] = { 0x08002000 };
	int offsets2[1] = { 0 };
	DAPBreakpoint results2[1];
	memset(results2, 0, sizeof(results2));
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 1;
	ctx->addresses = addrs2;
	ctx->offsets = offsets2;
	ctx->breakpoints = results2;

	CHECK(call_cb(DAP_CMD_SET_INSTRUCTION_BREAKPOINTS) == 0, "replace instr bps");
	CHECK(g_m.bp_mgr->bp_count == base_count + 1, "replace-all removed old DAP bps");
	CHECK(!bp_should_break_at(g_m.bp_mgr, 0x08023E9C), "old DAP bp gone");
	CHECK(bp_should_break_at(g_m.bp_mgr, 0x08002000), "new DAP bp active");
	CHECK(bp_should_break_at(g_m.bp_mgr, 0x00001111), "CLI bp untouched");

	/* Empty request clears all DAP instruction breakpoints */
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 0;
	CHECK(call_cb(DAP_CMD_SET_INSTRUCTION_BREAKPOINTS) == 0, "clear instr bps");
	CHECK(g_m.bp_mgr->bp_count == base_count, "only CLI bp remains");

	free(results[0].message);
	free(results[1].message);
	free(results2[0].message);
	bp_delete(g_m.bp_mgr, 0); /* remove CLI bp again */
}

/* ── Data breakpoints (watchpoints) ────────────────────────────── */

static void test_data_breakpoint_info(void) {
	DataBreakpointInfoCommandContext* ctx =
		&g_srv->current_command.context.data_breakpoint_info;

	/* Plain hex address, default virtual space, default 4 bytes */
	memset(ctx, 0, sizeof(*ctx));
	ctx->name = "0x1000032C";
	ctx->as_address = true;
	CHECK(call_cb(DAP_CMD_DATA_BREAKPOINT_INFO) == 0, "info hex addr");
	CHECK(ctx->data_id != NULL, "dataId produced");
	CHECK(ctx->data_id && strcmp(ctx->data_id, "V:0x1000032C:4") == 0,
	      "dataId is V:0x1000032C:4");
	CHECK(ctx->supports_read && ctx->supports_write && ctx->supports_read_write,
	      "all access types supported");
	free((char*)ctx->data_id);
	free((char*)ctx->description);

	/* Physical prefix and explicit byte count */
	memset(ctx, 0, sizeof(*ctx));
	ctx->name = "phys:0x2000";
	ctx->as_address = true;
	ctx->bytes = 2;
	CHECK(call_cb(DAP_CMD_DATA_BREAKPOINT_INFO) == 0, "info phys addr");
	CHECK(ctx->data_id && strcmp(ctx->data_id, "P:0x00002000:2") == 0,
	      "dataId is P:0x00002000:2");
	free((char*)ctx->data_id);
	free((char*)ctx->description);

	/* Unresolvable name yields NULL dataId (not an error response) */
	memset(ctx, 0, sizeof(*ctx));
	ctx->name = "no_such_symbol_xyz";
	CHECK(call_cb(DAP_CMD_DATA_BREAKPOINT_INFO) == 0, "info bad symbol rc");
	CHECK(ctx->data_id == NULL, "bad symbol gives NULL dataId");
	free((char*)ctx->description);
}

static void test_set_data_breakpoints(void) {
	SetDataBreakpointsCommandContext* ctx =
		&g_srv->current_command.context.set_data_breakpoints;

	/* Pre-existing CLI watchpoint must survive DAP replace-all */
	int cli_wp = wp_add(g_m.bp_mgr, 0x3333, 4, WP_TYPE_READ);
	CHECK(cli_wp >= 0, "CLI watchpoint added");
	int base_count = g_m.bp_mgr->wp_count;

	/* One write watch + one readWrite watch (becomes two entries) */
	char* ids[2] = { "P:0x00002000:4", "P:0x00002100:2" };
	char* access[2] = { "write", "readWrite" };
	DAPBreakpoint results[2];
	memset(results, 0, sizeof(results));
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 2;
	ctx->data_ids = ids;
	ctx->access_types = access;
	ctx->breakpoints = results;

	CHECK(call_cb(DAP_CMD_SET_DATA_BREAKPOINTS) == 0, "set data bps");
	CHECK(results[0].verified && results[1].verified, "both verified");
	CHECK(g_m.bp_mgr->wp_count == base_count + 3,
	      "write=1 entry, readWrite=2 entries");
	CHECK(wp_should_break_on_write(g_m.bp_mgr, 0x2000, 0xAB), "write watch fires");
	CHECK(!wp_should_break_on_write(g_m.bp_mgr, 0x2004, 0xAB),
	      "write watch bounded by length");
	CHECK(wp_should_break_on_read(g_m.bp_mgr, 0x2101), "readWrite read fires");
	CHECK(wp_should_break_on_write(g_m.bp_mgr, 0x2100, 1), "readWrite write fires");

	/* Replace-all with empty set removes DAP watchpoints only */
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 0;
	CHECK(call_cb(DAP_CMD_SET_DATA_BREAKPOINTS) == 0, "clear data bps");
	CHECK(g_m.bp_mgr->wp_count == base_count, "only CLI wp remains");
	CHECK(wp_should_break_on_read(g_m.bp_mgr, 0x3333), "CLI wp untouched");

	free(results[0].message);
	free(results[1].message);
	wp_delete(g_m.bp_mgr, 0);
}

static void test_register_watchpoints(void) {
	DataBreakpointInfoCommandContext* ictx =
		&g_srv->current_command.context.data_breakpoint_info;
	SetDataBreakpointsCommandContext* ctx =
		&g_srv->current_command.context.set_data_breakpoints;

	/* dataBreakpointInfo on a register name yields R:<NAME> */
	memset(ictx, 0, sizeof(*ictx));
	ictx->name = "I1";
	CHECK(call_cb(DAP_CMD_DATA_BREAKPOINT_INFO) == 0, "info register");
	CHECK(ictx->data_id && strcmp(ictx->data_id, "R:I1") == 0, "dataId is R:I1");
	free((char*)ictx->data_id);
	free((char*)ictx->description);

	/* setDataBreakpoints installs a register watch primed with the
	 * current value */
	g_cpu.I[0] = 0x11111111;
	char* ids[1] = { "R:I1" };
	DAPBreakpoint results[1];
	memset(results, 0, sizeof(results));
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 1;
	ctx->data_ids = ids;
	ctx->breakpoints = results;
	CHECK(call_cb(DAP_CMD_SET_DATA_BREAKPOINTS) == 0, "set register watch");
	CHECK(results[0].verified, "register watch verified");
	CHECK(g_m.bp_mgr->wp_count == 1, "one watchpoint installed");
	CHECK(g_m.bp_mgr->watchpoints[0].type == WP_TYPE_REGISTER, "type REGISTER");
	CHECK(g_m.bp_mgr->watchpoints[0].last_value == 0x11111111,
	      "primed with current value");

	/* wp_check_registers: no change -> no hit; change -> hit */
	uint32_t regs[WP_REG_INDEX_COUNT] = {
		g_cpu.PC, g_cpu.I[0], g_cpu.I[1], g_cpu.I[2], g_cpu.I[3],
		g_cpu.L, g_cpu.B, g_cpu.R
	};
	CHECK(wp_check_registers(g_m.bp_mgr, regs) == -1, "unchanged: no hit");
	regs[1] = 0x22222222;
	CHECK(wp_check_registers(g_m.bp_mgr, regs) == 0, "changed: watch 0 hit");
	CHECK(wp_check_registers(g_m.bp_mgr, regs) == -1, "hit consumed (last_value updated)");

	/* Register watch fires through a real CPU step: watch PC, execute
	 * one instruction (PC always changes) */
	g_m.memory[0x1000] = 0x6C;
	g_m.memory[0x1001] = 0x00;
	g_cpu.PC = 0x1000;
	char* ids2[1] = { "R:PC" };
	DAPBreakpoint results2[1];
	memset(results2, 0, sizeof(results2));
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 1;
	ctx->data_ids = ids2;
	ctx->breakpoints = results2;
	CHECK(call_cb(DAP_CMD_SET_DATA_BREAKPOINTS) == 0, "set PC watch");
	g_m.stop_reason = STOP_NONE;
	bool stepped = nd500_cpu_step(&g_cpu);
	CHECK(!stepped, "step returned false (watch hit)");
	CHECK(g_m.stop_reason == STOP_WATCHPOINT_REGISTER, "stop reason register watch");

	/* Clear DAP watches */
	memset(ctx, 0, sizeof(*ctx));
	ctx->breakpoint_count = 0;
	CHECK(call_cb(DAP_CMD_SET_DATA_BREAKPOINTS) == 0, "clear register watches");
	CHECK(g_m.bp_mgr->wp_count == 0, "all register watches removed");
	g_m.stop_reason = STOP_NONE;

	free(results[0].message);
	free(results2[0].message);
}

/* ── Memory read/write (base64) ────────────────────────────────── */

static void test_memory_roundtrip(void) {
	WriteMemoryCommandContext* wctx = &g_srv->current_command.context.write_memory;
	ReadMemoryCommandContext* rctx = &g_srv->current_command.context.read_memory;

	/* "HELLO" == base64 "SEVMTE8=" */
	memset(wctx, 0, sizeof(*wctx));
	wctx->memory_reference = 0x4000;
	wctx->data = "SEVMTE8=";
	CHECK(call_cb(DAP_CMD_WRITE_MEMORY) == 0, "write memory");
	CHECK(wctx->bytes_written == 5, "5 bytes written");
	CHECK(memcmp(g_m.memory + 0x4000, "HELLO", 5) == 0, "bytes landed in memory");

	memset(rctx, 0, sizeof(*rctx));
	rctx->memory_reference = 0x4000;
	rctx->count = 5;
	CHECK(call_cb(DAP_CMD_READ_MEMORY) == 0, "read memory");
	CHECK(rctx->base64_data && strcmp(rctx->base64_data, "SEVMTE8=") == 0,
	      "read returns base64 of HELLO");
	CHECK(rctx->unreadable_bytes == 0, "all bytes readable");
	free(rctx->base64_data);

	/* Debugger memory access must not trigger watchpoints */
	int wp = wp_add(g_m.bp_mgr, 0x4000, 4, WP_TYPE_WRITE);
	CHECK(wp >= 0, "watchpoint on write set");
	g_m.run_flag = 1;
	g_m.stop_reason = STOP_NONE;
	memset(wctx, 0, sizeof(*wctx));
	wctx->memory_reference = 0x4000;
	wctx->data = "SEVMTE8=";
	CHECK(call_cb(DAP_CMD_WRITE_MEMORY) == 0, "write over watched memory");
	CHECK(g_m.stop_reason == STOP_NONE, "DAP write did not trip watchpoint");
	CHECK(g_m.run_flag == 1, "DAP write did not stop machine");
	g_m.run_flag = 0;
	wp_delete(g_m.bp_mgr, 0);

	/* Out-of-range read reports unreadable bytes */
	memset(rctx, 0, sizeof(*rctx));
	rctx->memory_reference = g_m.memory_size - 2;
	rctx->count = 8;
	CHECK(call_cb(DAP_CMD_READ_MEMORY) == 0, "read past end");
	CHECK(rctx->unreadable_bytes == 6, "6 bytes unreadable past end");
	free(rctx->base64_data);
}

/* ── Registers: scopes / variables / setVariable / evaluate ────── */

static void free_variable_results(VariablesCommandContext* ctx) {
	for (int i = 0; i < ctx->variable_count; i++) {
		free(ctx->variable_array[i].name);
		free(ctx->variable_array[i].value);
		free(ctx->variable_array[i].type);
	}
	free(ctx->variable_array);
	ctx->variable_array = NULL;
	ctx->variable_count = 0;
}

static int count_scope_vars(int ref) {
	VariablesCommandContext* vctx = &g_srv->current_command.context.variables;
	memset(vctx, 0, sizeof(*vctx));
	vctx->variables_reference = ref;
	if (call_cb(DAP_CMD_VARIABLES) != 0) return -1;
	int n = vctx->variable_count;
	free_variable_results(vctx);
	return n;
}

static void test_scopes_and_variables(void) {
	ScopesCommandContext* sctx = &g_srv->current_command.context.scopes;
	memset(sctx, 0, sizeof(*sctx));
	CHECK(call_cb(DAP_CMD_SCOPES) == 0, "scopes");
	CHECK(sctx->scope_count == 8, "eight scopes (mirrors CLI regs sections)");
	CHECK(sctx->scopes && strcmp(sctx->scopes[0].name, "Core") == 0,
	      "first scope is Core");
	int core_ref = sctx->scopes[0].variables_reference;
	int int_ref = sctx->scopes[1].variables_reference;
	int float_ref = sctx->scopes[2].variables_reference;
	int mmu_ref = 0, trap_ref = 0;
	for (int i = 0; i < sctx->scope_count; i++) {
		if (strcmp(sctx->scopes[i].name, "MMU / Domain") == 0)
			mmu_ref = sctx->scopes[i].variables_reference;
		if (strcmp(sctx->scopes[i].name, "Trap Control") == 0)
			trap_ref = sctx->scopes[i].variables_reference;
		free(sctx->scopes[i].name);
	}
	free(sctx->scopes);
	CHECK(mmu_ref != 0 && trap_ref != 0, "MMU and Trap Control scopes present");

	g_cpu.PC = 0x08023E9C;
	g_cpu.I[0] = 0xDEADBEEF;
	g_cpu.OTE1 = 0x0F01DA00;
	g_cpu.ST1 = ND500_FLAG_Z | ND500_FLAG_PIA;

	/* Core scope: PC/FLAGS/ST1/ST2 + Flags ASCII summary */
	VariablesCommandContext* vctx = &g_srv->current_command.context.variables;
	memset(vctx, 0, sizeof(*vctx));
	vctx->variables_reference = core_ref;
	CHECK(call_cb(DAP_CMD_VARIABLES) == 0, "variables (Core)");
	CHECK(vctx->variable_count == 5, "Core: 5 variables");
	int found_pc = 0, found_flags_ascii = 0;
	for (int i = 0; i < vctx->variable_count; i++) {
		if (strcmp(vctx->variable_array[i].name, "PC") == 0 &&
		    strcmp(vctx->variable_array[i].value, "0x08023E9C") == 0) found_pc = 1;
		if (strcmp(vctx->variable_array[i].name, "Flags") == 0 &&
		    strcmp(vctx->variable_array[i].value, "PdZscko") == 0) found_flags_ascii = 1;
	}
	CHECK(found_pc, "PC listed with correct value");
	CHECK(found_flags_ascii, "Flags ASCII string correct (PdZscko)");
	free_variable_results(vctx);

	/* Integer scope: I1-I4 */
	memset(vctx, 0, sizeof(*vctx));
	vctx->variables_reference = int_ref;
	CHECK(call_cb(DAP_CMD_VARIABLES) == 0, "variables (Integer)");
	CHECK(vctx->variable_count == 4, "Integer: 4 variables");
	CHECK(strcmp(vctx->variable_array[0].name, "I1") == 0 &&
	      strcmp(vctx->variable_array[0].value, "0xDEADBEEF") == 0,
	      "I1 listed with correct value");
	free_variable_results(vctx);

	/* Float scope: A1-4, E1-4 + computed D1-D4 */
	CHECK(count_scope_vars(float_ref) == 12, "Float: 12 variables (A,E,D)");
	/* MMU scope: CED, CAD, PS, PSTP, DITBASE */
	CHECK(count_scope_vars(mmu_ref) == 5, "MMU: 5 variables");
	/* Trap Control: OTE/CTE/MTE/TEMM pairs */
	memset(vctx, 0, sizeof(*vctx));
	vctx->variables_reference = trap_ref;
	CHECK(call_cb(DAP_CMD_VARIABLES) == 0, "variables (Trap Control)");
	CHECK(vctx->variable_count == 8, "Trap Control: 8 variables");
	CHECK(strcmp(vctx->variable_array[0].name, "OTE1") == 0 &&
	      strcmp(vctx->variable_array[0].value, "0x0F01DA00") == 0,
	      "OTE1 listed with correct value");
	free_variable_results(vctx);

	/* setVariable writes a register (Integer scope) */
	SetVariableCommandContext* svctx = &g_srv->current_command.context.set_variable;
	memset(svctx, 0, sizeof(*svctx));
	svctx->variables_reference = int_ref;
	svctx->name = "I2";
	svctx->value = "0x12345678";
	CHECK(call_cb(DAP_CMD_SET_VARIABLE) == 0, "setVariable I2");
	CHECK(g_cpu.I[1] == 0x12345678, "I2 register written");
	CHECK(svctx->new_value && strcmp(svctx->new_value, "0x12345678") == 0,
	      "new value echoed");
	free((char*)svctx->new_value);
	free((char*)svctx->type);

	/* setVariable rejects unknown register and computed variables */
	memset(svctx, 0, sizeof(*svctx));
	svctx->variables_reference = int_ref;
	svctx->name = "NOPE";
	svctx->value = "1";
	CHECK(call_cb(DAP_CMD_SET_VARIABLE) != 0, "unknown register rejected");
	memset(svctx, 0, sizeof(*svctx));
	svctx->variables_reference = float_ref;
	svctx->name = "D1";
	svctx->value = "1";
	CHECK(call_cb(DAP_CMD_SET_VARIABLE) != 0, "computed D1 rejected");
	g_cpu.ST1 = 0;
}

static void test_evaluate(void) {
	EvaluateCommandContext* ctx = &g_srv->current_command.context.evaluate;

	g_cpu.R = 0x100002E4;
	memset(ctx, 0, sizeof(*ctx));
	ctx->expression = "R";
	CHECK(call_cb(DAP_CMD_EVALUATE) == 0, "evaluate register");
	CHECK(ctx->result && strcmp(ctx->result, "0x100002E4 (268436196)") == 0,
	      "register value formatted");
	free((char*)ctx->result);
	free((char*)ctx->type);

	memset(ctx, 0, sizeof(*ctx));
	ctx->expression = "0x1234";
	CHECK(call_cb(DAP_CMD_EVALUATE) == 0, "evaluate literal");
	CHECK(ctx->result && strcmp(ctx->result, "0x00001234 (4660)") == 0,
	      "literal value formatted");
	free((char*)ctx->result);
	free((char*)ctx->type);

	memset(ctx, 0, sizeof(*ctx));
	ctx->expression = "not(a)symbol";
	CHECK(call_cb(DAP_CMD_EVALUATE) == 0, "evaluate garbage rc");
	CHECK(ctx->type && strcmp(ctx->type, "error") == 0, "garbage yields error type");
	free((char*)ctx->result);
	free((char*)ctx->type);
}

/* ── Execution control state ───────────────────────────────────── */

static void test_execution_control(void) {
	StepCommandContext* step = &g_srv->current_command.context.step;

	/* Step executes exactly one instruction: put a 2-byte no-crash
	 * instruction at PC. Use 0x6C 0x00 ("w1 + $0" from the test json). */
	g_m.run_flag = 0;
	g_m.stop_reason = STOP_NONE;
	g_cpu.PC = 0x1000;
	g_m.memory[0x1000] = 0x6C;
	g_m.memory[0x1001] = 0x00;
	memset(step, 0, sizeof(*step));
	step->thread_id = 1;
	step->granularity = DAP_STEP_GRANULARITY_INSTRUCTION;
	CHECK(call_cb(DAP_CMD_NEXT) == 0, "step (next)");
	CHECK(g_cpu.PC == 0x1002, "PC advanced by one instruction");

	/* Stepping off a breakpoint the CPU is parked on must execute the
	 * instruction, not immediately re-break (resume-skip logic) */
	g_cpu.PC = 0x1000;
	int bp = bp_add(g_m.bp_mgr, 0x1000, false);
	CHECK(bp >= 0, "bp at parked PC set");
	g_m.stop_reason = STOP_BREAKPOINT; /* as if we just stopped here */
	memset(step, 0, sizeof(*step));
	step->thread_id = 1;
	g_m.stop_reason = STOP_NONE;
	CHECK(call_cb(DAP_CMD_NEXT) == 0, "step off breakpoint");
	CHECK(g_cpu.PC == 0x1002, "stepped past parked breakpoint");
	CHECK(g_m.stop_reason == STOP_NONE, "no spurious breakpoint stop");
	bp_delete(g_m.bp_mgr, bp);

	/* Step while running is rejected */
	g_m.run_flag = 1;
	CHECK(call_cb(DAP_CMD_NEXT) != 0, "step while running rejected");
	g_m.run_flag = 0;

	/* Pause stops the machine */
	g_m.run_flag = 1;
	CHECK(call_cb(DAP_CMD_PAUSE) == 0, "pause");
	CHECK(g_m.run_flag == 0, "pause cleared run flag");

	/* configurationDone bookkeeping */
	g_srv->debugger_state.configuration_done = false;
	CHECK(call_cb(DAP_CMD_CONFIGURATION_DONE) == 0, "configurationDone");
	CHECK(g_srv->debugger_state.configuration_done, "flag set");
}

/* ── Stack trace ───────────────────────────────────────────────── */

static void test_stack_trace(void) {
	StackTraceCommandContext* ctx = &g_srv->current_command.context.stack_trace;
	g_cpu.PC = 0x08023E9C;
	memset(ctx, 0, sizeof(*ctx));
	ctx->levels = 10;
	CHECK(call_cb(DAP_CMD_STACK_TRACE) == 0, "stackTrace");
	CHECK(ctx->frame_count == 1, "one frame");
	CHECK(ctx->frames && ctx->frames[0].instruction_pointer_reference == (int)0x08023E9C,
	      "frame IP is PC");
	CHECK(ctx->frames && ctx->frames[0].name != NULL, "frame has a name");
	if (ctx->frames) {
		free(ctx->frames[0].name);
		free(ctx->frames[0].source_path);
		free(ctx->frames[0].source_name);
		free(ctx->frames);
	}
}

/* ── Disassemble ───────────────────────────────────────────────── */

static void test_disassemble(void) {
	DisassembleCommandContext* ctx = &g_srv->current_command.context.disassemble;

	/* Two adds back to back at 0x1000 (placed by execution test) */
	g_m.memory[0x1000] = 0x6C;
	g_m.memory[0x1001] = 0x00;
	g_m.memory[0x1002] = 0x6C;
	g_m.memory[0x1003] = 0x01;
	memset(ctx, 0, sizeof(*ctx));
	ctx->memory_reference = 0x1000;
	ctx->instruction_count = 2;
	CHECK(call_cb(DAP_CMD_DISASSEMBLE) == 0, "disassemble");
	CHECK(ctx->actual_instruction_count == 2, "two instructions decoded");
	CHECK(ctx->instructions && ctx->instructions[0].address &&
	      strcmp(ctx->instructions[0].address, "0x00001000") == 0,
	      "first address formatted");
	for (int i = 0; i < ctx->actual_instruction_count; i++) {
		free(ctx->instructions[i].address);
		free(ctx->instructions[i].instruction);
		free(ctx->instructions[i].symbol);
	}
	free(ctx->instructions);
}

int main(void) {
	printf("DAP adapter unit tests\n");

	nd500_machine_init(&g_m, 1024 * 1024);
	nd500_cpu_init(&g_cpu, &g_m);
	nd500_cpu_reset(&g_cpu);

	DAPServerConfig config;
	memset(&config, 0, sizeof(config));
	config.transport.type = DAP_TRANSPORT_TCP;
	config.transport.config.tcp.host = "localhost";
	config.transport.config.tcp.port = 0;
	g_srv = dap_server_create(&config);
	if (!g_srv) {
		printf("FATAL: dap_server_create failed\n");
		return 1;
	}
	if (nd500_dap_bind(&g_m, g_srv) != 0) {
		printf("FATAL: nd500_dap_bind failed\n");
		return 1;
	}
	/* Tests drive callbacks without a transport; mark session live */
	g_srv->attached = true;
	g_srv->is_running = false;

	test_instruction_breakpoints();
	test_data_breakpoint_info();
	test_set_data_breakpoints();
	test_register_watchpoints();
	test_memory_roundtrip();
	test_scopes_and_variables();
	test_evaluate();
	test_execution_control();
	test_stack_trace();
	test_disassemble();

	printf("%d checks, %d failures\n", g_tests_run, g_tests_failed);
	return g_tests_failed == 0 ? 0 : 1;
}
