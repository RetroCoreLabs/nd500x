#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ndlib.h"

/* Symbol cache for fast lookup */
typedef struct {
    char* name;
    uint32_t addr;
    uint8_t type;  /* 0x00=UNDF, 0x04=TEXT, 0x06=DATA, 0x08=BSS, +0x01=EXT */
} SymbolEntry;

/* Relocation entry */
typedef struct {
    uint32_t address;      /* Address in text/data segment */
    int symbol_index;      /* Index into symbol table */
    char* symbol_name;     /* Symbol name */
    uint8_t is_undefined;  /* 1 if UNDF|EXT */
} RelocationEntry;

static SymbolEntry* g_symbols = NULL;
static int g_symbol_count = 0;

static RelocationEntry* g_text_relocs = NULL;
static int g_text_reloc_count = 0;

/* ND-500 a.out structures (same as ndlib_aout.c) */
struct nd500_exec {
    unsigned int a_magic, a_text, a_data, a_bss, a_syms, a_entry, a_trsize, a_drsize;
};
struct nd500_nlist {
    unsigned int n_strx;
    unsigned int _pad1;
    unsigned char n_type;
    unsigned char n_other;
    unsigned short n_desc;
    unsigned int n_value;
    unsigned int _pad2, _pad3;
};

#define OMAGIC 0407
#define NMAGIC 0410
#define ZMAGIC 0413
#define IMAGIC 0411

static int bad_magic(unsigned int m) {
    return !(m == OMAGIC || m == NMAGIC || m == ZMAGIC || m == IMAGIC);
}

void ndlib_symbols_clear(void) {
    if (g_symbols) {
        for (int i = 0; i < g_symbol_count; i++) {
            free(g_symbols[i].name);
        }
        free(g_symbols);
        g_symbols = NULL;
    }
    g_symbol_count = 0;
    
    /* Clear relocations */
    if (g_text_relocs) {
        for (int i = 0; i < g_text_reloc_count; i++) {
            free(g_text_relocs[i].symbol_name);
        }
        free(g_text_relocs);
        g_text_relocs = NULL;
    }
    g_text_reloc_count = 0;
}

