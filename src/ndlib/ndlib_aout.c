#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../machine/machine_protos.h"
#include "ndlib.h"

/* Minimal ND-500 a.out header and symbol structures based on ragge/pcc-nd500 */
struct nd500_exec {
	unsigned int   a_magic;
	unsigned int   a_text;
	unsigned int   a_data;
	unsigned int   a_bss;
	unsigned int   a_syms;
	unsigned int   a_entry;
	unsigned int   a_trsize;
	unsigned int   a_drsize;
};

/* On-disk symbol table entry format (24 bytes with padding) */
struct nd500_nlist {
	unsigned int n_strx;    /* String table index (4 bytes) */
	unsigned int _pad1;     /* Padding to match 64-bit pointer size */
	unsigned char n_type;   /* Symbol type (offset 8) */
	unsigned char n_other;  /* Unused */
	unsigned short n_desc;  /* Descriptor */
	unsigned int n_value;   /* Symbol value (offset 12) */
	unsigned int _pad2;     /* Padding */
	unsigned int _pad3;     /* Padding */
};

#define OMAGIC  0407
#define NMAGIC  0410
#define ZMAGIC  0413
#define IMAGIC  0411

static int bad_magic(unsigned int m) {
	return !(m == OMAGIC || m == NMAGIC || m == ZMAGIC || m == IMAGIC);
}

static const char* loaded_aout_path = NULL;

int ndlib_loadaout_file_ex(Nd500Machine* m, const char* path, unsigned int* out_entry, unsigned int* out_text_size) {
	if (!m || !path) return -1;
	FILE* f = fopen(path, "rb");
	if (!f) return -1;
	struct nd500_exec hdr;
	if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
		fclose(f); return -1;
	}
	if (bad_magic(hdr.a_magic)) { fclose(f); return -1; }
	
	/* Detect file type:
	 * 
	 * IMPORTANT: On ND-500, both object files and executables typically use IMAGIC (0x0109).
	 * Magic number alone is NOT sufficient to distinguish them!
	 * 
	 * Detection logic (tested against nd500-dump and real files):
	 *   1. Has relocations (a_trsize > 0 || a_drsize > 0) → OBJECT FILE
	 *      - Even partially linked files with external deps have relocations
	 *   2. Entry point == 4 → OBJECT FILE
	 *      - Historical placeholder value used by assembler
	 *   3. No relocations AND entry != 4 → EXECUTABLE
	 *      - Fully linked, ready to run
	 * 
	 * Test results:
	 *   add.o:  IMAGIC, relocs=0,  entry=0x4  → OBJECT (placeholder entry)
	 *   math.o: IMAGIC, relocs=24, entry=0x4  → OBJECT (has relocations)
	 *   math:   IMAGIC, relocs=24, entry=0x26 → OBJECT (still has relocations)
	 * 
	 * Note: NMAGIC (0x0108) and ZMAGIC (0x010B) are always executables if encountered.
	 * 
	 * See: /home/ronny/repos/ragge/pcc-nd500/docs/reference/nd500/AOUT_FORMAT.md
	 */
	int has_relocations = (hdr.a_trsize > 0 || hdr.a_drsize > 0);
	int is_placeholder_entry = (hdr.a_entry == 4);
	int is_object = has_relocations || is_placeholder_entry;
	unsigned int entry_point;
	
	if (is_object) {
		/* Object files cannot execute - set entry to 0 */
		entry_point = 0;
		printf("File Type:      OBJECT FILE (needs linking)\n");
		if (has_relocations) {
			printf("Relocations:    text=%u data=%u bytes (not yet resolved)\n",
			       hdr.a_trsize, hdr.a_drsize);
		}
		if (is_placeholder_entry) {
			printf("Note:           Entry point is placeholder (0x4)\n");
		}
		printf("Note:           Setting entry point to 0 (object files cannot execute)\n");
	} else {
		/* Executable: use the actual entry point from header */
		entry_point = hdr.a_entry;
		printf("File Type:      EXECUTABLE (ready to run)\n");
		printf("Entry point:    0x%08X\n", entry_point);
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
	if (out_entry) *out_entry = entry_point;
	if (out_text_size) *out_text_size = hdr.a_text;
	fclose(f);
	
	/* Remember path for symbol listing */
	if (loaded_aout_path) free((void*)loaded_aout_path);
	loaded_aout_path = strdup(path);
	
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


