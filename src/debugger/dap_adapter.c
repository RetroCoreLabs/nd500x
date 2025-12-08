#ifdef WITH_DEBUGGER
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../cpu/cpu_protos.h"
#include "../disasm/nd500_disasm.h"
#include "../ndlib/ndlib.h"
#include "../../external/libdap/libdap/include/dap_server.h"

static Nd500Machine* g_machine = NULL;

static int cmd_continue_cb(DAPServer *server) {
	(void)server;
	if (!g_machine) return -1;
	nd500_dbg_run(g_machine);
	return 0;
}

static int cmd_next_cb(DAPServer *server) {
	(void)server;
	if (!g_machine) return -1;
	nd500_dbg_step(g_machine, 1);
	return 0;
}

static int cmd_step_in_cb(DAPServer *server) { return cmd_next_cb(server); }
static int cmd_step_out_cb(DAPServer *server) { return cmd_next_cb(server); }

static int cmd_read_memory_cb(DAPServer *server) {
	ReadMemoryCommandContext *ctx = &server->current_command.context.read_memory;
	if (!g_machine) return -1;
	uint32_t addr = ctx->memory_reference + (ctx->offset > 0 ? (uint32_t)ctx->offset : 0);
	int count = ctx->count;
	if (count < 0) count = 0;
	uint8_t* tmp = (uint8_t*)malloc((size_t)count);
	if (!tmp) return -1;
	nd500_dbg_mem_dump(g_machine, addr, (uint32_t)count, tmp, (size_t)count);
	/* Simple hex string as base64 placeholder */
	int outlen = count * 2 + 1;
	char* hex = (char*)malloc((size_t)outlen);
	for (int i = 0; i < count; ++i) sprintf(hex + i*2, "%02X", tmp[i]);
	ctx->base64_data = hex;
	ctx->unreadable_bytes = 0;
	free(tmp);
	return 0;
}

static int cmd_write_memory_cb(DAPServer *server) {
	WriteMemoryCommandContext *ctx = &server->current_command.context.write_memory;
	if (!g_machine) return -1;
	/* Interpret ctx->data as hex string for now */
	const char* s = ctx->data;
	int len = (int)strlen(s) / 2;
	uint8_t* buf = (uint8_t*)malloc((size_t)len);
	for (int i = 0; i < len; ++i) {
		unsigned int v = 0; sscanf(s + i*2, "%02x", &v); buf[i] = (uint8_t)v;
	}
	uint32_t addr = ctx->memory_reference + (ctx->offset > 0 ? (uint32_t)ctx->offset : 0);
	for (int i = 0; i < len; ++i) nd500_bus_write8(g_machine, addr + (uint32_t)i, buf[i]);
	ctx->bytes_written = (uint16_t)len;
	free(buf);
	return 0;
}

static int cmd_disassemble_cb(DAPServer *server) {
	DisassembleCommandContext *ctx = &server->current_command.context.disassemble;
	if (!g_machine) return -1;
	
	int requested_count = ctx->instruction_count > 0 ? ctx->instruction_count : 10;
	ctx->instructions = (DisassembleInstruction*)calloc((size_t)requested_count, sizeof(DisassembleInstruction));
	if (!ctx->instructions) return -1;
	
	uint32_t addr = ctx->memory_reference;
	int count = 0;
	
	for (int i = 0; i < requested_count && addr < g_machine->memory_size; ++i) {
		Nd500FetchedInstruction fi;
		memset(&fi, 0, sizeof(fi));
		int rc = nd500_decode_at(g_machine, addr, &fi);
		
		/* Format address */
		char *addr_str = (char*)malloc(16);
		if (!addr_str) break;
		snprintf(addr_str, 16, "0x%08X", addr);
		
		/* Format instruction */
		char inst_buf[128];
		if (rc != 0 || fi.total_len == 0) {
			/* Unknown opcode - show hex */
			uint8_t b = nd500_bus_read8(g_machine, addr);
			snprintf(inst_buf, sizeof(inst_buf), "??? ; 0x%02X", b);
			fi.total_len = 1;
		} else {
			/* Build instruction string: mnemonic + operands */
			char* p = inst_buf;
			char* end = inst_buf + sizeof(inst_buf);
			const char* mnem = fi.mnemonic ? fi.mnemonic : "???";
			
			/* Add dtype prefix if present */
			if (nd500_instr_has_rn(fi.opcode)) {
				int dreg = nd500_instr_dest_reg(fi.opcode);
				if (dreg >= 0 && dreg < 4) {
					const char* dts = nd500_instr_dtype_prefix(fi.opcode);
					int n = snprintf(p, (size_t)(end-p), "%s%d ", dts, dreg + 1);
					if (n > 0) p += n;
				}
			}
			
			/* Add mnemonic */
			int n = snprintf(p, (size_t)(end-p), "%s", mnem);
			if (n > 0) p += n;
			
			/* Add operands */
			for (uint8_t oi = 0; oi < fi.operand_count && p < end; ++oi) {
				char obuf[64];
				int ol = nd500_format_operand(obuf, sizeof(obuf), &fi.operands[oi], false);
				if (ol > 0) {
					n = snprintf(p, (size_t)(end-p), "%s%s", (oi > 0) ? ", " : " ", obuf);
					if (n > 0) p += n;
				}
			}
		}
		
		char *inst_str = strdup(inst_buf);
		if (!inst_str) { free(addr_str); break; }
		
		/* Look up symbol at this address */
		const char* sym = ndlib_symbols_name_for_addr(addr);
		char *sym_str = (sym && *sym) ? strdup(sym) : NULL;
		
		ctx->instructions[count].address = addr_str;
		ctx->instructions[count].instruction = inst_str;
		ctx->instructions[count].symbol = sym_str;
		count++;
		
		addr += fi.total_len > 0 ? fi.total_len : 1;
	}
	
	ctx->actual_instruction_count = count;
	return 0;
}

