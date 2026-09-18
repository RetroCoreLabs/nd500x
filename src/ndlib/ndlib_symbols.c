#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ndlib.h"
#include <stdlib.h>

/* Loader chatter is diagnostic, not user-facing: gate it behind
 * ND500X_LOADDBG so a normal boot shows only real information. */
static int ndlib_loaddbg(void) {
    static int v = -1;
    if (v < 0) { const char* e = getenv("ND500X_LOADDBG"); v = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return v;
}


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

/* Source line mapping (from .map file) */
typedef struct {
    char* source_file;     /* Source filename */
    int line_number;       /* Line number in source */
    uint32_t address;      /* Memory address */
    int is_text;           /* 1 = TEXT (code), 0 = DATA (variable) */
} SourceLineEntry;

/* Source file content cache */
typedef struct {
    char* filename;
    char* content;         /* Full file content */
    char** lines;          /* Array of line pointers */
    int line_count;
} SourceFileCache;

static SymbolEntry* g_symbols = NULL;
static int g_symbol_count = 0;

static RelocationEntry* g_text_relocs = NULL;
static int g_text_reloc_count = 0;

static SourceLineEntry* g_source_lines = NULL;
static int g_source_line_count = 0;

#define MAX_SOURCE_FILES 32
static SourceFileCache g_source_files[MAX_SOURCE_FILES];
static int g_source_file_count = 0;

/* ND-500 a.out structures (same as ndlib_aout.c) */
struct nd500_exec {
    unsigned int a_magic, a_text, a_data, a_bss, a_syms, a_entry, a_trsize, a_drsize;
};
/* On-disk symbol table format - 12 bytes */
struct nd500_nlist {
    int32_t n_strx;         /* String table index (4 bytes) */
    unsigned char n_type;   /* Type flag (1 byte) */
    unsigned char n_other;  /* Unused (1 byte) */
    int16_t n_desc;         /* Description (2 bytes) */
    uint32_t n_value;       /* Value/address (4 bytes) */
} __attribute__((packed));

#define OMAGIC 0407
#define NMAGIC 0410
#define ZMAGIC 0413
#define IMAGIC 0411

static int bad_magic(unsigned int m) {
    return !(m == OMAGIC || m == NMAGIC || m == ZMAGIC || m == IMAGIC);
}

/*
 * ND-500 a.out metadata (header, symbol table, relocations) is stored
 * big-endian on disk. This emulator runs on a little-endian host, so the
 * multi-byte fields must be decoded big-endian rather than read raw.
 */
static uint16_t sym_be16(const unsigned char *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}
static uint32_t sym_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
/* Read the fixed 32-byte exec header big-endian. All eight fields are 32-bit
 * big-endian at offsets 0,4,8,12,16,20,24,28 (the magic is a 32-bit field at
 * offset 0, matching the kernel's int Ux_mag in h/user.h). */
static uint32_t sym_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t sym_le16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
/* out_le (may be NULL) reports the detected byte order so the symbol/string
 * decode below uses the SAME order as the header (the records are in the same
 * endianness as the exec header, not always big-endian). */
static int sym_read_exec_be(FILE *f, struct nd500_exec *h, int *out_le) {
    unsigned char b[32];
    if (fread(b, 1, sizeof(b), f) != sizeof(b))
        return -1;
    /* Big-endian is the current on-disk format. Legacy pre-flip images (an
     * older LE-built vmunix, magic bytes 09 01 00 00) are little-endian;
     * auto-detect so both load. Mirrors read_exec_be in ndlib_aout.c. */
    uint32_t (*rd)(const unsigned char *) = sym_be32;
    int is_le = 0;
    if (bad_magic(sym_be32(b + 0)) && !bad_magic(sym_le32(b + 0))) {
        rd = sym_le32; is_le = 1;
    }
    if (out_le) *out_le = is_le;
    h->a_magic  = rd(b + 0);
    h->a_text   = rd(b + 4);
    h->a_data   = rd(b + 8);
    h->a_bss    = rd(b + 12);
    h->a_syms   = rd(b + 16);
    h->a_entry  = rd(b + 20);
    h->a_trsize = rd(b + 24);
    h->a_drsize = rd(b + 28);
    return 0;
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

    /* Clear source line mappings */
    if (g_source_lines) {
        for (int i = 0; i < g_source_line_count; i++) {
            free(g_source_lines[i].source_file);
        }
        free(g_source_lines);
        g_source_lines = NULL;
    }
    g_source_line_count = 0;

    /* Clear source file cache */
    for (int i = 0; i < g_source_file_count; i++) {
        free(g_source_files[i].filename);
        free(g_source_files[i].content);
        free(g_source_files[i].lines);
    }
    g_source_file_count = 0;
}

int ndlib_symbols_load(const char* aout_path) {
    if (!aout_path) return -1;

    /* Clear existing symbols */
    ndlib_symbols_clear();

    FILE* f = fopen(aout_path, "rb");
    if (!f) return -1;

    struct nd500_exec hdr;
    int is_le = 0;
    if (sym_read_exec_be(f, &hdr, &is_le) != 0 || bad_magic(hdr.a_magic)) {
        fclose(f);
        return -1;
    }
    /* Symbol/string records share the header's byte order. */
    uint32_t (*rd32)(const unsigned char *) = is_le ? sym_le32 : sym_be32;
    uint16_t (*rd16)(const unsigned char *) = is_le ? sym_le16 : sym_be16;

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

    /* Symbol records are stored big-endian; decode the multi-byte fields.
     * n_type and n_other are single bytes and need no swapping. */
    for (int i = 0; i < nsyms; i++) {
        unsigned char *sb = (unsigned char *)&symbols[i];
        symbols[i].n_strx  = (int32_t)rd32(sb + 0);
        symbols[i].n_desc  = (int16_t)rd16(sb + 6);
        symbols[i].n_value = rd32(sb + 8);
    }


    /* Read string table size prefix (stored big-endian, target order) */
    unsigned int strsize = 0;
    unsigned char strszbuf[4];
    fseek(f, str_off, SEEK_SET);
    if (fread(strszbuf, 1, 4, f) != 4) {
        free(symbols); fclose(f); return -1;
    }
    strsize = rd32(strszbuf);
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
        if (i < 3) {  /* Debug first 3 symbols */
            if (ndlib_loaddbg()) fprintf(stderr, "[DEBUG] Symbol %d: n_strx=%d n_type=0x%02x n_value=0x%08x name='%s'\n",
                    i, symbols[i].n_strx, symbols[i].n_type, symbols[i].n_value, name);
        }
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
                    /* Relocations are stored big-endian: r_address (32) then a
                     * big-endian MSB-first bitfield word (symbolnum in bits
                     * 31..8, pcrel bit 7, length bits 6..5, extern bit 4). */
                    for (int i = 0; i < nrelocs; i++) {
                        unsigned char *rb = (unsigned char *)&reloc_table[i];
                        uint32_t w = sym_be32(rb + 4);
                        reloc_table[i].r_address   = sym_be32(rb + 0);
                        reloc_table[i].r_symbolnum = (w >> 8) & 0xFFFFFF;
                        reloc_table[i].r_pcrel     = (w >> 7) & 0x1;
                        reloc_table[i].r_length    = (w >> 5) & 0x3;
                        reloc_table[i].r_extern    = (w >> 4) & 0x1;
                        reloc_table[i].r_pad       = w & 0xF;
                    }
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

/* Get symbols count and data for iteration */
int ndlib_symbols_get_count(void) {
    return g_symbol_count;
}

const char* ndlib_symbols_get_name(int index) {
    if (index < 0 || index >= g_symbol_count) return NULL;
    return g_symbols[index].name;
}

uint32_t ndlib_symbols_get_addr(int index) {
    if (index < 0 || index >= g_symbol_count) return 0;
    return g_symbols[index].addr;
}

uint8_t ndlib_symbols_get_type(int index) {
    if (index < 0 || index >= g_symbol_count) return 0;
    return g_symbols[index].type;
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

/* Lookup symbol by name - returns 0 on success, -1 if not found */
int ndlib_symbols_lookup(const char* name, uint32_t* out_addr, uint8_t* out_type) {
    if (!name) return -1;

    for (int i = 0; i < g_symbol_count; i++) {
        if (strcmp(g_symbols[i].name, name) == 0) {
            if (out_addr) *out_addr = g_symbols[i].addr;
            if (out_type) *out_type = g_symbols[i].type;
            return 0;
        }
    }
    return -1;  /* Symbol not found */
}

/* Get absolute address for symbol (segment base + offset) */
int ndlib_symbols_absolute_addr(const char* name, uint32_t* out_addr) {
    uint32_t offset;
    uint8_t type;

    if (ndlib_symbols_lookup(name, &offset, &type) != 0) {
        return -1;  /* Symbol not found */
    }

    /* Get segment bases from loader */
    uint32_t text_base, text_size, data_base, data_size, bss_base, bss_size;
    ndlib_aout_get_segment_info(&text_base, &text_size, &data_base, &data_size, &bss_base, &bss_size);

    /* Calculate absolute address based on segment type */
    uint8_t seg_type = type & 0x0E;
    uint32_t absolute;

    switch (seg_type) {
        case 0x04:  /* TEXT */
            absolute = text_base + offset;
            break;
        case 0x06:  /* DATA */
            absolute = data_base + offset;
            break;
        case 0x08:  /* BSS */
            absolute = bss_base + offset;
            break;
        default:
            return -1;  /* Invalid segment type */
    }

    if (out_addr) *out_addr = absolute;
    return 0;
}

/* List symbols filtered by segment type */
void ndlib_symbols_list_by_type(uint8_t seg_type) {
    int count = 0;

    /* Count matching symbols */
    for (int i = 0; i < g_symbol_count; i++) {
        uint8_t type = g_symbols[i].type & 0x0E;
        if (seg_type == 0xFF || type == seg_type) {  /* 0xFF = all */
            count++;
        }
    }

    if (count == 0) {
        printf("No symbols found\n");
        return;
    }

    /* Print header */
    const char* seg_name = "ALL";
    if (seg_type == 0x04) seg_name = "TEXT";
    else if (seg_type == 0x06) seg_name = "DATA";
    else if (seg_type == 0x08) seg_name = "BSS";

    printf("=== %s SYMBOLS (%d) ===\n", seg_name, count);
    printf("%-4s %-30s %-12s %-10s\n", "Idx", "Name", "Type", "Address");
    printf("%-4s %-30s %-12s %-10s\n", "---", "----", "----", "-------");

    /* Print symbols */
    int idx = 0;
    for (int i = 0; i < g_symbol_count; i++) {
        uint8_t type = g_symbols[i].type & 0x0E;
        if (seg_type == 0xFF || type == seg_type) {
            /* Format type with EXT flag */
            char type_buf[32];
            const char* base_str = "???";
            switch (type) {
                case 0x00: base_str = "UNDF"; break;
                case 0x02: base_str = "ABS"; break;
                case 0x04: base_str = "TEXT"; break;
                case 0x06: base_str = "DATA"; break;
                case 0x08: base_str = "BSS"; break;
            }
            if (g_symbols[i].type & 0x01) {
                snprintf(type_buf, sizeof(type_buf), "%s|EXT", base_str);
            } else {
                snprintf(type_buf, sizeof(type_buf), "%s", base_str);
            }

            printf("%-4d %-30s %-12s 0x%08X\n", idx++, g_symbols[i].name, type_buf, g_symbols[i].addr);
        }
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════ */
/* SOURCE LINE MAPPING (.map file support) */
/* ═══════════════════════════════════════════════════════ */

/* Comparison function for sorting source lines by address */
static int compare_source_lines(const void* a, const void* b) {
    const SourceLineEntry* sa = (const SourceLineEntry*)a;
    const SourceLineEntry* sb = (const SourceLineEntry*)b;
    if (sa->address < sb->address) return -1;
    if (sa->address > sb->address) return 1;
    return 0;
}

/* Parse address from map file - hex if 0x prefix, otherwise octal */
static uint32_t parse_map_address(const char* str) {
    if (!str) return 0;

    /* Skip whitespace */
    while (*str == ' ' || *str == '\t') str++;

    /* Hex format: 0x... or 0X... */
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return (uint32_t)strtoul(str, NULL, 16);
    }

    /* Default: octal (ND-500 convention) */
    return (uint32_t)strtoul(str, NULL, 8);
}

/* Load .map file: format "filename:line -> address" */
int ndlib_map_load(const char* map_path) {
    if (!map_path) return -1;

    FILE* f = fopen(map_path, "r");
    if (!f) return -1;

    /* First pass: count lines */
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        /* Skip comment lines starting with ; */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '\n' || *p == '\0') continue;

        /* Check for valid format: filename:line -> address */
        if (strchr(line, ':') && strstr(line, "->")) {
            count++;
        }
    }

    if (count == 0) {
        fclose(f);
        return 0;  /* Empty map file, not an error */
    }

    /* Allocate storage */
    g_source_lines = calloc(count, sizeof(SourceLineEntry));
    if (!g_source_lines) {
        fclose(f);
        return -1;
    }

    /* Second pass: parse lines */
    rewind(f);
    int idx = 0;
    while (fgets(line, sizeof(line), f) && idx < count) {
        /* Skip comment lines starting with ; */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '\n' || *p == '\0') continue;

        /* Parse format: "filename:line -> address" */
        char* colon = strchr(line, ':');
        if (!colon) continue;

        char* arrow = strstr(line, "->");
        if (!arrow) continue;

        /* Extract filename */
        *colon = '\0';
        char* filename = line;
        /* Trim leading whitespace */
        while (*filename == ' ' || *filename == '\t') filename++;

        /* Strip path - keep only basename */
        char* basename = filename;
        char* last_slash = strrchr(filename, '/');
        char* last_backslash = strrchr(filename, '\\');
        /* Use whichever path separator is found last */
        if (last_slash && last_backslash) {
            basename = (last_slash > last_backslash) ? last_slash + 1 : last_backslash + 1;
        } else if (last_slash) {
            basename = last_slash + 1;
        } else if (last_backslash) {
            basename = last_backslash + 1;
        }

        /* Extract line number */
        char* line_str = colon + 1;
        int line_num = atoi(line_str);

        /* Extract address */
        char* addr_str = arrow + 2;
        uint32_t addr = parse_map_address(addr_str);

        /* Check for segment marker in comment: "# TEXT" or "# DATA" */
        int is_text = 1;  /* Default to TEXT if no marker */
        char* comment = strchr(addr_str, '#');
        if (comment) {
            /* Skip whitespace after # */
            comment++;
            while (*comment == ' ' || *comment == '\t') comment++;

            /* Check for DATA marker */
            if (strncmp(comment, "DATA", 4) == 0) {
                is_text = 0;
            }
            /* TEXT is already the default, but check explicitly for clarity */
            else if (strncmp(comment, "TEXT", 4) == 0) {
                is_text = 1;
            }
        }

        /* Only store TEXT entries (skip DATA) */
        if (is_text) {
            g_source_lines[idx].source_file = strdup(basename);
            g_source_lines[idx].line_number = line_num;
            g_source_lines[idx].address = addr;
            g_source_lines[idx].is_text = is_text;
            idx++;
        }
    }

    g_source_line_count = idx;
    fclose(f);

    /* Sort by address for fast lookups */
    qsort(g_source_lines, g_source_line_count, sizeof(SourceLineEntry), compare_source_lines);

    return 0;
}

/* Get line number for address */
int ndlib_symbols_line_for_addr(uint32_t addr) {
    for (int i = 0; i < g_source_line_count; i++) {
        if (g_source_lines[i].address == addr) {
            return g_source_lines[i].line_number;
        }
    }
    return -1;
}

/* Get source filename for address */
const char* ndlib_symbols_file_for_addr(uint32_t addr) {
    for (int i = 0; i < g_source_line_count; i++) {
        if (g_source_lines[i].address == addr) {
            return g_source_lines[i].source_file;
        }
    }
    return NULL;
}

/* Get C source mapping for address (returns 1 if found, 0 if not) */
int ndlib_symbols_get_c_mapping(uint32_t addr, const char** out_file, int* out_line) {
    /* Search backwards to find the LAST .c entry for this address
     * (multiple entries can exist for same address; last is most specific) */
    for (int i = g_source_line_count - 1; i >= 0; i--) {
        if (g_source_lines[i].address == addr) {
            const char* ext = strrchr(g_source_lines[i].source_file, '.');
            if (ext && strcmp(ext, ".c") == 0) {
                if (out_file) *out_file = g_source_lines[i].source_file;
                if (out_line) *out_line = g_source_lines[i].line_number;
                return 1;
            }
        }
    }
    return 0;
}

/* Get assembly source mapping for address (returns 1 if found, 0 if not) */
int ndlib_symbols_get_s_mapping(uint32_t addr, const char** out_file, int* out_line) {
    /* Search backwards to find the LAST .s entry for this address
     * (multiple entries can exist for same address; last is most specific) */
    for (int i = g_source_line_count - 1; i >= 0; i--) {
        if (g_source_lines[i].address == addr) {
            const char* ext = strrchr(g_source_lines[i].source_file, '.');
            if (ext && strcmp(ext, ".s") == 0) {
                if (out_file) *out_file = g_source_lines[i].source_file;
                if (out_line) *out_line = g_source_lines[i].line_number;
                return 1;
            }
        }
    }
    return 0;
}

/* Get address for source file:line (for breakpoints) */
int ndlib_symbols_addr_for_line(const char* file, int line, uint32_t* out_addr) {
    if (!file || !out_addr) return -1;

    for (int i = 0; i < g_source_line_count; i++) {
        if (g_source_lines[i].line_number == line &&
            strcmp(g_source_lines[i].source_file, file) == 0) {
            *out_addr = g_source_lines[i].address;
            return 0;
        }
    }
    return -1;  /* Not found */
}

/* Get all addresses for source file:line (for checking breakpoints across multiple mappings) */
int ndlib_symbols_get_addrs_for_line(const char* file, int line, uint32_t* out_addrs, int max_addrs) {
    if (!file || !out_addrs || max_addrs <= 0) return 0;

    int count = 0;
    for (int i = 0; i < g_source_line_count && count < max_addrs; i++) {
        if (g_source_lines[i].line_number == line &&
            strcmp(g_source_lines[i].source_file, file) == 0) {
            out_addrs[count++] = g_source_lines[i].address;
        }
    }
    return count;
}

/* Get first non-zero instruction address (for initial PC in object files) */
uint32_t ndlib_symbols_first_instruction_addr(void) {
    /* Source lines are sorted by address after map load */
    for (int i = 0; i < g_source_line_count; i++) {
        if (g_source_lines[i].address > 0) {
            return g_source_lines[i].address;
        }
    }
    return 0;  /* No instructions found, default to 0 */
}

/* ═══════════════════════════════════════════════════════ */
/* SOURCE FILE CONTENT CACHE */
/* ═══════════════════════════════════════════════════════ */

/* Store source file content in memory */
int ndlib_source_store(const char* filename, const char* content) {
    if (!filename || !content) return -1;
    if (g_source_file_count >= MAX_SOURCE_FILES) return -1;

    /* Check if already loaded */
    for (int i = 0; i < g_source_file_count; i++) {
        if (strcmp(g_source_files[i].filename, filename) == 0) {
            /* Replace existing */
            free(g_source_files[i].content);
            free(g_source_files[i].lines);
            g_source_files[i].content = strdup(content);

            /* Split into lines */
            int line_count = 1;
            for (const char* p = content; *p; p++) {
                if (*p == '\n') line_count++;
            }

            g_source_files[i].lines = calloc(line_count + 1, sizeof(char*));
            g_source_files[i].line_count = line_count;

            char* copy = strdup(content);
            char* line = copy;
            int idx = 0;
            for (char* p = copy; *p; p++) {
                if (*p == '\n') {
                    *p = '\0';
                    g_source_files[i].lines[idx++] = strdup(line);
                    line = p + 1;
                }
            }
            /* Last line */
            if (*line) {
                g_source_files[i].lines[idx] = strdup(line);
            }
            free(copy);

            return 0;
        }
    }

    /* Add new */
    int idx = g_source_file_count++;
    g_source_files[idx].filename = strdup(filename);
    g_source_files[idx].content = strdup(content);

    /* Split into lines */
    int line_count = 1;
    for (const char* p = content; *p; p++) {
        if (*p == '\n') line_count++;
    }

    g_source_files[idx].lines = calloc(line_count + 1, sizeof(char*));
    g_source_files[idx].line_count = line_count;

    char* copy = strdup(content);
    char* line = copy;
    int i = 0;
    for (char* p = copy; *p; p++) {
        if (*p == '\n') {
            *p = '\0';
            g_source_files[idx].lines[i++] = strdup(line);
            line = p + 1;
        }
    }
    /* Last line */
    if (*line) {
        g_source_files[idx].lines[i] = strdup(line);
    }
    free(copy);

    return 0;
}

/* Get specific line from cached source file */
const char* ndlib_source_get_line(const char* filename, int line) {
    if (!filename || line < 1) return NULL;

    for (int i = 0; i < g_source_file_count; i++) {
        if (strcmp(g_source_files[i].filename, filename) == 0) {
            if (line <= g_source_files[i].line_count) {
                return g_source_files[i].lines[line - 1];
            }
            return NULL;
        }
    }
    return NULL;
}

/* Get full source file content */
const char* ndlib_source_get_content(const char* filename) {
    if (!filename) return NULL;

    for (int i = 0; i < g_source_file_count; i++) {
        if (strcmp(g_source_files[i].filename, filename) == 0) {
            return g_source_files[i].content;
        }
    }
    return NULL;
}

/* Get line count for a source file */
int ndlib_source_count_lines(const char* filename) {
    if (!filename) return 0;

    for (int i = 0; i < g_source_file_count; i++) {
        if (strcmp(g_source_files[i].filename, filename) == 0) {
            return g_source_files[i].line_count;
        }
    }
    return 0;
}


