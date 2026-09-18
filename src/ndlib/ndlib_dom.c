/*
 * ndlib_dom.c - DOM/SEG file loader
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Loads DOM (Domain) or SEG (Segment) files including header and segment data.
 *
 * Reference: ND-860289-2-EN ND Linker User Guide and Reference Manual
 */

#include "ndlib.h"
#include "nd500_dom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*============================================================================
 * Loaded file structure - holds header and all segment data
 *============================================================================*/
typedef struct {
    nd500_header_t header;
    int is_dom;                         /* 1=DOM, 0=SEG */
    uint8_t *segment_data[32];          /* Loaded segment program data */
    uint32_t segment_size[32];          /* Size of each program segment */
    uint32_t segment_load_addr[32];     /* Load address for each segment */
    uint8_t *data_data[32];             /* Loaded segment data sections */
    uint32_t data_size[32];             /* Size of each data section */
    uint32_t data_load_addr[32];        /* Load address for data sections */
    char filepath[256];
    FILE* file;
    int is_loaded;
} nd500_file_t;

static nd500_file_t g_dom_file = {0};

/*============================================================================
 * Offset constants for reading from raw header
 *============================================================================*/
#define OFF_LINKLOCK    0x00
#define OFF_VERSION     0x04
#define OFF_REVISION    0x05
#define OFF_FLAGS       0x06
#define OFF_MACHINE     0x07
#define OFF_OS_ID       0x08

/* Common part offsets */
#define OFF_DEB_LB      0xC8
#define OFF_DEB_SZ      0xCC
#define OFF_LINK_LB     0xD0
#define OFF_LINK_SZ     0xD4
#define OFF_STADDR      0xD8
#define OFF_RESTADDR    0xDC
#define OFF_THA         0xE0

/* DOM segment table */
#define OFF_DOM_SEGTAB  0x254
#define SEG_DESC_SIZE   56      /* 28 bytes program + 28 bytes data */
#define SEG_PART_SIZE   28

/* Segment part field offsets (within a 28-byte segment part) */
#define SEG_OFF_LB      0
#define SEG_OFF_SZ      4
#define SEG_OFF_ATT     8
#define SEG_OFF_FLA     12
#define SEG_OFF_FUA     16
#define SEG_OFF_AFA     20
#define SEG_OFF_MINP    24
#define SEG_OFF_MAXP    26

/*============================================================================
 * Attribute checking
 *============================================================================*/

/* Check if segment is linked (ATT bit 14) */
static int is_linked_segment(const uint8_t* att_ptr) {
    uint32_t att = nd500_read32(att_ptr);
    return (att & ND500_SEG_ATT_LINKED_SEGMENT) != 0;
}

/* Check if segment slot is used (ATT bit 13) */
static int is_segment_used(const uint8_t* att_ptr) {
    uint32_t att = nd500_read32(att_ptr);
    return (att & ND500_SEG_ATT_SEGMENT_USED) != 0;
}

/* Should we load this segment from the file? */
static int should_load_segment(const uint8_t* att_ptr) {
    /* Load if: NOT linked AND marked as used */
    return !is_linked_segment(att_ptr) && is_segment_used(att_ptr);
}

/*============================================================================
 * Bounds validation
 *============================================================================*/
static int validate_segment_bounds(uint32_t lb, uint32_t sz, long file_size) {
    if (sz == 0) return 0;              /* Empty, skip */
    if (sz > (uint32_t)file_size) return -1;
    if (lb > (uint32_t)file_size) return -1;
    if (lb + sz > (uint32_t)file_size) return -1;
    if (sz > 0x10000000) return -1;     /* > 256MB - likely garbage */
    return 1;                           /* Valid */
}

/*============================================================================
 * Calculate body start (where segment data begins in file)
 *============================================================================*/
static uint32_t get_body_start(const uint8_t* raw) {
    uint32_t link_lb = nd500_read32(&raw[OFF_LINK_LB]);
    uint32_t link_sz = nd500_read32(&raw[OFF_LINK_SZ]);
    return link_lb + link_sz;
}

/*============================================================================
 * Load DOM file segments (32 segment descriptors at 0x254)
 *============================================================================*/
