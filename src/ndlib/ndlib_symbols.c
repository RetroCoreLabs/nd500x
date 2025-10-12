#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ndlib.h"

/* Simple symbol support without libsymbols - read directly from a.out */

int ndlib_symbols_load(const char* aout_path) {
    /* Symbols are read on-demand from a.out file */
    (void)aout_path;
    return 0;
}

const char* ndlib_symbols_name_for_addr(uint32_t addr) {
    /* TODO: Implement symbol lookup from cached a.out */
    (void)addr;
    return NULL;
}

int ndlib_symbols_line_for_addr(uint32_t addr) {
    (void)addr;
    return -1;
}

void ndlib_symbols_list_all(void) {
    const char* path = ndlib_aout_get_loaded_path();
    if (path) {
        ndlib_aout_dump_symbols(path);
    } else {
        printf("No file loaded\n");
    }
}


