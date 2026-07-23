#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "../machine/machine_protos.h"
#include "ndlib.h"

/* Minimal ND-500 a.out header and symbol structures based on ragge/pcc-nd500
 *
 * IMPORTANT: This matches the actual binary format from ragge/pcc-nd500 toolchain.
 * Total size: 32 bytes (0x20)
 *
 * Magic field is 2 bytes + 2 bytes padding to maintain 4-byte alignment.
 * All size fields are 4 bytes (uint32_t).
 *
 * See: /home/ronny/repos/ragge/pcc-nd500/src/include/nd500/a.out.h
 * See: /home/ronny/repos/ragge/pcc-nd500/docs/toolchain/OBJECT_VS_EXECUTABLE_DETECTION.md
 */
struct nd500_exec {
	uint16_t   a_magic;     /* Magic number (2 bytes) - offset 0 */
	uint16_t   a_pad;       /* Padding (2 bytes) - offset 2 */
	uint32_t   a_text;      /* Size of text segment (4 bytes) - offset 4 */
	uint32_t   a_data;      /* Size of initialized data (4 bytes) - offset 8 */
	uint32_t   a_bss;       /* Size of uninitialized data (4 bytes) - offset 12 */
	uint32_t   a_syms;      /* Size of symbol table (4 bytes) - offset 16 */
	uint32_t   a_entry;     /* Entry point (4 bytes) - offset 20 */
	uint32_t   a_trsize;    /* Text relocation size (4 bytes) - offset 24 */
	uint32_t   a_drsize;    /* Data relocation size (4 bytes) - offset 28 */
} __attribute__((packed));

/* On-disk symbol table entry - 12 bytes, matches file format exactly */
struct nd500_nlist {
	int32_t n_strx;         /* String table index (4 bytes) */
	uint8_t n_type;         /* Symbol type (1 byte) */
	uint8_t n_other;        /* Unused (1 byte) */
	int16_t n_desc;         /* Descriptor (2 bytes) */
	uint32_t n_value;       /* Symbol value (4 bytes) */
} __attribute__((packed));

#define NLIST_SIZE 12  /* On-disk symbol table entry size */

#define OMAGIC  0407
#define NMAGIC  0410
#define ZMAGIC  0413
#define IMAGIC  0411

static int bad_magic(unsigned int m) {
	return !(m == OMAGIC || m == NMAGIC || m == ZMAGIC || m == IMAGIC);
}

static const char* loaded_aout_path = NULL;

/* Segment layout tracking for absolute address calculation */
static uint32_t g_text_base = 0;
static uint32_t g_text_size = 0;
static uint32_t g_data_base = 0;
static uint32_t g_data_size = 0;
static uint32_t g_bss_base = 0;
static uint32_t g_bss_size = 0;