static int load_dom_segments(void) {
    FILE* f = g_dom_file.file;
    const uint8_t* raw = g_dom_file.header.raw;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);

    /* Iterate all 32 segments */
    for (int i = 0; i < ND500_MAX_SEGMENTS; i++) {
        /* Calculate offset to this segment descriptor */
        uint32_t seg_offset = OFF_DOM_SEGTAB + (i * SEG_DESC_SIZE);
        const uint8_t* prog_ptr = &raw[seg_offset];
        const uint8_t* data_ptr = &raw[seg_offset + SEG_PART_SIZE];

        /* Track loaded ranges for overlap detection */
        uint32_t prog_lb = 0, prog_sz = 0, data_lb = 0, data_sz = 0;
        int prog_loaded = 0, data_loaded = 0;

        /* Check PROGRAM part */
        const uint8_t* prog_att = &prog_ptr[SEG_OFF_ATT];
        if (should_load_segment(prog_att)) {
            uint32_t lb = nd500_read32(&prog_ptr[SEG_OFF_LB]);
            uint32_t sz = nd500_read32(&prog_ptr[SEG_OFF_SZ]);

            int valid = validate_segment_bounds(lb, sz, file_size);
            if (valid == 1) {
                g_dom_file.segment_data[i] = malloc(sz);
                if (g_dom_file.segment_data[i]) {
                    fseek(f, lb, SEEK_SET);
                    size_t read = fread(g_dom_file.segment_data[i], 1, sz, f);
                    if (read == sz) {
                        g_dom_file.segment_size[i] = sz;
                        /* FLA (Fixed Lower Address) is the load address */
                        g_dom_file.segment_load_addr[i] = nd500_read32(&prog_ptr[SEG_OFF_FLA]);
                        /* Track for overlap detection */
                        prog_lb = lb; prog_sz = sz; prog_loaded = 1;
                        nd500_log("DOM Load: Seg[%d] PROG: file=0x%08X..0x%08X size=%u load_addr=0x%08X",
                                  i, lb, lb + sz - 1, sz, g_dom_file.segment_load_addr[i]);
                    } else {
                        free(g_dom_file.segment_data[i]);
                        g_dom_file.segment_data[i] = NULL;
                    }
                }
            }
        }

        /* Check DATA part */
        const uint8_t* data_att = &data_ptr[SEG_OFF_ATT];
        if (should_load_segment(data_att)) {
            uint32_t lb = nd500_read32(&data_ptr[SEG_OFF_LB]);
            uint32_t sz = nd500_read32(&data_ptr[SEG_OFF_SZ]);

            int valid = validate_segment_bounds(lb, sz, file_size);
            if (valid == 1) {
                g_dom_file.data_data[i] = malloc(sz);
                if (g_dom_file.data_data[i]) {
                    fseek(f, lb, SEEK_SET);
                    size_t read = fread(g_dom_file.data_data[i], 1, sz, f);
                    if (read == sz) {
                        g_dom_file.data_size[i] = sz;
                        g_dom_file.data_load_addr[i] = nd500_read32(&data_ptr[SEG_OFF_FLA]);
                        /* Track for overlap detection */
                        data_lb = lb; data_sz = sz; data_loaded = 1;
                        nd500_log("DOM Load: Seg[%d] DATA: file=0x%08X..0x%08X size=%u load_addr=0x%08X",
                                  i, lb, lb + sz - 1, sz, g_dom_file.data_load_addr[i]);
                    } else {
                        free(g_dom_file.data_data[i]);
                        g_dom_file.data_data[i] = NULL;
                    }
                }
            }
        }

        /* Check for overlap between PROG and DATA file regions */
        if (prog_loaded && data_loaded) {
            uint32_t prog_end = prog_lb + prog_sz;
            uint32_t data_end = data_lb + data_sz;
            if (prog_lb < data_end && data_lb < prog_end) {
                nd500_log("DOM Load: WARNING Seg[%d] OVERLAP! PROG=[0x%X..0x%X] DATA=[0x%X..0x%X]",
                          i, prog_lb, prog_end - 1, data_lb, data_end - 1);
            }
        }
    }

    return 0;
}

/*============================================================================
 * Load SEG file segments (single segment in SEG-specific area)
 *============================================================================*/

/* SEG-specific offsets (per ND-860289-2-EN page 250)
 * Reference: C# SEGHeader.cs uses progOffset: 0x14, dataOffset: 0x30 */
#define OFF_SEG_PROGRAM 0x14    /* Program part at 0024 octal = 0x14 */
#define OFF_SEG_DATA    0x30    /* Data part at 0060 octal = 0x30 */