static int cmd_launch_cb(DAPServer *server) {
	LaunchCommandContext *ctx = &server->current_command.context.launch;
	(void)server;
	if (!g_machine) return -1;
	if (ctx->program_path && *ctx->program_path) {
		/* Use unified loading function (no auto-map for DAP - let IDE handle it) */
		ndlib_load_aout_with_debug(g_machine, ctx->program_path, 0, NULL, NULL);
	}
	return 0;
}

int nd500_dap_attach(Nd500Machine* m, DAPServer* server) {
	g_machine = m;
	if (!server) return -1;
	dap_server_register_command_callback(server, DAP_CMD_CONTINUE, cmd_continue_cb);
	dap_server_register_command_callback(server, DAP_CMD_NEXT, cmd_next_cb);
	dap_server_register_command_callback(server, DAP_CMD_STEP_IN, cmd_step_in_cb);
	dap_server_register_command_callback(server, DAP_CMD_STEP_OUT, cmd_step_out_cb);
	dap_server_register_command_callback(server, DAP_CMD_READ_MEMORY, cmd_read_memory_cb);
	dap_server_register_command_callback(server, DAP_CMD_WRITE_MEMORY, cmd_write_memory_cb);
	dap_server_register_command_callback(server, DAP_CMD_DISASSEMBLE, cmd_disassemble_cb);
	dap_server_register_command_callback(server, DAP_CMD_LAUNCH, cmd_launch_cb);
	/* Additional commands: provide simple success stubs */
	/* Configuration done */
	dap_server_register_command_callback(server, DAP_CMD_CONFIGURATION_DONE, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	/* Restart */
	dap_server_register_command_callback(server, DAP_CMD_RESTART, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	/* Disconnect */
	dap_server_register_command_callback(server, DAP_CMD_DISCONNECT, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	/* Terminate */
	dap_server_register_command_callback(server, DAP_CMD_TERMINATE, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	/* Set exception breakpoints */
	dap_server_register_command_callback(server, DAP_CMD_SET_EXCEPTION_BREAKPOINTS, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	/* Set breakpoints: clear and accept */
	dap_server_register_command_callback(server, DAP_CMD_SET_BREAKPOINTS, (DAPCommandCallback)[](DAPServer* s){
		dap_server_clear_breakpoints(s);
		BreakpointCommandContext *ctx = &s->current_command.context.breakpoint;
		for (int i = 0; i < ctx->breakpoint_count; ++i) {
			dap_server_add_breakpoint(s, &ctx->breakpoints[i]);
		}
		return 0;
	});
	/* Scopes/Variables/StackTrace/Source: stub success */
	dap_server_register_command_callback(server, DAP_CMD_SCOPES, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	dap_server_register_command_callback(server, DAP_CMD_VARIABLES, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	dap_server_register_command_callback(server, DAP_CMD_SET_VARIABLE, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	dap_server_register_command_callback(server, DAP_CMD_STACK_TRACE, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	dap_server_register_command_callback(server, DAP_CMD_SOURCE, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	/* Control DAP-specific CPU interaction hooks */
	dap_server_register_command_callback(server, DAP_WAIT_FOR_DEBUGGER, (DAPCommandCallback)[](DAPServer* s){ (void)s; if (g_machine) nd500_dbg_stop(g_machine); return 0; });
	dap_server_register_command_callback(server, DAP_RELEASE_DEBUGGER, (DAPCommandCallback)[](DAPServer* s){ (void)s; if (g_machine) nd500_dbg_run(g_machine); return 0; });
	dap_server_register_command_callback(server, DAP_CHECK_CPU_EVENTS, (DAPCommandCallback)[](DAPServer* s){ (void)s; return 0; });
	return 0;
}

int nd500_dap_start(Nd500Machine* m, int port) {
	DAPServerConfig config = {
		.transport = {
			.type = DAP_TRANSPORT_TCP,
			.config = { .tcp = { .host = "localhost", .port = port } }
		}
	};
	DAPServer* server = dap_server_create(&config);
	if (!server) return -1;
	if (nd500_dap_attach(m, server) != 0) return -1;
	if (dap_server_start(server) != 0) return -1;
	return 0;
}

#endif /* WITH_DEBUGGER */