int ndlib_symbols_load(const char* aout_path) {
    if (!aout_path) return -1;
    
    /* Clear existing symbols */
    ndlib_symbols_clear();
    
    FILE* f = fopen(aout_path, "rb");
    if (!f) return -1;
    
    struct nd500_exec hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) || bad_magic(hdr.a_magic)) {
        fclose(f);
        return -1;
    }
    
    if (hdr.a_syms == 0) {
        fclose(f);
        return 0;  /* No symbols, but not an error */
    }
    
    /* Calculate offsets */
    unsigned int sym_off = sizeof(hdr) + hdr.a_text + hdr.a_data + hdr.a_trsize + hdr.a_drsize;
    unsigned int str_off = sym_off + hdr.a_syms;
    int nsyms = hdr.a_syms / sizeof(struct nd500_nlist);
    
    /* Read symbols */
    struct nd500_nlist* symbols = malloc(hdr.a_syms);
    if (!symbols) { fclose(f); return -1; }
    fseek(f, sym_off, SEEK_SET);
    if (fread(symbols, 1, hdr.a_syms, f) != hdr.a_syms) {
        free(symbols); fclose(f); return -1;
    }
    
    /* Read string table */
    unsigned int strsize = 0;
    fseek(f, str_off, SEEK_SET);
    if (fread(&strsize, 1, 4, f) != 4) {
        free(symbols); fclose(f); return -1;
    }
    char* strings = malloc(strsize);
    if (!strings) { free(symbols); fclose(f); return -1; }
    fseek(f, str_off, SEEK_SET);
    if (fread(strings, 1, strsize, f) != strsize) {
        free(symbols); free(strings); fclose(f); return -1;
    }
    fclose(f);
    
    /* Build symbol cache */
    g_symbols = calloc(nsyms, sizeof(SymbolEntry));
    if (!g_symbols) {
        free(symbols); free(strings); return -1;
    }
    
    int idx = 0;
    for (int i = 0; i < nsyms; i++) {
        if (symbols[i].n_strx == 0 || symbols[i].n_strx >= strsize) continue;
        
        const char* name = strings + symbols[i].n_strx;
        g_symbols[idx].name = strdup(name);
        g_symbols[idx].addr = symbols[i].n_value;
        g_symbols[idx].type = symbols[i].n_type;
        idx++;
    }
    g_symbol_count = idx;
    
    /* === Read relocation table === */
    if (hdr.a_trsize > 0) {
        /* ND-500 relocation_info structure (8 bytes) */
        struct nd500_reloc {
            uint32_t r_address;
            uint32_t r_symbolnum : 24;
            uint32_t r_pcrel : 1;
            uint32_t r_length : 2;
            uint32_t r_extern : 1;
            uint32_t r_pad : 4;
        } __attribute__((packed));
        
        int nrelocs = hdr.a_trsize / 8;  /* 8 bytes per relocation */
        struct nd500_reloc* reloc_table = malloc(hdr.a_trsize);
        if (reloc_table) {
            unsigned int reloc_off = sizeof(hdr) + hdr.a_text + hdr.a_data;
            FILE* f2 = fopen(aout_path, "rb");
            if (f2) {
                fseek(f2, reloc_off, SEEK_SET);
                if (fread(reloc_table, 1, hdr.a_trsize, f2) == hdr.a_trsize) {
                    /* Build relocation array */
                    g_text_relocs = calloc(nrelocs, sizeof(RelocationEntry));
                    if (g_text_relocs) {
                        g_text_reloc_count = 0;
                        for (int i = 0; i < nrelocs; i++) {
                            g_text_relocs[g_text_reloc_count].address = reloc_table[i].r_address;
                            g_text_relocs[g_text_reloc_count].symbol_index = reloc_table[i].r_symbolnum;
                            g_text_relocs[g_text_reloc_count].symbol_name = NULL;
                            g_text_relocs[g_text_reloc_count].is_undefined = 0;
                            
                            /* Find symbol name if external */
                            if (reloc_table[i].r_extern && reloc_table[i].r_symbolnum < (uint32_t)g_symbol_count) {
                                g_text_relocs[g_text_reloc_count].symbol_name = strdup(g_symbols[reloc_table[i].r_symbolnum].name);
                                /* Check if it's undefined */
                                if ((g_symbols[reloc_table[i].r_symbolnum].type & 0x0E) == 0x00) {
                                    g_text_relocs[g_text_reloc_count].is_undefined = 1;
                                }
                            }
                            g_text_reloc_count++;
                        }
                    }
                }
                fclose(f2);
            }
            free(reloc_table);
        }
    }
    
    free(symbols);
    free(strings);
    return 0;
}

const char* ndlib_symbols_name_for_addr(uint32_t addr) {
    for (int i = 0; i < g_symbol_count; i++) {
        if (g_symbols[i].addr == addr && (g_symbols[i].type & 0x0E) != 0x00) {
            return g_symbols[i].name;
        }
    }
    return NULL;
}

const char* ndlib_symbols_unresolved_for_addr(uint32_t addr) {
    /* Return unresolved symbol name if address matches an UNDF|EXT symbol */
    for (int i = 0; i < g_symbol_count; i++) {
        if (g_symbols[i].addr == addr && 
            (g_symbols[i].type & 0x0E) == 0x00 &&  /* UNDF */
            (g_symbols[i].type & 0x01)) {           /* EXT */
            return g_symbols[i].name;
        }
    }
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

void ndlib_symbols_list_unresolved(void) {
    int count = 0;
    for (int i = 0; i < g_symbol_count; i++) {
        if ((g_symbols[i].type & 0x0E) == 0x00 && (g_symbols[i].type & 0x01)) {
            count++;
        }
    }
    
    if (count == 0) return;
    
    printf("; Unresolved Externals: %d\n", count);
    for (int i = 0; i < g_symbol_count; i++) {
        if ((g_symbols[i].type & 0x0E) == 0x00 && (g_symbols[i].type & 0x01)) {
            printf(";   - %s\n", g_symbols[i].name);
        }
    }
}

/* Find relocation at or within address range */
const char* ndlib_symbols_reloc_for_range(uint32_t start_addr, uint32_t end_addr, uint8_t* out_is_undefined) {
    for (int i = 0; i < g_text_reloc_count; i++) {
        if (g_text_relocs[i].address >= start_addr && g_text_relocs[i].address < end_addr) {
            if (out_is_undefined) {
                *out_is_undefined = g_text_relocs[i].is_undefined;
            }
            return g_text_relocs[i].symbol_name;
        }
    }
    return NULL;
}