static int load_seg_segments(void) {
    FILE* f = g_dom_file.file;
    const uint8_t* raw = g_dom_file.header.raw;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);

    uint32_t body_start = get_body_start(raw);

    /* Program part */
    const uint8_t* prog_ptr = &raw[OFF_SEG_PROGRAM];
    const uint8_t* prog_att = &prog_ptr[SEG_OFF_ATT];

    if (should_load_segment(prog_att)) {
        uint32_t lb = nd500_read32(&prog_ptr[SEG_OFF_LB]);
        uint32_t sz = nd500_read32(&prog_ptr[SEG_OFF_SZ]);

        /* For SEG files, LB might be relative to body_start if < header size */
        if (lb < ND500_HEADER_SIZE && sz > 0) {
            lb = body_start;
        }

        int valid = validate_segment_bounds(lb, sz, file_size);
        if (valid == 1) {
            g_dom_file.segment_data[0] = malloc(sz);
            if (g_dom_file.segment_data[0]) {
                fseek(f, lb, SEEK_SET);
                size_t read = fread(g_dom_file.segment_data[0], 1, sz, f);
                if (read == sz) {
                    g_dom_file.segment_size[0] = sz;
                    g_dom_file.segment_load_addr[0] = nd500_read32(&prog_ptr[SEG_OFF_FLA]);
                } else {
                    free(g_dom_file.segment_data[0]);
                    g_dom_file.segment_data[0] = NULL;
                }
            }
        }
    }

    /* Data part */
    const uint8_t* data_ptr = &raw[OFF_SEG_DATA];
    const uint8_t* data_att = &data_ptr[SEG_OFF_ATT];

    if (should_load_segment(data_att)) {
        uint32_t lb = nd500_read32(&data_ptr[SEG_OFF_LB]);
        uint32_t sz = nd500_read32(&data_ptr[SEG_OFF_SZ]);

        int valid = validate_segment_bounds(lb, sz, file_size);
        if (valid == 1) {
            g_dom_file.data_data[0] = malloc(sz);
            if (g_dom_file.data_data[0]) {
                fseek(f, lb, SEEK_SET);
                size_t read = fread(g_dom_file.data_data[0], 1, sz, f);
                if (read == sz) {
                    g_dom_file.data_size[0] = sz;
                    g_dom_file.data_load_addr[0] = nd500_read32(&data_ptr[SEG_OFF_FLA]);
                } else {
                    free(g_dom_file.data_data[0]);
                    g_dom_file.data_data[0] = NULL;
                }
            }
        }
    }

    return 0;
}

/*============================================================================
 * Free loaded segment data
 *============================================================================*/
static void free_segment_data(void) {
    for (int i = 0; i < 32; i++) {
        if (g_dom_file.segment_data[i]) {
            free(g_dom_file.segment_data[i]);
            g_dom_file.segment_data[i] = NULL;
        }
        if (g_dom_file.data_data[i]) {
            free(g_dom_file.data_data[i]);
            g_dom_file.data_data[i] = NULL;
        }
        g_dom_file.segment_size[i] = 0;
        g_dom_file.data_size[i] = 0;
    }
}

/*============================================================================
 * Public API
 *============================================================================*/

int ndlib_load_dom_header(const char* path) {
    /* Close any previously open file and free data */
    ndlib_close_dom();

    /* Open DOM file */
    FILE* f = fopen(path, "rb");
    if (!f) {
        return -1;  /* File not found */
    }

    /* Read 4096-byte header */
    size_t bytes_read = fread(g_dom_file.header.raw, 1, ND500_HEADER_SIZE, f);
    if (bytes_read != ND500_HEADER_SIZE) {
        fclose(f);
        return -2;  /* Header read failed */
    }

    /* Detect file type */
    g_dom_file.is_dom = nd500_is_dom_file(&g_dom_file.header);

    /* Keep file open for segment loading */
    g_dom_file.file = f;
    strncpy(g_dom_file.filepath, path, sizeof(g_dom_file.filepath) - 1);
    g_dom_file.filepath[sizeof(g_dom_file.filepath) - 1] = '\0';
    g_dom_file.is_loaded = 1;

    return 0;
}

int ndlib_load_dom_segments(void) {
    if (!g_dom_file.is_loaded || !g_dom_file.file) {
        return -1;
    }

    if (g_dom_file.is_dom) {
        return load_dom_segments();
    } else {
        return load_seg_segments();
    }
}

