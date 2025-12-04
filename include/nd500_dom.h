/*
 * ND-500 Domain/Segment File Header Structures
 *
 * Reference: ND-860289-2-EN ND Linker User Guide and Reference Manual
 *
 * IMPORTANT: ND-500 is BIG-ENDIAN. Use byte-swap macros when reading on little-endian systems.
 */

#ifndef ND500_DOM_H
#define ND500_DOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read big-endian values from byte pointer */
static inline uint16_t nd500_read16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t nd500_read32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* Header size constants */
#define ND500_HEADER_SIZE       4096    /* 2 pages x 2048 bytes */
#define ND500_PAGE_SIZE         2048
#define ND500_MAX_SEGMENTS      32
#define ND500_MAX_INDIRECT_SEGS 32
#define ND500_MAX_LINKED_SEGS   32
#define ND500_MAX_N100_SEGS     10
#define ND500_MAX_CHILDREN      16

/* Segment table offset in DOM header */
#define ND500_SEGTAB_OFFSET     0x254   /* Octal 1124 - start of segment descriptors */

/* Name pool offsets */
#define ND500_NAMEPOOL_START_DOM  0x954  /* Octal 4524 */
#define ND500_NAMEPOOL_START_SEG  0x454  /* Octal 2124 */
#define ND500_NAMEPOOL_END        0x1000 /* Octal 10000 */

/*============================================================================
 * FLAGS field (byte at offset 0x06)
 *============================================================================*/
typedef enum {
    ND500_FLAG_TRAPBLOCK_VALID = (1 << 3),  /* Bit 3: TrapBlock is valid */
    ND500_FLAG_IS_DOMAIN_FILE  = (1 << 4),  /* Bit 4: TRUE if :DOM file */
    ND500_FLAG_IS_ROOT_DOMAIN  = (1 << 5),  /* Bit 5: Root-domain (Multidomain) */
    ND500_FLAG_IS_SINTRAN_III  = (1 << 6),  /* Bit 6: SIN-III domain */
    ND500_FLAG_IS_ND500        = (1 << 7),  /* Bit 7: TRUE if ND-500/5000 */
} nd500_domain_flags_t;

/*============================================================================
 * Segment ATTributes (32-bit field)
 *============================================================================*/
typedef enum {
    ND500_SEG_ATT_FIXED_ABSOLUTE    = (1 << 10),
    ND500_SEG_ATT_FIXED_CONTIGUOUS  = (1 << 11),
    ND500_SEG_ATT_FIXED_SCATTERED   = (1 << 12),
    ND500_SEG_ATT_SEGMENT_USED      = (1 << 13),  /* Slot contains valid data */
    ND500_SEG_ATT_LINKED_SEGMENT    = (1 << 14),  /* LB/SZ are name index/LINKKEY */
    ND500_SEG_ATT_ROUTINE_VECTOR    = (1 << 15),
    ND500_SEG_ATT_INSUFF_LOADED     = (1 << 16),
    ND500_SEG_ATT_FORTRAN_COMMON    = (1 << 17),
    ND500_SEG_ATT_OTHER_MACHINE     = (1 << 18),
    ND500_SEG_ATT_START_VECTOR      = (1 << 19),
    ND500_SEG_ATT_INDIRECT          = (1 << 20),
    ND500_SEG_ATT_SHARED_ND100      = (1 << 21),
    ND500_SEG_ATT_COPY_CAPABILITY   = (1 << 22),
    ND500_SEG_ATT_CLEAR_CAPABILITY  = (1 << 23),
    ND500_SEG_ATT_CACHE             = (1 << 24),
    ND500_SEG_ATT_FILE_AS_SEGMENT   = (1 << 25),
    ND500_SEG_ATT_EMPTY_DATA        = (1 << 26),
    ND500_SEG_ATT_SHARED_DATA       = (1 << 27),
    ND500_SEG_ATT_PROGRAM_SEGMENT   = (1 << 28),
    ND500_SEG_ATT_SWAP_ON_SWAPFILE  = (1 << 29),
    ND500_SEG_ATT_PARAMETER_ACCESS  = (1 << 30),
    ND500_SEG_ATT_WRITE_PERMIT      = (1u << 31),
} nd500_seg_att_t;

