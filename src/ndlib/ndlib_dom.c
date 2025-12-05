/*
 * ND-500 DOM/SEG File Loader
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
                    } else {
                        free(g_dom_file.data_data[i]);
                        g_dom_file.data_data[i] = NULL;
                    }
                }
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