void ndlib_close_dom(void) {
    free_segment_data();
    if (g_dom_file.file) {
        fclose(g_dom_file.file);
    }
    memset(&g_dom_file, 0, sizeof(g_dom_file));
}

const nd500_header_t* ndlib_get_dom_header(void) {
    return g_dom_file.is_loaded ? &g_dom_file.header : NULL;
}

FILE* ndlib_get_dom_file(void) {
    return g_dom_file.file;
}

int ndlib_dom_is_loaded(void) {
    return g_dom_file.is_loaded;
}

int ndlib_dom_is_dom_file(void) {
    return g_dom_file.is_loaded ? g_dom_file.is_dom : 0;
}

const char* ndlib_get_dom_filepath(void) {
    return g_dom_file.is_loaded ? g_dom_file.filepath : NULL;
}

/*============================================================================
 * Segment access functions
 *============================================================================*/

int ndlib_dom_get_segment_count(void) {
    if (!g_dom_file.is_loaded) return 0;

    int count = 0;
    for (int i = 0; i < 32; i++) {
        if (g_dom_file.segment_data[i] || g_dom_file.data_data[i]) {
            count++;
        }
    }
    return count;
}

const uint8_t* ndlib_dom_get_segment_data(int index, uint32_t* out_size, uint32_t* out_load_addr) {
    if (!g_dom_file.is_loaded || index < 0 || index >= 32) {
        if (out_size) *out_size = 0;
        if (out_load_addr) *out_load_addr = 0;
        return NULL;
    }

    if (out_size) *out_size = g_dom_file.segment_size[index];
    if (out_load_addr) *out_load_addr = g_dom_file.segment_load_addr[index];
    return g_dom_file.segment_data[index];
}

const uint8_t* ndlib_dom_get_data_section(int index, uint32_t* out_size, uint32_t* out_load_addr) {
    if (!g_dom_file.is_loaded || index < 0 || index >= 32) {
        if (out_size) *out_size = 0;
        if (out_load_addr) *out_load_addr = 0;
        return NULL;
    }

    if (out_size) *out_size = g_dom_file.data_size[index];
    if (out_load_addr) *out_load_addr = g_dom_file.data_load_addr[index];
    return g_dom_file.data_data[index];
}

/*============================================================================
 * Segment info for display
 *============================================================================*/

int ndlib_dom_get_segment_info(int index, uint32_t* prog_size, uint32_t* prog_addr,
                               uint32_t* data_size, uint32_t* data_addr,
                               int* is_linked, int* is_used) {
    if (!g_dom_file.is_loaded || index < 0 || index >= 32) {
        return -1;
    }

    const uint8_t* raw = g_dom_file.header.raw;

    if (g_dom_file.is_dom) {
        /* DOM: segment descriptor at 0x254 + index * 56 */
        uint32_t seg_offset = OFF_DOM_SEGTAB + (index * SEG_DESC_SIZE);
        const uint8_t* prog_ptr = &raw[seg_offset];
        const uint8_t* data_ptr = &raw[seg_offset + SEG_PART_SIZE];

        if (prog_size) *prog_size = nd500_read32(&prog_ptr[SEG_OFF_SZ]);
        if (prog_addr) *prog_addr = nd500_read32(&prog_ptr[SEG_OFF_FLA]);
        if (data_size) *data_size = nd500_read32(&data_ptr[SEG_OFF_SZ]);
        if (data_addr) *data_addr = nd500_read32(&data_ptr[SEG_OFF_FLA]);
        if (is_linked) *is_linked = is_linked_segment(&prog_ptr[SEG_OFF_ATT]) ||
                                    is_linked_segment(&data_ptr[SEG_OFF_ATT]);
        if (is_used) *is_used = is_segment_used(&prog_ptr[SEG_OFF_ATT]) ||
                                is_segment_used(&data_ptr[SEG_OFF_ATT]);
    } else {
        /* SEG: only index 0 is valid */
        if (index != 0) return -1;

        const uint8_t* prog_ptr = &raw[OFF_SEG_PROGRAM];
        const uint8_t* data_ptr = &raw[OFF_SEG_DATA];

        if (prog_size) *prog_size = nd500_read32(&prog_ptr[SEG_OFF_SZ]);
        if (prog_addr) *prog_addr = nd500_read32(&prog_ptr[SEG_OFF_FLA]);
        if (data_size) *data_size = nd500_read32(&data_ptr[SEG_OFF_SZ]);
        if (data_addr) *data_addr = nd500_read32(&data_ptr[SEG_OFF_FLA]);
        if (is_linked) *is_linked = is_linked_segment(&prog_ptr[SEG_OFF_ATT]) ||
                                    is_linked_segment(&data_ptr[SEG_OFF_ATT]);
        if (is_used) *is_used = is_segment_used(&prog_ptr[SEG_OFF_ATT]) ||
                                is_segment_used(&data_ptr[SEG_OFF_ATT]);
    }

    return 0;
}