int ndlib_loadaout_file_ex(Nd500Machine* m, const char* path, unsigned int* out_entry, unsigned int* out_text_size) {
	if (!m || !path) return -1;
	FILE* f = fopen(path, "rb");
	if (!f) return -1;
	struct nd500_exec hdr;
	if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
		fclose(f); return -1;
	}
	if (bad_magic(hdr.a_magic)) { fclose(f); return -1; }
	
	/* Detect file type correctly (object vs executable):
	 * Object file if it has unresolved externals (UNDF|EXT) OR relocations.
	 * Executable if no unresolved externals and no relocations.
	 * Magic number alone is insufficient on ND-500.
	 */
	int has_relocations = (hdr.a_trsize > 0 || hdr.a_drsize > 0);
	int has_unresolved = 0;
	if (hdr.a_syms > 0) {
		unsigned int sym_off = (unsigned int)sizeof(hdr) + hdr.a_text + hdr.a_data + hdr.a_trsize + hdr.a_drsize;
		if (fseek(f, (long)sym_off, SEEK_SET) == 0) {
			int nsyms = (int)(hdr.a_syms / (unsigned int)sizeof(struct nd500_nlist));
			for (int i = 0; i < nsyms; i++) {
				struct nd500_nlist sym;
				if (fread(&sym, 1, sizeof(sym), f) != sizeof(sym)) break;
				/* Skip symbols with no name (n_strx == 0) - these include STAB debug symbols */
				if (sym.n_strx == 0) continue;
				unsigned char base_type = (unsigned char)(sym.n_type & 0x0E); /* N_TYPE mask */
				int ext = (sym.n_type & 0x01) ? 1 : 0; /* N_EXT */
				if (base_type == 0x00 && ext) { has_unresolved = 1; break; } /* N_UNDF|EXT with actual name */
			}
		}
		/* Position will be reset before actual segment reads */
	}
	int is_object = (has_relocations || has_unresolved);
	unsigned int entry_point;
	
	if (is_object) {
		/* Object files cannot execute - set entry to 0 */
		entry_point = 0;
		printf("File Type:      OBJECT FILE (needs linking)\n");
		if (has_relocations) {
			printf("Relocations:    text=%u data=%u bytes (not yet resolved)\n",
			       hdr.a_trsize, hdr.a_drsize);
		}
		if (has_unresolved) {
			printf("Symbols:        unresolved externals present (UNDF|EXT)\n");
		}
		printf("Note:           Setting entry point to 0 (object files cannot execute)\n");
	} else {
		/* Executable: use the actual entry point from header */
		entry_point = hdr.a_entry;
		printf("File Type:      EXECUTABLE (ready to run)\n");
		printf("Entry point:    0x%08X\n", entry_point);
		printf("[DEBUG ndlib_loadaout_file_ex] hdr.a_entry=0x%x, setting entry_point=0x%x\n", hdr.a_entry, entry_point);
	}
	
	/* Text immediately after header, per nd500 a.out */
	unsigned int text_off = sizeof(hdr);
	unsigned int data_off = text_off + hdr.a_text;
	/* Load text */
	if (hdr.a_text) {
		if (fseek(f, (long)text_off, SEEK_SET) != 0) { fclose(f); return -1; }
		for (unsigned int i = 0; i < hdr.a_text; ++i) {
			int c = fgetc(f);
			if (c == EOF) { fclose(f); return -1; }
			nd500_bus_write8(m, i, (unsigned char)c);
		}
	}
	/* Load data after text */
	if (hdr.a_data) {
		if (fseek(f, (long)data_off, SEEK_SET) != 0) { fclose(f); return -1; }
		for (unsigned int i = 0; i < hdr.a_data; ++i) {
			int c = fgetc(f);
			if (c == EOF) { fclose(f); return -1; }
			nd500_bus_write8(m, hdr.a_text + i, (unsigned char)c);
		}
	}
	/* Zero BSS */
	for (unsigned int i = 0; i < hdr.a_bss; ++i) {
		nd500_bus_write8(m, hdr.a_text + hdr.a_data + i, 0);
	}
	if (out_entry) {
		*out_entry = entry_point;
		printf("[DEBUG ndlib_loadaout_file_ex] Setting *out_entry = 0x%x\n", entry_point);
	}
	if (out_text_size) *out_text_size = hdr.a_text;
	fclose(f);

	/* Remember path for symbol listing */
	if (loaded_aout_path) free((void*)loaded_aout_path);
	loaded_aout_path = strdup(path);

	/* Track segment layout for absolute address calculation */
	g_text_base = 0;
	g_text_size = hdr.a_text;
	g_data_base = hdr.a_text;
	g_data_size = hdr.a_data;
	g_bss_base = hdr.a_text + hdr.a_data;
	g_bss_size = hdr.a_bss;
	
	return 0;
}

int ndlib_loadaout_file(Nd500Machine* m, const char* path, unsigned int* out_entry) {
	return ndlib_loadaout_file_ex(m, path, out_entry, NULL);
}

const char* ndlib_aout_get_loaded_path(void) {
    return loaded_aout_path;
}

