#pragma once
#include <stdint.h>
#include "../machine/machine_types.h"

void nd500_log(const char* fmt, ...);

/* ND-500 a.out loader */
int ndlib_loadaout_file(Nd500Machine* m, const char* path, unsigned int* out_entry);
int ndlib_loadaout_file_ex(Nd500Machine* m, const char* path, unsigned int* out_entry, unsigned int* out_text_size);
int ndlib_aout_dump_metadata(const char* path);
void ndlib_aout_dump_symbols(const char* path);
const char* ndlib_aout_get_loaded_path(void);

/* Symbols (optional, requires libsymbols) */
int ndlib_symbols_load(const char* aout_path);
void ndlib_symbols_clear(void);
const char* ndlib_symbols_name_for_addr(uint32_t addr);
const char* ndlib_symbols_unresolved_for_addr(uint32_t addr);
const char* ndlib_symbols_reloc_for_range(uint32_t start_addr, uint32_t end_addr, uint8_t* out_is_undefined);
int ndlib_symbols_line_for_addr(uint32_t addr);
void ndlib_symbols_list_all(void);
void ndlib_symbols_list_unresolved(void);