/*============================================================================
 * OLD-FORMAT DOMAIN: a :PSEG / :DSEG pair described by DESCRIPTION-FILE:DESC
 *
 * Before the self-contained :DOM file existed, a linked ND-500 domain was
 * three files on the owning user - <name>:PSEG, <name>:DSEG, <name>:LINK -
 * plus one entry in that user's DESCRIPTION-FILE:DESC (ND-30.003.007 System
 * Supervisor, "old domain format"; docs/CONVERT_DOMAIN_FORMAT_AND_USAGE.md).
 * The DESC entry is what turns the bare files into a runnable domain: it holds
 * the start address, the trap handler address (THA), the trap enable word and
 * the segment number. The DESC byte layout is include/nd500_desc.h.
 *
 * This loader stages such a domain into g_dom_file exactly as if it had been
 * read from a :DOM, so ndlib_dom_load_to_machine() places it unchanged. The
 * mapping below is not invented: it reproduces what the vendor CONVERT-DOMAIN
 * (A03) program writes when it converts the same files, checked on two
 * conversions made under nd500x (both read with a header dump script):
 *
 *   $ND500USERS/FLOPPY-USER/LINKAGE-LOAD-H02.DOM  (from the DESC
 *   entry LINKAGE-LOAD-H02: STADR b0000dd1, THA b0215310, ENABLEINT 0ffe00ac,
 *   PSEG use bit 22, .pseg 123989 bytes, .dseg 2184977 bytes)
 *     -> STADDR b0000dd1, THA b0215310, OTE1 fc015800, OTE2 0000001f,
 *        seg 22 PROG sz 123989 att 10002000, seg 22 DATA sz 2184977
 *        att e1002000, FLA 0 on both, TEMM1 fffffa00, TEMM2 0000001f
 *   $ND500USERS/SYSTEM/LED-NEW.DOM  (from LED-B03: STADR 08000004,
 *   THA 0806011c, ENABLEINT 0, PSEG use bit 1, .pseg 223695, .dseg 394525)
 *     -> STADDR 08000004, THA 0806011c, OTE 0, seg 1 PROG sz 223695,
 *        seg 1 DATA sz 394525, same ATT/TEMM values
 *
 * So: the WHOLE .pseg is the program segment image from segment offset 0, the
 * WHOLE .dseg is the data segment image from offset 0 (DLB is not an offset
 * into the segment - LINKAGE-LOAD has DLB 75834 and its converted DATA size
 * is still the full file), the segment number is the one bit set in the
 * domain entry's PSEG/DSEG use bitmaps, and start address and THA are copied.
 *
 * ENABLEINT -> OTE: every one of the 15 set bits of 0ffe00ac lands 9 bits
 * higher in the 64-bit OTE (OTE1 = low word, OTE2 = high word):
 *   bits 2,3,5,7 -> OTE1 bits 11,12,14,16; bits 17..22 -> OTE1 26..31;
 *   bits 23..27 -> OTE2 0..4.  OTE1 = ENABLEINT << 9, OTE2 = ENABLEINT >> 23.
 * That is derived from ONE non-zero sample (plus the trivial LED zero); it is
 * exact for that sample but has no second witness. TEMM is the same constant
 * in both conversions and is copied as such.
 *
 * All 13 DESC files known to this project (docs/CONVERT_DOMAIN_FORMAT_AND_USAGE.md
 * section B, NDInsight SINTRAN/File-Formats/samples) describe exactly ONE
 * segment per domain with PSEG use == DSEG use. A domain whose segment chain
 * has more than one entry is refused: the segment entry's own segment-number
 * field is not decoded (nd500_desc.h), so the entries could not be told apart
 * without guessing.
 *============================================================================*/

#include "nd500_desc.h"

static void olddom_err(char* err, size_t n, const char* fmt, ...) {
    if (!err || n == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, n, fmt, ap);
    va_end(ap);
}