int ndlib_aout_dump_metadata(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    struct nd500_exec hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { fclose(f); return -1; }
    if (bad_magic(hdr.a_magic)) { fclose(f); return -1; }
    
    /* Detect file type by relocations and entry point */
    int has_relocations = (hdr.a_trsize > 0 || hdr.a_drsize > 0);
    int is_placeholder_entry = (hdr.a_entry == 4);
    int is_object = has_relocations || is_placeholder_entry;
    const char* file_type;
    
    if (is_object) {
        file_type = "OBJECT FILE (needs linking)";
    } else {
        file_type = "EXECUTABLE";
    }
    
    /* Print fancy header like nd500-dis */
    printf("; %s\n", "═══════════════════════════════════════════════════════════════");
    printf("; ND-500 Disassembly\n");
    printf("; %s\n", "═══════════════════════════════════════════════════════════════");
    printf("; File: %s\n;\n", path);
    printf("; File Type:    %s\n", file_type);
    if (is_object && (hdr.a_trsize > 0 || hdr.a_drsize > 0)) {
        printf("; Relocations:  text=%u data=%u bytes (not yet resolved)\n", 
               hdr.a_trsize, hdr.a_drsize);
    }
    printf(";\n");
    
    /* List unresolved externals if any */
    ndlib_symbols_list_unresolved();
    
    printf(";\n; Text size:    %u bytes (0x%X)\n", hdr.a_text, hdr.a_text);
    printf("; %s\n;\n", "═══════════════════════════════════════════════════════════════");
    fclose(f);
    return 0;
}

int ndlib_aout_dump_metadata_old(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    struct nd500_exec hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { fclose(f); return -1; }
    if (bad_magic(hdr.a_magic)) { fclose(f); return -1; }
    
    /* Detect file type by relocations and entry point */
    int has_relocations = (hdr.a_trsize > 0 || hdr.a_drsize > 0);
    int is_placeholder_entry = (hdr.a_entry == 4);
    int is_object = has_relocations || is_placeholder_entry;
    const char* file_type;
    
    if (is_object) {
        file_type = "OBJECT FILE (needs linking)";
    } else {
        file_type = "EXECUTABLE";
    }
    
    printf("=== A.OUT HEADER ===\n");
    printf("File:           %s\n", path);
    printf("File Type:      %s\n", file_type);
    if (is_object && (hdr.a_trsize > 0 || hdr.a_drsize > 0)) {
        printf("Relocations:    text=%u data=%u bytes (not yet resolved)\n", 
               hdr.a_trsize, hdr.a_drsize);
    }
    printf("\nMagic number:   0x%04X\n", hdr.a_magic);
    printf("Text size:      %u bytes (0x%x)\n", hdr.a_text, hdr.a_text);
    printf("Data size:      %u bytes (0x%x)\n", hdr.a_data, hdr.a_data);
    printf("BSS size:       %u bytes (0x%x)\n", hdr.a_bss, hdr.a_bss);
    printf("Symbol table:   %u bytes (0x%x)\n", hdr.a_syms, hdr.a_syms);
    printf("Entry point:    0x%x\n", hdr.a_entry);
    printf("Text reloc:     %u bytes (0x%x)\n", hdr.a_trsize, hdr.a_trsize);
    printf("Data reloc:     %u bytes (0x%x)\n", hdr.a_drsize, hdr.a_drsize);
    printf("\n=== FILE LAYOUT ===\n");
    unsigned int txt = (unsigned int)sizeof(hdr);
    printf("Header:         0x0000 - 0x%04X (%zu bytes)\n", (unsigned)sizeof(hdr), sizeof(hdr));
    printf("Text segment:   0x%04X - 0x%04X (%u bytes)\n", txt, txt + hdr.a_text, hdr.a_text);
    printf("Data segment:   0x%04X - 0x%04X (%u bytes)\n", txt + hdr.a_text, txt + hdr.a_text + hdr.a_data, hdr.a_data);
    printf("Text reloc:     0x%04X - 0x%04X (%u bytes)\n", txt + hdr.a_text + hdr.a_data, txt + hdr.a_text + hdr.a_data + hdr.a_trsize, hdr.a_trsize);
    printf("Data reloc:     0x%04X - 0x%04X (%u bytes)\n", txt + hdr.a_text + hdr.a_data + hdr.a_trsize, txt + hdr.a_text + hdr.a_data + hdr.a_trsize + hdr.a_drsize, hdr.a_drsize);
    unsigned int sym = txt + hdr.a_text + hdr.a_data + hdr.a_trsize + hdr.a_drsize;
    printf("Symbol table:   0x%04X - 0x%04X (%u bytes)\n", sym, sym + hdr.a_syms, hdr.a_syms);
    printf("String table:   0x%04X - (size at offset)\n", sym + hdr.a_syms);
    printf("\n");
    fclose(f);
    
    /* Remember path for symbol listing */
    if (loaded_aout_path) free((void*)loaded_aout_path);
    loaded_aout_path = strdup(path);
    
    return 0;
}

