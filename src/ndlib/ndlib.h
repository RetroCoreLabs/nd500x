#pragma once
#include <stdint.h>
#include <stdio.h>
#include "../machine/machine_types.h"

void nd500_log(const char* fmt, ...);
/* Set non-zero to suppress nd500_log() informational output (shell clean mode). */
extern int nd500_log_quiet;

/* ND-500 a.out loader */
int ndlib_loadaout_file(Nd500Machine* m, const char* path, unsigned int* out_entry);
int ndlib_loadaout_file_ex(Nd500Machine* m, const char* path, unsigned int* out_entry, unsigned int* out_text_size);
int ndlib_aout_dump_metadata(const char* path);
void ndlib_aout_dump_symbols(const char* path);
const char* ndlib_aout_get_loaded_path(void);
void ndlib_aout_get_segment_info(uint32_t* text_base, uint32_t* text_size,
                                  uint32_t* data_base, uint32_t* data_size,
                                  uint32_t* bss_base, uint32_t* bss_size);
uint32_t ndlib_aout_get_data_base(void);
void ndlib_aout_set_data_base(uint32_t data_base);

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

/* Map file support (source-level debugging) */
int ndlib_map_load(const char* map_path);
const char* ndlib_symbols_file_for_addr(uint32_t addr);
int ndlib_symbols_get_c_mapping(uint32_t addr, const char** out_file, int* out_line);
int ndlib_symbols_get_s_mapping(uint32_t addr, const char** out_file, int* out_line);
int ndlib_symbols_addr_for_line(const char* file, int line, uint32_t* out_addr);
int ndlib_symbols_get_addrs_for_line(const char* file, int line, uint32_t* out_addrs, int max_addrs);
uint32_t ndlib_symbols_first_instruction_addr(void);

/* Source file storage and retrieval */
int ndlib_source_store(const char* filename, const char* content);
const char* ndlib_source_get_line(const char* filename, int line);
const char* ndlib_source_get_content(const char* filename);
int ndlib_source_count_lines(const char* filename);

/* ===================================================================
 * UNIFIED FILE LOADING (eliminates code duplication)
 * ===================================================================
 */

/* Load a.out file with symbols, optional map file, and set PC correctly.
 * This function consolidates the loading logic used by CLI, DAP, and WASM.
 *
 * Parameters:
 *   m            - Machine to load into
 *   aout_path    - Path to .o or .out file (required)
 *   auto_map     - If 1, try to load .map file automatically
 *   out_entry    - Returns entry point from a.out header
 *   out_pc       - Returns PC value set (first instruction or entry point)
 *
 * Returns: 0 on success, -1 on error
 */
int ndlib_load_aout_with_debug(Nd500Machine* m, const char* aout_path,
                                int auto_map, uint32_t* out_entry, uint32_t* out_pc);


/* ===================================================================
 * DOM/SEG FILE LOADING
 * ===================================================================
 */

/* Forward declaration - full type in nd500_dom.h */
typedef union nd500_header nd500_header_t;

/* Load DOM/SEG file header (4096 bytes). File stays open for segment loading.
 * Returns: 0 on success, -1 file not found, -2 read error */
int ndlib_load_dom_header(const char* path);

/* Load all segments from DOM/SEG file into memory.
 * Must call ndlib_load_dom_header() first.
 * Returns: 0 on success, -1 on error */
int ndlib_load_dom_segments(void);

/* Close DOM file, free segment data, and clear state */
void ndlib_close_dom(void);

/* Stage an OLD-FORMAT domain (<name>:PSEG + <name>:DSEG described by an entry
 * in DESCRIPTION-FILE:DESC) exactly as if it had been read from a :DOM, so
 * ndlib_dom_load_to_machine() places it unchanged. Replaces BOTH
 * ndlib_load_dom_header() and ndlib_load_dom_segments() for such a domain.
 * The domain name must match the DESC entry exactly (case-insensitive).
 * Returns: 0 staged, 1 no such domain in this DESC file (caller may try the
 * next one), -1 error with err[] filled (file missing, size does not match
 * the entry, more than one segment - never guessed around). */
int ndlib_load_old_domain(const char* desc_path, const char* domain_name,
                          char* err, size_t err_len);

/* Get loaded header (NULL if not loaded) */
const nd500_header_t* ndlib_get_dom_header(void);

/* Get file handle for reading segment data */
FILE* ndlib_get_dom_file(void);

/* Check if DOM is loaded */
int ndlib_dom_is_loaded(void);

/* Check if loaded file is DOM (1) or SEG (0) */
int ndlib_dom_is_dom_file(void);

/* Get path of loaded DOM file */
const char* ndlib_get_dom_filepath(void);

/* Get number of loaded segments (with data) */
int ndlib_dom_get_segment_count(void);

/* Get segment program data (call after ndlib_load_dom_segments)
 * Returns pointer to data, or NULL if not loaded */
const uint8_t* ndlib_dom_get_segment_data(int index, uint32_t* out_size, uint32_t* out_load_addr);

/* Get segment data section (call after ndlib_load_dom_segments) */
const uint8_t* ndlib_dom_get_data_section(int index, uint32_t* out_size, uint32_t* out_load_addr);

/* Get segment info from header (does not require loading segment data)
 * Returns: 0 on success, -1 on error */
int ndlib_dom_get_segment_info(int index, uint32_t* prog_size, uint32_t* prog_addr,
                               uint32_t* data_size, uint32_t* data_addr,
                               int* is_linked, int* is_used);

/* Forward declaration for CPU type */
typedef struct Nd500Cpu Nd500Cpu;

/* Load DOM/SEG into machine with full MMU and domain setup.
 * Must call ndlib_load_dom_header() and ndlib_load_dom_segments() first.
 *
 * Parameters:
 *   m              - Machine to load into
 *   cpu            - CPU to configure (domain and MMU)
 *   target_domain  - Domain to load into: -1 = auto-allocate (1-255), 0-255 = specific domain
 *   log_callback   - Optional callback for progress messages (NULL to suppress)
 *   log_context    - Context passed to log_callback
 *   out_start_addr - Returns start address from header (may be NULL)
 *   out_domain     - Returns actual domain loaded into (may be NULL)
 *
 * Returns: 0 on success, -1 on error
 */
int ndlib_dom_load_to_machine(
    Nd500Machine* m,
    Nd500Cpu* cpu,
    int target_domain,
    void (*log_callback)(void* ctx, const char* fmt, ...),
    void* log_context,
    uint32_t* out_start_addr,
    int* out_domain);

/* Simple printf-based log callback for command line use */
void ndlib_dom_log_printf(void* ctx, const char* fmt, ...);

