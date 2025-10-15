#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "machine_protos.h"

#ifdef HAVE_LIBSYMBOLS
#include "../../external/libsymbols/include/symbols.h"
#include "../../external/libsymbols/include/aout.h"
#include "../../external/libsymbols/include/mapfile.h"
#endif

int nd500_dbg_load_aout_buffer(Nd500Machine* m, const uint8_t* data, size_t size, uint32_t* out_entry_pc) {
	if (!m || !data || size == 0) return -1;
#ifdef HAVE_LIBSYMBOLS
	/* TODO: parse a.out and map segments properly using libsymbols */
	/* Placeholder: copy to base and set entry 0 */
#endif
	if (size > m->memory_size) size = m->memory_size;
	memcpy(m->memory, data, size);
	if (out_entry_pc) *out_entry_pc = 0;
	return 0;
}

int nd500_dbg_load_aout_file(Nd500Machine* m, const char* path, uint32_t* out_entry_pc) {
	if (!m || !path) return -1;
	FILE* f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	if (sz < 0) { fclose(f); return -1; }
	fseek(f, 0, SEEK_SET);
	uint8_t* buf = (uint8_t*)malloc((size_t)sz);
	if (!buf) { fclose(f); return -1; }
	size_t rd = fread(buf, 1, (size_t)sz, f);
	fclose(f);
	int rc = nd500_dbg_load_aout_buffer(m, buf, rd, out_entry_pc);
	free(buf);
	return rc;
}

/* Generic helper: load a binary file into memory at base address */
int nd500_load_file_to_memory(Nd500Machine* m, const char* path, uint32_t base_addr) {
    if (!m || !path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    int rc = 0;
    uint8_t buffer[4096];
    uint32_t offset = 0;

    /* Check if MMU is enabled - if so, allow virtual addresses */
    int mmu_enabled = nd500_machine_mmu_is_enabled(m);

    for (;;) {
        size_t rd = fread(buffer, 1, sizeof(buffer), f);
        if (rd == 0) break;

        /* Only check physical memory range if MMU is disabled */
        if (!mmu_enabled && (base_addr + offset + (uint32_t)rd > m->memory_size)) {
            rc = -2; /* out of range */
            break;
        }

        /* nd500_bus_write8 will handle MMU translation if enabled */
        for (size_t i = 0; i < rd; ++i) {
            nd500_bus_write8(m, base_addr + (uint32_t)offset + (uint32_t)i, buffer[i]);
        }
        offset += (uint32_t)rd;
    }
    fclose(f);
    return rc;
}

int nd500_load_pseg_file(Nd500Machine* m, const char* path, uint32_t pseg_base_addr) {
    return nd500_load_file_to_memory(m, path, pseg_base_addr);
}

int nd500_load_dseg_file(Nd500Machine* m, const char* path, uint32_t dseg_base_addr) {
    return nd500_load_file_to_memory(m, path, dseg_base_addr);
}

/* Helper: Get descriptive error message for load failures */
const char* nd500_load_strerror(int error_code, uint32_t attempted_addr, uint32_t mem_size) {
    static char error_buffer[512];

    switch (error_code) {
        case -1:
            return "Cannot open file or invalid parameters";

        case -2: {
            uint32_t mem_mb = mem_size / (1024 * 1024);
            snprintf(error_buffer, sizeof(error_buffer),
                "Address 0x%08X is out of range. Emulator has %uMB physical memory "
                "(0x00000000-0x%08X) and MMU is disabled.\n"
                "                 Solution: Enable MMU with 'mmusetup' command first:\n"
                "                   1. Run 'mmusetup' to configure virtual memory\n"
                "                   2. Then load files at virtual addresses (0x08000000 for kernel)\n"
                "                 Or load at physical addresses:\n"
                "                   load pseg <file> kernel 0x00000000\n"
                "                   load dseg <file> kernel 0x00400000",
                attempted_addr, mem_mb, mem_size - 1);
            return error_buffer;
        }

        case 0:
            return "Success";

        default:
            snprintf(error_buffer, sizeof(error_buffer), "Unknown error code: %d", error_code);
            return error_buffer;
    }
}