void ndlib_aout_dump_symbols(const char* path) {
    if (!path) return;
    FILE* f = fopen(path, "rb");
    if (!f) return;
    
    struct nd500_exec hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) { fclose(f); return; }
    if (bad_magic(hdr.a_magic)) { fclose(f); return; }
    
    if (hdr.a_syms == 0) {
        printf("No symbols\n");
        fclose(f);
        return;
    }
    
    /* Calculate offsets */
    unsigned int sym_off = sizeof(hdr) + hdr.a_text + hdr.a_data + hdr.a_trsize + hdr.a_drsize;
    unsigned int str_off = sym_off + hdr.a_syms;
    int nsyms = hdr.a_syms / sizeof(struct nd500_nlist);
    
    /* Read symbols */
    struct nd500_nlist* symbols = malloc(hdr.a_syms);
    if (!symbols) { fclose(f); return; }
    fseek(f, sym_off, SEEK_SET);
    if (fread(symbols, 1, hdr.a_syms, f) != hdr.a_syms) {
        free(symbols); fclose(f); return;
    }
    
    /* Read string table size */
    unsigned int strsize = 0;
    fseek(f, str_off, SEEK_SET);
    if (fread(&strsize, 1, 4, f) != 4) {
        free(symbols); fclose(f); return;
    }
    
    /* Read string table */
    char* strings = malloc(strsize);
    if (!strings) { free(symbols); fclose(f); return; }
    fseek(f, str_off, SEEK_SET);
    if (fread(strings, 1, strsize, f) != strsize) {
        free(symbols); free(strings); fclose(f); return;
    }
    
    printf("=== SYMBOL TABLE ===\n");
    
    /* Count actual symbols (skip aux entries with n_strx == 0) */
    int actual_syms = 0;
    for (int i = 0; i < nsyms; i++) {
        if (symbols[i].n_strx > 0) actual_syms++;
    }
    
    printf("Total symbols: %d\n", actual_syms);
    printf("String table size: %u bytes\n\n", strsize);
    printf("%-4s %-20s %-12s %-10s %s\n", "Idx", "Name", "Type", "Value", "Desc");
    printf("%-4s %-20s %-12s %-10s %s\n", "---", "----", "----", "-----", "----");
    
    int idx = 0;
    for (int i = 0; i < nsyms; i++) {
        /* Skip aux/debug entries without names */
        if (symbols[i].n_strx == 0) continue;
        
        const char* name = (symbols[i].n_strx < strsize) 
                          ? (strings + symbols[i].n_strx) : "(invalid)";
        
        /* Format type with EXT flag */
        char type_buf[32];
        unsigned char base_type = symbols[i].n_type & 0x0E;
        const char* base_str = "???";
        switch (base_type) {
            case 0x00: base_str = "UNDF"; break;
            case 0x02: base_str = "ABS"; break;
            case 0x04: base_str = "TEXT"; break;
            case 0x06: base_str = "DATA"; break;
            case 0x08: base_str = "BSS"; break;
        }
        if (symbols[i].n_type & 0x01) {
            snprintf(type_buf, sizeof(type_buf), "%s|EXT", base_str);
        } else {
            snprintf(type_buf, sizeof(type_buf), "%s", base_str);
        }
        
        printf("%-4d %-20s %-12s 0x%08X %d\n", idx++, name, type_buf, symbols[i].n_value, symbols[i].n_desc);
    }
    
    free(symbols);
    free(strings);
    fclose(f);
}

