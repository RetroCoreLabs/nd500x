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
const char* ndlib_symbols_name_for_addr(uint32_t addr);
int ndlib_symbols_line_for_addr(uint32_t addr);
void ndlib_symbols_list_all(void);