#pragma pack(push, 1)

/*============================================================================
 * File Header (shared by DOM and SEG) - 16 bytes at offset 0x00
 *============================================================================*/
typedef struct {
    uint32_t linklock;          /* 0x00: LINKLOCK random number */
    uint8_t  version;           /* 0x04: Linker version */
    uint8_t  revision;          /* 0x05: Linker revision */
    uint8_t  flags;             /* 0x06: Domain flags (nd500_domain_flags_t) */
    uint8_t  machine;           /* 0x07: Target machine type */
    uint8_t  os_id;             /* 0x08: Operating system ID */
    uint8_t  reserved;          /* 0x09: Reserved */
    uint8_t  subsystem_key[6];  /* 0x0A: Subsystem key */
} nd500_file_header_t;  /* 16 bytes, ends at 0x10 */

/*============================================================================
 * Segment Part (Program or Data) - 28 bytes
 *============================================================================*/
typedef struct {
    uint32_t lb;    /* Lower Bound (file offset) - or name pool index if LINKED */
    uint32_t sz;    /* Size in bytes - or LINKKEY if LINKED */
    uint32_t att;   /* Attributes (nd500_seg_att_t) */
    uint32_t fla;   /* Fixed Lower Address */
    uint32_t fua;   /* Fixed Upper Address */
    uint32_t afa;   /* Absolute Fix Address */
    uint16_t minp;  /* Min pages - or MIN name index if LINKED */
    uint16_t maxp;  /* Max pages - or MAX name index if LINKED */
} nd500_segment_part_t;  /* 28 bytes */

/*============================================================================
 * Full Segment Descriptor (Program + Data) - 56 bytes
 * Segments are stored contiguously in DOM files starting at offset 0x254
 * 32 segments x 56 bytes = 1792 bytes (0x700), ending at 0x954 (name pool start)
 *============================================================================*/
typedef struct {
    nd500_segment_part_t program;  /* 28 bytes */
    nd500_segment_part_t data;     /* 28 bytes */
} nd500_segment_desc_t;  /* 56 bytes */

/*============================================================================
 * Domain Reference (Mother/Child) - 8 bytes
 *============================================================================*/
typedef struct {
    uint16_t min_index;   /* MIN index to name in name pool */
    uint16_t max_index;   /* MAX index to name in name pool */
    uint32_t link_key;    /* Should match LINKLOCK of referenced domain */
} nd500_domain_ref_t;  /* 8 bytes */

/*============================================================================
 * Common Part (shared by DOM and SEG) - starts at 0xC6
 *============================================================================*/
typedef struct {
    uint16_t freind;      /* 0xC6: Free pointer in name pool */
    uint32_t deb_lb;      /* 0xC8: Debug info Lower Bound */
    uint32_t deb_sz;      /* 0xCC: Debug info Size */
    uint32_t link_lb;     /* 0xD0: Link info Lower Bound */
    uint32_t link_sz;     /* 0xD4: Link info Size */
    uint32_t staddr;      /* 0xD8: Start address */
    uint32_t restaddr;    /* 0xDC: Restart address */
    uint32_t tha;         /* 0xE0: Trap Handler Address */
    uint32_t mte2;        /* 0xE4: Memory Trap Enable (high 32 bits) */
    uint32_t mte1;        /* 0xE8: Memory Trap Enable (low 32 bits) */
    uint32_t ote2;        /* 0xEC: Own Trap Enable (high 32 bits) */
    uint32_t ote1;        /* 0xF0: Own Trap Enable (low 32 bits) */
    uint32_t cte2;        /* 0xF4: Child Trap Enable (high 32 bits) */
    uint32_t cte1;        /* 0xF8: Child Trap Enable (low 32 bits) */
    uint32_t temm2;       /* 0xFC: Trap Enable Mod Mask (high 32 bits) */
    uint32_t temm1;       /* 0x100: Trap Enable Mod Mask (low 32 bits) */
    uint32_t priority;    /* 0x104: Process priority */
} nd500_common_part_t;  /* 66 bytes, ends at 0x108 */

