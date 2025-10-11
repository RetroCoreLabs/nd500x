#pragma once
#include <stdint.h>
#include "../machine/machine_types.h"

void nd500_log(const char* fmt, ...);

/* ND-500 a.out loader */
int ndlib_loadaout_file(Nd500Machine* m, const char* path, unsigned int* out_entry);

/* Symbols (optional, requires libsymbols) */
int ndlib_symbols_load(const char* aout_path);
const char* ndlib_symbols_name_for_addr(uint32_t addr);
int ndlib_symbols_line_for_addr(uint32_t addr);


