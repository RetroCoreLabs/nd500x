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