/*============================================================================
 * Indirect Segment - 10 bytes
 *============================================================================*/
typedef struct {
    uint16_t min_index;   /* MIN index to name in name pool */
    uint16_t max_index;   /* MAX index to name in name pool */
    uint32_t link_key;    /* Link key */
    uint8_t  slog;        /* Logical segment number */
    uint8_t  reserved;    /* Reserved */
} nd500_indirect_seg_t;  /* 10 bytes */

/*============================================================================
 * Linked Segment (SEG files only) - 16 bytes
 *============================================================================*/
typedef struct {
    uint16_t prog_min;    /* Program segment MIN name index */
    uint16_t prog_max;    /* Program segment MAX name index */
    uint32_t prog_key;    /* Program link key */
    uint16_t data_min;    /* Data segment MIN name index */
    uint16_t data_max;    /* Data segment MAX name index */
    uint32_t data_key;    /* Data link key */
} nd500_linked_seg_t;  /* 16 bytes */

/*============================================================================
 * ND-100 RT Segment (SEG files only) - 12 bytes
 *============================================================================*/
typedef struct {
    uint8_t  n100sw[6];   /* ND-100 segment name */
    uint16_t n100sno;     /* ND-100 segment number */
    uint16_t n500logpa;   /* Map address in ND-500 logical memory (pages) */
    uint16_t n100size;    /* ND-100 segment size (pages) */
} nd500_n100_rt_seg_t;  /* 12 bytes */

/*============================================================================
 * DOM-specific part (from 0x10 to CommonPart)
 *============================================================================*/
typedef struct {
    uint16_t privileges[4];                         /* 0x10: Domain privileges */
    uint8_t  _pad_0x18[0x26 - 0x18];                /* Padding to 0x26 */
    nd500_domain_ref_t mother;                      /* 0x26: Mother domain */
    nd500_domain_ref_t children[ND500_MAX_CHILDREN];/* 0x2E: 16 child domains */
    uint8_t  _pad_to_common[0xC6 - (0x2E + 16*8)];  /* Padding to CommonPart */
} nd500_dom_specific_t;

/*============================================================================
 * SEG-specific part (from 0x10 to CommonPart)
 *============================================================================*/
typedef struct {
    nd500_segment_part_t program;   /* 0x10: Program segment (28 bytes) */
    uint8_t _pad1[0x38 - 0x2C];     /* Padding */
    nd500_segment_part_t data;      /* 0x38: Data segment (28 bytes) */
    uint8_t _pad2[0x70 - 0x54];     /* Padding */
    uint8_t  prog_logseg;           /* 0x70: Program logical segment number */
    uint8_t  data_logseg;           /* 0x71: Data logical segment number */
    uint16_t n100_count;            /* 0x72: Number of ND-100 RT segments */
    nd500_n100_rt_seg_t n100_segs[ND500_MAX_N100_SEGS];  /* 0x74: 10 x 12 bytes, ends at 0xEC */
    /* Note: SEG-specific data extends past common part start (0xC6) - no padding needed */
} nd500_seg_specific_t;

/*============================================================================
 * Complete DOM Header - 4096 bytes
 *============================================================================*/