/* Segment info accessors for CLI commands */
void ndlib_aout_get_segment_info(uint32_t* text_base, uint32_t* text_size,
                                   uint32_t* data_base, uint32_t* data_size,
                                   uint32_t* bss_base, uint32_t* bss_size) {
    if (text_base) *text_base = g_text_base;
    if (text_size) *text_size = g_text_size;
    if (data_base) *data_base = g_data_base;
    if (data_size) *data_size = g_data_size;
    if (bss_base) *bss_base = g_bss_base;
    if (bss_size) *bss_size = g_bss_size;
}

/* Physical base where the flat a.out loader placed the DATA section (= a_text).
 * The ND-500 has separate I-space (program/text) and D-space (data): a data
 * access to a segment-0 virtual address V targets physical (data_base + V), while
 * a program fetch of V targets physical V (text). The MMU uses this to keep the
 * two spaces from aliasing. Returns 0 when no a.out is loaded. */
uint32_t ndlib_aout_get_data_base(void) {
    return g_data_base;
}

/* Helper: Read entire file into dynamically allocated string */
static char* read_file_contents(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    char* content = (char*)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(content, 1, (size_t)size, f);
    content[read] = '\0';
    fclose(f);

    return content;
}

/* Unified loading function to eliminate code duplication across CLI/DAP/WASM */
int ndlib_load_aout_with_debug(Nd500Machine* m, const char* aout_path,
                                int auto_map, uint32_t* out_entry, uint32_t* out_pc) {
    if (!m || !aout_path) return -1;

    /* Step 1: Load a.out file */
    uint32_t entry = 0;
    int rc = ndlib_loadaout_file_ex(m, aout_path, &entry, NULL);
    if (rc != 0) return -1;

    /* Step 2: Load symbols from a.out */
    ndlib_symbols_load(aout_path);

    /* Step 3: Auto-load .map and .s files if requested */
    if (auto_map) {
        char alt_path[512];
        strncpy(alt_path, aout_path, sizeof(alt_path) - 1);
        alt_path[sizeof(alt_path) - 1] = '\0';

        char* ext = strrchr(alt_path, '.');
        if (ext && (strcmp(ext, ".o") == 0 || strcmp(ext, ".out") == 0)) {
            /* Try to load .map file */
            strcpy(ext, ".map");
            ndlib_map_load(alt_path);  /* Ignore errors - map file is optional */

            /* Try to load .s source file */
            strcpy(ext, ".s");
            char* source_content = read_file_contents(alt_path);
            if (source_content) {
                /* Extract just the filename (no path) for storage */
                const char* basename = strrchr(alt_path, '/');
                basename = basename ? basename + 1 : alt_path;
                ndlib_source_store(basename, source_content);
                free(source_content);
            }

            /* Try to load .c source file */
            strcpy(ext, ".c");
            char* c_content = read_file_contents(alt_path);
            if (c_content) {
                /* Extract just the filename (no path) for storage */
                const char* basename = strrchr(alt_path, '/');
                basename = basename ? basename + 1 : alt_path;
                ndlib_source_store(basename, c_content);
                free(c_content);
            }
        }
    }

    /* Step 4: Set PC correctly for object files vs executables */
    uint32_t pc = 0;
    if (entry == 0) {
        /* Object file (no entry point) - use first instruction from map */
        uint32_t first_instr = ndlib_symbols_first_instruction_addr();
        pc = (first_instr > 0) ? first_instr : 0;
        printf("[ndlib_load_aout_with_debug] Object file: entry=0, using first_instr=0x%x as PC\n", pc);
    } else {
        /* Executable - use entry point (even if it's 4) */
        pc = entry;
        printf("[ndlib_load_aout_with_debug] Executable: entry=0x%x, setting PC=0x%x\n", entry, pc);
    }

    /* Set PC on machine's CPU */
    if (m->cpu) {
        printf("[ndlib_load_aout_with_debug] Setting m->cpu->PC = 0x%x (was 0x%x)\n", pc, m->cpu->PC);
        m->cpu->PC = pc;
        printf("[ndlib_load_aout_with_debug] After set: m->cpu->PC = 0x%x\n", m->cpu->PC);
    } else {
        printf("[ndlib_load_aout_with_debug] WARNING: m->cpu is NULL, cannot set PC!\n");
    }

    /* Return values */
    if (out_entry) *out_entry = entry;
    if (out_pc) *out_pc = pc;

    return 0;
}


