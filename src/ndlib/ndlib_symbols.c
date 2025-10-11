#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef HAVE_LIBSYMBOLS
#include "../../external/libsymbols/include/symbols.h"
#endif

/* Thin adapters to query symbols with 32-bit addresses by downcasting to 16-bit for now */

#ifdef HAVE_LIBSYMBOLS
static symbol_table_t* g_symtab = NULL;

int ndlib_symbols_load(const char* aout_path) {
	if (g_symtab) { symbols_free(g_symtab); g_symtab = NULL; }
	g_symtab = symbols_create();
	if (!g_symtab) return -1;
	if (!symbols_load_aout(g_symtab, aout_path)) {
		symbols_free(g_symtab); g_symtab = NULL; return -1;
	}
	return 0;
}

const char* ndlib_symbols_name_for_addr(uint32_t addr) {
	if (!g_symtab) return NULL;
	const symbol_entry_t* e = symbols_lookup_by_address(g_symtab, (uint16_t)(addr & 0xFFFF));
	return e ? e->name : NULL;
}

int ndlib_symbols_line_for_addr(uint32_t addr) {
	if (!g_symtab) return -1;
	return symbols_get_line(g_symtab, (uint16_t)(addr & 0xFFFF));
}

#else
int ndlib_symbols_load(const char* aout_path) { (void)aout_path; return -1; }
const char* ndlib_symbols_name_for_addr(uint32_t addr) { (void)addr; return NULL; }
int ndlib_symbols_line_for_addr(uint32_t addr) { (void)addr; return -1; }
#endif