typedef struct {
    nd500_file_header_t file;                    /* 0x00: File header (16 bytes) */
    nd500_dom_specific_t dom;                    /* 0x10: DOM-specific part */
    nd500_common_part_t common;                  /* 0xC6: Common part */
    nd500_indirect_seg_t indirect[ND500_MAX_INDIRECT_SEGS]; /* 0x108: 32 x 10 bytes */
    uint32_t language_msal;                      /* 0x248: Language and MSAL */
    uint16_t idx_free_min;                       /* 0x24C: Free text MIN index */
    uint16_t idx_free_max;                       /* 0x24E: Free text MAX index */
    uint8_t  _pad_to_segtab[0x254 - 0x250];      /* Padding to segment table */
    nd500_segment_desc_t segments[ND500_MAX_SEGMENTS]; /* 0x254: 32 segments x 56 bytes = 1792 bytes */
    uint8_t  name_pool[0x1000 - 0x954];          /* 0x954: Name pool to end of header */
} nd500_dom_header_t;

/*============================================================================
 * Complete SEG Header - 4096 bytes
 *============================================================================*/
typedef struct {
    nd500_file_header_t file;                    /* 0x00: File header (16 bytes) */
    nd500_seg_specific_t seg;                    /* 0x10: SEG-specific part */
    nd500_common_part_t common;                  /* 0xC6: Common part */
    nd500_indirect_seg_t indirect[ND500_MAX_INDIRECT_SEGS]; /* 0x108: 32 x 10 bytes */
    uint32_t language_msal;                      /* 0x1C8: Language and MSAL */
    uint16_t idx_min;                            /* 0x1CC: ID message MIN index */
    uint16_t idx_max;                            /* 0x1CE: ID message MAX index */
    uint8_t  _pad_0x1D0[4];                      /* Reserved */
    nd500_linked_seg_t linked[ND500_MAX_LINKED_SEGS]; /* 0x1D4: 32 x 16 bytes */
    uint8_t  name_pool[0x1000 - 0x454];          /* Name pool to end of header */
} nd500_seg_header_t;

/*============================================================================
 * Union for DOM or SEG header
 *============================================================================*/
typedef union nd500_header {
    nd500_dom_header_t dom;
    nd500_seg_header_t seg;
    uint8_t raw[ND500_HEADER_SIZE];
} nd500_header_t;

#pragma pack(pop)

/*============================================================================
 * Helper functions
 *============================================================================*/

/* Check if header is a DOM file (vs SEG) */
static inline int nd500_is_dom_file(const nd500_header_t *hdr) {
    return (hdr->raw[6] & ND500_FLAG_IS_DOMAIN_FILE) != 0;
}

/* Get segment descriptor from DOM header by index */
static inline nd500_segment_desc_t* nd500_dom_get_segment(nd500_header_t *hdr, int seg_num) {
    return &hdr->dom.segments[seg_num];
}

/* Check if segment is linked (ATT.LINKED bit set) - pass pointer to att field */
static inline int nd500_seg_is_linked(const uint8_t* att_ptr) {
    return (nd500_read32(att_ptr) & ND500_SEG_ATT_LINKED_SEGMENT) != 0;
}

/* Check if segment slot is used (ATT.SEGMENTUSED bit set) - pass pointer to att field */
static inline int nd500_seg_is_used(const uint8_t* att_ptr) {
    return (nd500_read32(att_ptr) & ND500_SEG_ATT_SEGMENT_USED) != 0;
}

/* Resolve name from name pool (returns pointer into pool, not copied) */
static inline const char* nd500_resolve_name(const uint8_t *header, const uint8_t* min_idx_ptr, const uint8_t* max_idx_ptr, int is_seg) {
    uint16_t pool_start = is_seg ? ND500_NAMEPOOL_START_SEG : ND500_NAMEPOOL_START_DOM;
    uint16_t min_idx = nd500_read16(min_idx_ptr);
    uint16_t max_idx = nd500_read16(max_idx_ptr);
    if (min_idx == 0 || max_idx == 0 || min_idx >= max_idx) return NULL;
    return (const char*)(header + pool_start + min_idx);
}

#ifdef __cplusplus
}
#endif

#endif /* ND500_DOM_H */