static int olddom_read_file(const char* path, uint8_t** out, uint32_t* out_size) {
    *out = NULL; *out_size = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0 || sz > 0x10000000L) { fclose(f); return -1; }
    uint8_t* buf = malloc(sz > 0 ? (size_t)sz : 1);
    if (!buf) { fclose(f); return -1; }
    fseek(f, 0, SEEK_SET);
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    *out = buf; *out_size = (uint32_t)sz;
    return 0;
}

/* Copy a DESC name field: up to max bytes, ended by the 0x27 terminator. */
static void olddom_name(const uint8_t* p, size_t max, char* out, size_t n) {
    size_t i = 0;
    while (i < max && i + 1 < n && p[i] != DESC_DOMAIN_DNAME_TERMINATOR && p[i] != 0) {
        out[i] = (char)p[i];
        i++;
    }
    out[i] = '\0';
}

static int olddom_strieq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? (char)(*a - 32) : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? (char)(*b - 32) : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Segment number from a use bitmap: the index of its single set bit.
 * Returns -1 for zero and -2 for more than one bit. */
static int olddom_bitmap_segno(uint32_t bm) {
    if (bm == 0) return -1;
    int seg = -1;
    for (int i = 0; i < 32; i++) {
        if (bm & (1u << i)) {
            if (seg >= 0) return -2;
            seg = i;
        }
    }
    return seg;
}

