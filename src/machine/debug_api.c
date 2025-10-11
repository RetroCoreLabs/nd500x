#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "machine_protos.h"
#include "../cpu/cpu_protos.h"
#include "../ndlib/ndlib.h"

#include "../cpu/cpu_protos.h"

size_t nd500_dbg_mem_dump(Nd500Machine* m, uint32_t addr, uint32_t len, uint8_t* out, size_t out_cap) {
	if (!m || !out || out_cap == 0) return 0;
	if (addr >= m->memory_size) return 0;
	uint32_t max = (uint32_t)((addr + len) > m->memory_size ? (m->memory_size - addr) : len);
	if (max > out_cap) max = (uint32_t)out_cap;
	memcpy(out, m->memory + addr, max);
	return max;
}

size_t nd500_dbg_disasm(Nd500Machine* m, uint32_t addr, uint32_t len, char* out, size_t out_cap) {
    /* Enrich with symbol name if available; still hex dump for now */
	if (!m || !out || out_cap == 0) return 0;
	char* p = out;
	char* end = out + out_cap;
    const char* sym = ndlib_symbols_name_for_addr(addr);
    if (sym) {
        int n = snprintf(p, (size_t)(end - p), "%08X <%s>: ", addr, sym);
        if (n > 0) p += (n < (end - p) ? n : (int)(end - p));
    } else {
        int n = snprintf(p, (size_t)(end - p), "%08X: ", addr);
        if (n > 0) p += (n < (end - p) ? n : (int)(end - p));
    }
    uint32_t end_addr = addr + len;
	if (end_addr > m->memory_size) end_addr = m->memory_size;
    for (uint32_t a = addr; a < end_addr;) {
        Nd500FetchedInstruction fi;
        if (nd500_decode_at(m, a, &fi) != 0) break;
        int n = snprintf(p, (size_t)(end - p), "%08X: %s  ", fi.address, fi.mnemonic ? fi.mnemonic : "???");
        if (n <= 0 || p + n >= end) break;
        p += n;
        for (int i = 0; i < fi.opcode_len && (a + (uint32_t)i) < end_addr; ++i) {
            n = snprintf(p, (size_t)(end - p), "%02X ", m->memory[a + (uint32_t)i]);
            if (n <= 0 || p + n >= end) break;
            p += n;
        }
        n = snprintf(p, (size_t)(end - p), "\n");
        if (n <= 0 || p + n >= end) break;
        p += n;
        a += (uint32_t)fi.opcode_len;
    }
	if (p < end) *p = '\0';
	return (size_t)(p - out);
}

void nd500_dbg_step(Nd500Machine* m, uint32_t count) {
	if (!m || !m->cpu) return;
	for (uint32_t i = 0; i < count; ++i) {
		nd500_cpu_step(m->cpu);
	}
}

#ifndef __unix__
void nd500_dbg_run(Nd500Machine* m) {
	if (!m) return;
	m->run_flag = 1;
}

void nd500_dbg_stop(Nd500Machine* m) {
	if (!m) return;
	m->run_flag = 0;
}
#endif

int nd500_dbg_is_running(Nd500Machine* m) {
	return m ? m->run_flag : 0;
}

/* Optional: expose regs snapshot through machine */
void nd500_dbg_regs(struct Nd500Cpu* cpu, Nd500Regs* out_regs) {
	nd500_cpu_get_regs(cpu, out_regs);
}

int nd500_dbg_load_aout_file(Nd500Machine* m, const char* path, uint32_t* out_entry_pc) {
	/* TODO: integrate libsymbols; placeholder just zero entry */
	if (out_entry_pc) *out_entry_pc = 0;
	(void)m; (void)path;
	return 0;
}

int nd500_dbg_load_aout_buffer(Nd500Machine* m, const uint8_t* data, size_t size, uint32_t* out_entry_pc) {
	/* TODO: integrate libsymbols; placeholder copy to base */
	if (!m || !data || size == 0) return -1;
	if (size > m->memory_size) size = m->memory_size;
	memcpy(m->memory, data, size);
	if (out_entry_pc) *out_entry_pc = 0;
	return 0;
}


