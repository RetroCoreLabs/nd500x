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
void ndlib_aout_get_segment_info(uint32_t* text_base, uint32_t* text_size,
                                  uint32_t* data_base, uint32_t* data_size,
                                  uint32_t* bss_base, uint32_t* bss_size);

/* Symbols (optional, requires libsymbols) */
int ndlib_symbols_load(const char* aout_path);
void ndlib_symbols_clear(void);
const char* ndlib_symbols_name_for_addr(uint32_t addr);
const char* ndlib_symbols_unresolved_for_addr(uint32_t addr);
const char* ndlib_symbols_reloc_for_range(uint32_t start_addr, uint32_t end_addr, uint8_t* out_is_undefined);
int ndlib_symbols_line_for_addr(uint32_t addr);
void ndlib_symbols_list_all(void);
void ndlib_symbols_list_unresolved(void);
int ndlib_symbols_get_count(void);
const char* ndlib_symbols_get_name(int index);
uint32_t ndlib_symbols_get_addr(int index);
uint8_t ndlib_symbols_get_type(int index);

/* Symbol lookup and filtering (for CLI commands) */
int ndlib_symbols_lookup(const char* name, uint32_t* out_addr, uint8_t* out_type);
int ndlib_symbols_absolute_addr(const char* name, uint32_t* out_addr);
void ndlib_symbols_list_by_type(uint8_t seg_type);