/* Directory part of a path (no trailing slash); "." when there is none. */
static void olddom_dirname(const char* path, char* out, size_t n) {
    const char* last = NULL;
    for (const char* p = path; *p; p++) if (*p == '/') last = p;
    if (!last) { snprintf(out, n, "."); return; }
    size_t len = (size_t)(last - path);
    if (len == 0) len = 1;              /* "/x" -> "/" */
    if (len >= n) len = n - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

/* Locate <NAME>.<TYPE> for a DESC segment file name of the form
 * "(directory:user)NAME" or "NAME". Tried in order (root = the parent of
 * the DESC's directory, i.e. the sintran-root/USER layout):
 *   <root>/<user>/NAME.TYPE   only when the name carries a (directory:user)
 *                             prefix - where the DESC says the file is;
 *   <DESC directory>/NAME.TYPE   the owning user's own directory;
 *   <root>/SYSTEM/NAME.TYPE   the SINTRAN own-directory-then-(SYSTEM) rule
 *                             that every unqualified file open follows
 *                             (docs/SINTRAN-CONVENTIONS.md), so a pair kept
 *                             under SYSTEM is reachable from any user's DESC.
 * Returns 1 with out[] filled, 0 if none exists. */
static int olddom_segment_file(const char* desc_path, const char* sname,
                               const char* type, char* out, size_t n) {
    char user[64] = "";
    const char* name = sname;
    if (*sname == '(') {
        const char* close = strchr(sname, ')');
        if (close) {
            const char* colon = memchr(sname, ':', (size_t)(close - sname));
            const char* u = colon ? colon + 1 : sname + 1;
            size_t ul = (size_t)(close - u);
            if (ul >= sizeof(user)) ul = sizeof(user) - 1;
            memcpy(user, u, ul);
            user[ul] = '\0';
            name = close + 1;
        }
    }
    char desc_dir[512], root[512];
    olddom_dirname(desc_path, desc_dir, sizeof desc_dir);
    olddom_dirname(desc_dir, root, sizeof root);
    if (user[0]) {
        snprintf(out, n, "%s/%s/%s.%s", root, user, name, type);
        FILE* f = fopen(out, "rb");
        if (f) { fclose(f); return 1; }
    }
    snprintf(out, n, "%s/%s.%s", desc_dir, name, type);
    FILE* f = fopen(out, "rb");
    if (f) { fclose(f); return 1; }
    snprintf(out, n, "%s/SYSTEM/%s.%s", root, name, type);
    f = fopen(out, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static void olddom_write32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

int ndlib_load_old_domain(const char* desc_path, const char* domain_name,
                          char* err, size_t err_len) {
    if (err && err_len) err[0] = '\0';
    if (!desc_path || !domain_name || !*domain_name) return -1;

    uint8_t* desc = NULL;
    uint32_t desc_size = 0;
    if (olddom_read_file(desc_path, &desc, &desc_size) != 0) {
        olddom_err(err, err_len, "cannot read %s", desc_path);
        return -1;
    }

    /* ---- find the domain entry by exact name (case-insensitive) ---- */
    const uint8_t* dent = NULL;
    for (uint32_t idx = 0; idx <= DESC_MAX_DOMAIN_INDEX; idx++) {
        uint32_t pos = DESC_DOMAIN_ENTRY_POSITION(idx);
        if (pos + DESC_DOMAIN_ENTRY_SIZE > desc_size) break;
        const uint8_t* e = desc + pos;
        uint16_t fp = desc_read16(e + DESC_DOMAIN_FLAGPRIOR_OFFSET);
        if (!DESC_DOMAIN_FLAG_DINUSE(fp)) continue;
        char dname[DESC_DOMAIN_DNAME_SIZE + 1];
        olddom_name(e + DESC_DOMAIN_DNAME_OFFSET, DESC_DOMAIN_DNAME_SIZE, dname, sizeof dname);
        if (olddom_strieq(dname, domain_name)) { dent = e; break; }
    }
    if (!dent) { free(desc); return 1; }

    uint32_t seglink = desc_read32(dent + DESC_DOMAIN_SEGLINK_OFFSET);
    uint32_t stadr   = desc_read32(dent + DESC_DOMAIN_STADR_OFFSET);
    uint32_t enabint = desc_read32(dent + DESC_DOMAIN_ENABLEINT_OFFSET);
    uint32_t tha     = desc_read32(dent + DESC_DOMAIN_THA_OFFSET);
    uint32_t pbitmap = desc_read32(dent + DESC_DOMAIN_PBITMAP_OFFSET);
    uint32_t dbitmap = desc_read32(dent + DESC_DOMAIN_DBITMAP_OFFSET);

    if (seglink == 0 || seglink + DESC_SEGMENT_ENTRY_SIZE > desc_size) {
        olddom_err(err, err_len, "%s: domain %s has no segment entry (SEGLINK 0x%08X)",
                   desc_path, domain_name, seglink);
        free(desc);
        return -1;
    }
    const uint8_t* sent = desc + seglink;
    uint32_t next = desc_read32(sent + DESC_SEGMENT_SEGLINK_OFFSET);
    if (next != 0) {
        olddom_err(err, err_len, "%s: domain %s has more than one segment entry; "
                   "only single-segment old-format domains are supported "
                   "(the per-entry segment number is not decoded)",
                   desc_path, domain_name);
        free(desc);
        return -1;
    }

    int pseg_no = olddom_bitmap_segno(pbitmap);
    int dseg_no = olddom_bitmap_segno(dbitmap);
    if (pseg_no == -2 || dseg_no == -2) {
        olddom_err(err, err_len, "%s: domain %s uses more than one segment "
                   "(PSEG use 0x%08X, DSEG use 0x%08X) - not supported",
                   desc_path, domain_name, pbitmap, dbitmap);
        free(desc);
        return -1;
    }
    if (pseg_no < 0 && dseg_no < 0) {
        olddom_err(err, err_len, "%s: domain %s has empty PSEG/DSEG use bitmaps",
                   desc_path, domain_name);
        free(desc);
        return -1;
    }

    char sname[DESC_SEGMENT_SNAME_SIZE + 1];
    olddom_name(sent + DESC_SEGMENT_SNAME_OFFSET, DESC_SEGMENT_SNAME_SIZE, sname, sizeof sname);
    uint32_t plb   = desc_read32(sent + DESC_SEGMENT_PLB_OFFSET);
    uint32_t psize = desc_read32(sent + DESC_SEGMENT_PSIZE_OFFSET);
    uint32_t dlb   = desc_read32(sent + DESC_SEGMENT_DLB_OFFSET);
    uint32_t dsize = desc_read32(sent + DESC_SEGMENT_DSIZE_OFFSET);
    uint64_t want_pseg = DESC_PSEG_FILE_SIZE(plb, psize);
    uint64_t want_dseg = DESC_DSEG_FILE_SIZE(dlb, dsize);

    /* ---- read the segment files, checking them against the entry ---- */
    uint8_t* pbuf = NULL; uint32_t pbytes = 0;
    uint8_t* dbuf = NULL; uint32_t dbytes = 0;
    char ppath[1024] = "", dpath[1024] = "";

    if (pseg_no >= 0) {
        if (!olddom_segment_file(desc_path, sname, "PSEG", ppath, sizeof ppath)) {
            olddom_err(err, err_len, "%s: segment file %s:PSEG not found (looked for %s)",
                       desc_path, sname, ppath);
            free(desc);
            return -1;
        }
        if (olddom_read_file(ppath, &pbuf, &pbytes) != 0) {
            olddom_err(err, err_len, "cannot read %s", ppath);
            free(desc);
            return -1;
        }
        if ((uint64_t)pbytes != want_pseg) {
            olddom_err(err, err_len, "%s is %u bytes but the DESC entry says PLB+PSIZE+1 = %llu",
                       ppath, pbytes, (unsigned long long)want_pseg);
            free(pbuf); free(desc);
            return -1;
        }
    }
    if (dseg_no >= 0) {
        if (!olddom_segment_file(desc_path, sname, "DSEG", dpath, sizeof dpath)) {
            olddom_err(err, err_len, "%s: segment file %s:DSEG not found (looked for %s)",
                       desc_path, sname, dpath);
            free(pbuf); free(desc);
            return -1;
        }
        if (olddom_read_file(dpath, &dbuf, &dbytes) != 0) {
            olddom_err(err, err_len, "cannot read %s", dpath);
            free(pbuf); free(desc);
            return -1;
        }
        if ((uint64_t)dbytes != want_dseg) {
            olddom_err(err, err_len, "%s is %u bytes but the DESC entry says DLB+DSIZE+1 = %llu",
                       dpath, dbytes, (unsigned long long)want_dseg);
            free(pbuf); free(dbuf); free(desc);
            return -1;
        }
    }
    free(desc);

    /* ---- stage it as a DOM (values as CONVERT-DOMAIN A03 writes them) ---- */
    ndlib_close_dom();
    uint8_t* raw = g_dom_file.header.raw;
    raw[OFF_VERSION]  = 97;     /* both converted samples */
    raw[OFF_REVISION] = 3;
    raw[OFF_FLAGS]    = 0xF8;   /* includes ND500_FLAG_IS_DOMAIN_FILE */
    raw[OFF_MACHINE]  = 0;
    olddom_write32(&raw[OFF_STADDR],   stadr);
    olddom_write32(&raw[OFF_RESTADDR], 0xFFFFFFFFu);
    olddom_write32(&raw[OFF_THA],      tha);
    olddom_write32(&raw[0xEC],  enabint >> 23);           /* OTE2 */
    olddom_write32(&raw[0xF0],  enabint << 9);            /* OTE1 */
    olddom_write32(&raw[0xFC],  0x0000001Fu);             /* TEMM2 */
    olddom_write32(&raw[0x100], 0xFFFFFA00u);             /* TEMM1 */

    if (pseg_no >= 0 && pbytes > 0) {
        uint8_t* part = &raw[OFF_DOM_SEGTAB + pseg_no * SEG_DESC_SIZE];
        olddom_write32(&part[SEG_OFF_SZ], pbytes);
        olddom_write32(&part[SEG_OFF_ATT], 0x10002000u);
        g_dom_file.segment_data[pseg_no] = pbuf;
        g_dom_file.segment_size[pseg_no] = pbytes;
        g_dom_file.segment_load_addr[pseg_no] = 0;
        pbuf = NULL;
    }
    if (dseg_no >= 0 && dbytes > 0) {
        uint8_t* part = &raw[OFF_DOM_SEGTAB + dseg_no * SEG_DESC_SIZE + SEG_PART_SIZE];
        olddom_write32(&part[SEG_OFF_SZ], dbytes);
        olddom_write32(&part[SEG_OFF_ATT], 0xE1002000u);
        g_dom_file.data_data[dseg_no] = dbuf;
        g_dom_file.data_size[dseg_no] = dbytes;
        g_dom_file.data_load_addr[dseg_no] = 0;
        dbuf = NULL;
    }
    free(pbuf); free(dbuf);

    g_dom_file.is_dom = 1;
    g_dom_file.file = NULL;
    /* The PSEG path names the domain for the debugger's domain registry
     * (basename without extension), the same way a :DOM path does. */
    snprintf(g_dom_file.filepath, sizeof g_dom_file.filepath, "%s",
             ppath[0] ? ppath : dpath);
    g_dom_file.is_loaded = 1;

    nd500_log("OLD-FORMAT Load: %s from %s: PSEG seg %d %u bytes, DSEG seg %d %u bytes, "
              "start 0x%08X THA 0x%08X ENABLEINT 0x%08X",
              domain_name, desc_path, pseg_no, pbytes, dseg_no, dbytes, stadr, tha, enabint);
    return 0;
}
