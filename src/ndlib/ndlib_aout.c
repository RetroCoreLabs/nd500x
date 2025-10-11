#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../machine/machine_protos.h"

/* Minimal ND-500 a.out header based on ragge/pcc-nd500 */
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

#define OMAGIC  0407
#define NMAGIC  0410
#define ZMAGIC  0413
#define IMAGIC  0411

static int bad_magic(unsigned int m) {
	return !(m == OMAGIC || m == NMAGIC || m == ZMAGIC || m == IMAGIC);
}

int ndlib_loadaout_file(Nd500Machine* m, const char* path, unsigned int* out_entry) {
	if (!m || !path) return -1;
	FILE* f = fopen(path, "rb");
	if (!f) return -1;
	struct nd500_exec hdr;
	if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
		fclose(f); return -1;
	}
	if (bad_magic(hdr.a_magic)) { fclose(f); return -1; }
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
	if (out_entry) *out_entry = hdr.a_entry;
	fclose(f);
	return 0;
}


