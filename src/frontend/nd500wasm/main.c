#include <stdint.h>
#include <stdlib.h>
#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#ifndef HAVE_SYSTEM_CJSON
#include <cjson/cJSON.h>
#endif

static Nd500Machine g_machine;
static Nd500Cpu g_cpu;

void nd500wasm_init(void) {
	nd500_machine_init(&g_machine, 8 * 1024 * 1024);
	nd500_cpu_init(&g_cpu, &g_machine);
	nd500_cpu_reset(&g_cpu);
}

static char* dup_json_string(cJSON* obj) {
	char* s = cJSON_PrintUnformatted(obj);
	cJSON_Delete(obj);
	return s ? s : "{}";
}

const char* nd500_dbg_mem_json(uint32_t addr, uint32_t len) {
	cJSON* root = cJSON_CreateObject();
	uint32_t cap = len;
	uint8_t* buf = (uint8_t*)malloc(cap);
	if (!buf) return "{}";
	size_t got = nd500_dbg_mem_dump(&g_machine, addr, len, buf, cap);
	cJSON_AddNumberToObject(root, "addr", addr);
	cJSON_AddNumberToObject(root, "len", (double)got);
	cJSON* arr = cJSON_CreateArray();
	for (size_t i = 0; i < got; ++i) {
		char tmp[3];
		snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
		cJSON_AddItemToArray(arr, cJSON_CreateString(tmp));
	}
	cJSON_AddItemToObject(root, "bytes", arr);
	free(buf);
	return dup_json_string(root);
}

const char* nd500_dbg_disasm_json(uint32_t addr, uint32_t len) {
	cJSON* root = cJSON_CreateObject();
	char txt[1024];
	size_t n = nd500_dbg_disasm(&g_machine, addr, len, txt, sizeof(txt));
	cJSON_AddNumberToObject(root, "addr", addr);
	cJSON_AddNumberToObject(root, "len", (double)len);
	cJSON_AddNumberToObject(root, "out_len", (double)n);
	cJSON_AddStringToObject(root, "text", txt);
    /* Optionally include symbol name */
    /* In WASM we did not load symbols; skip for now */
	return dup_json_string(root);
}

const char* nd500_dbg_regs_json(void) {
	if (!g_machine.cpu) return "{}";
	Nd500Regs r; memset(&r, 0, sizeof(r));
	nd500_cpu_get_regs(g_machine.cpu, &r);
	cJSON* root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "PC", r.PC);
	cJSON_AddNumberToObject(root, "FLAGS", r.FLAGS);
	cJSON* I = cJSON_CreateArray();
	for (int i = 0; i < 4; ++i) cJSON_AddItemToArray(I, cJSON_CreateNumber(r.I[i]));
	cJSON* A = cJSON_CreateArray();
	for (int i = 0; i < 4; ++i) cJSON_AddItemToArray(A, cJSON_CreateNumber(r.A[i]));
	cJSON* E = cJSON_CreateArray();
	for (int i = 0; i < 4; ++i) cJSON_AddItemToArray(E, cJSON_CreateNumber(r.E[i]));
	cJSON_AddItemToObject(root, "I", I);
	cJSON_AddItemToObject(root, "A", A);
	cJSON_AddItemToObject(root, "E", E);
	cJSON_AddNumberToObject(root, "L", r.L);
	cJSON_AddNumberToObject(root, "B", r.B);
	cJSON_AddNumberToObject(root, "R", r.R);
	cJSON_AddNumberToObject(root, "TOS", r.TOS);
	cJSON_AddNumberToObject(root, "LL", r.LL);
	cJSON_AddNumberToObject(root, "HL", r.HL);
	cJSON_AddNumberToObject(root, "THA", r.THA);
	cJSON_AddNumberToObject(root, "OTE1", r.OTE1);
	cJSON_AddNumberToObject(root, "OTE2", r.OTE2);
	cJSON_AddNumberToObject(root, "CTE1", r.CTE1);
	cJSON_AddNumberToObject(root, "CTE2", r.CTE2);
	cJSON_AddNumberToObject(root, "MTE1", r.MTE1);
	cJSON_AddNumberToObject(root, "MTE2", r.MTE2);
	cJSON_AddNumberToObject(root, "TEMM1", r.TEMM1);
	cJSON_AddNumberToObject(root, "TEMM2", r.TEMM2);
	return dup_json_string(root);
}

void nd500_dbg_step_js(uint32_t n) { nd500_dbg_step(&g_machine, n ? n : 1); }
void nd500_dbg_run_js(void) { nd500_dbg_run(&g_machine); }
void nd500_dbg_stop_js(void) { nd500_dbg_stop(&g_machine); }
int nd500_dbg_load_aout_js(const uint8_t* data, uint32_t size) { return nd500_dbg_load_aout_buffer(&g_machine, data, size, NULL); }


