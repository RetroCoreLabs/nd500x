/*
 * Memory Map Generator for ND-500 Emulator
 *
 * Generates a JSON representation of physical memory usage by scanning:
 * - PST (Physical Segment Table) entries to find physical memory allocations
 * - PCB (Process Control Block) to find which domains/segments map to each physical block
 *
 * Output format:
 * {
 *   "total_memory": 16777216,
 *   "blocks": [
 *     {
 *       "phys_start": 0,
 *       "phys_end": 4194304,
 *       "size": 4194304,
 *       "domain": 0,
 *       "segment": 0,
 *       "virtual_start": 134217728,
 *       "psn": 100,
 *       "mode": "AZI",
 *       "writable": false,
 *       "public": false
 *     },
 *     { ... }
 *   ]
 * }
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "machine_protos.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/nd500_mmu.h"

/* Both arms of the old #ifndef HAVE_SYSTEM_CJSON here included the very same
 * header, so the test decided nothing. cJSON is a hard requirement of this
 * file either way; the top-level CMakeLists guarantees the header is reachable
 * under <cjson/...> whether it came from a package or from FetchContent. */
#include <cjson/cJSON.h>

/* Internal representation of a memory block */
typedef struct {
    uint32_t phys_start;
    uint32_t phys_end;
    uint32_t size;
    int domain;        /* -1 = free/unmapped */
    int segment;       /* -1 = free/unmapped */
    uint32_t virtual_start;
    int psn;           /* -1 = free/unmapped */
    uint8_t mode;      /* PS_AZI, PS_ASI, PS_ADI */
    int writable;
    int public;        /* User accessible */
} MemoryBlock;

#define MAX_BLOCKS 8192  /* Support all possible PST entries (MAX_PST = 8192) */
static MemoryBlock g_blocks[MAX_BLOCKS];
static int g_block_count = 0;

/* Helper: Add a memory block */
static void add_block(uint32_t phys_start, uint32_t phys_end, int domain, int segment,
                      uint32_t virtual_start, int psn, uint8_t mode, int writable, int public) {
    if (g_block_count >= MAX_BLOCKS) return;

    g_blocks[g_block_count].phys_start = phys_start;
    g_blocks[g_block_count].phys_end = phys_end;
    g_blocks[g_block_count].size = phys_end - phys_start;
    g_blocks[g_block_count].domain = domain;
    g_blocks[g_block_count].segment = segment;
    g_blocks[g_block_count].virtual_start = virtual_start;
    g_blocks[g_block_count].psn = psn;
    g_blocks[g_block_count].mode = mode;
    g_blocks[g_block_count].writable = writable;
    g_blocks[g_block_count].public = public;
    g_block_count++;
}

/* Comparison function for qsort (sort by physical address) */
static int compare_blocks(const void* a, const void* b) {
    const MemoryBlock* ba = (const MemoryBlock*)a;
    const MemoryBlock* bb = (const MemoryBlock*)b;
    if (ba->phys_start < bb->phys_start) return -1;
    if (ba->phys_start > bb->phys_start) return 1;
    return 0;
}

/**
 * Check if a PSN is accessible from a specific domain
 * Returns 1 if the domain has a capability (program or data) that maps to this PSN
 */
static int psn_accessible_from_domain(Nd500Cpu* cpu, uint32_t psn, int domain) {
    /* Check program capabilities */
    for (int seg = 0; seg < MAXSEG; seg++) {
        uint16_t pc = nd500_mmu_get_program_capability(cpu, domain, seg);
        if ((pc & PC_PSN) == psn) {
            return 1;
        }
    }

    /* Check data capabilities */
    for (int seg = 0; seg < MAXSEG; seg++) {
        uint16_t dc = nd500_mmu_get_data_capability(cpu, domain, seg);
        if ((dc & DC_PSN) == psn) {
            return 1;
        }
    }

    return 0;
}

/**
 * Generate memory map as JSON
 *
 * Algorithm:
 * 1. Scan all PST entries (0-8191)
 * 2. For each configured PST entry, calculate physical address range
 * 3. Find which PCB capabilities point to this PSN
 * 4. Determine domain, segment, virtual address
 * 5. Sort by physical address
 * 6. Fill gaps with "free" blocks
 * 7. Return JSON
 *
 * @param filter_domain: -1 for all domains, 0-255 for specific domain
 */
static const char* nd500_dbg_memory_map_json_filtered(Nd500Machine* m, int filter_domain) {
    static char json_buffer[524288];  /* 512KB buffer - enough for 8192 PST entries */

    if (!m || !m->cpu) {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"Invalid machine\"}");
        return json_buffer;
    }

    g_block_count = 0;

    /* If MMU is disabled, scan physical memory directly for non-zero 2KB blocks */
    if (!nd500_machine_mmu_is_enabled(m)) {
        uint32_t num_pages = m->memory_size / NBPG;  /* Total 2KB pages */

        for (uint32_t page = 0; page < num_pages; page++) {
            uint32_t phys_addr = page << PGSHIFT;  /* page * 2048 */
            int has_data = 0;

            /* Check if this 2KB page has any non-zero bytes */
            for (uint32_t offset = 0; offset < NBPG && !has_data; offset += 64) {
                uint32_t check_addr = phys_addr + offset;
                if (check_addr < m->memory_size) {
                    uint8_t byte = nd500_bus_read8(m, check_addr);
                    if (byte != 0) has_data = 1;
                }
            }

            /* Add block if it contains data */
            if (has_data) {
                uint32_t phys_start = phys_addr;
                uint32_t phys_end = phys_start + NBPG;
                if (phys_end > m->memory_size) phys_end = m->memory_size;

                /* domain=-1 means "physical memory, no MMU mapping" */
                add_block(phys_start, phys_end, -1, -1, 0, -1, 0, 0, 0);
            }
        }

        /* Sort and generate JSON */
        qsort(g_blocks, g_block_count, sizeof(MemoryBlock), compare_blocks);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "total_memory", m->memory_size);

        cJSON* blocks_array = cJSON_CreateArray();
        for (int i = 0; i < g_block_count; i++) {
            cJSON* block = cJSON_CreateObject();
            cJSON_AddNumberToObject(block, "phys_start", g_blocks[i].phys_start);
            cJSON_AddNumberToObject(block, "phys_end", g_blocks[i].phys_end);
            cJSON_AddNumberToObject(block, "size", g_blocks[i].size);
            cJSON_AddNullToObject(block, "domain");  /* No domain info without MMU */
            cJSON_AddItemToArray(blocks_array, block);
        }

        cJSON_AddItemToObject(root, "blocks", blocks_array);

        char* json_str = cJSON_PrintUnformatted(root);
        if (json_str) {
            strncpy(json_buffer, json_str, sizeof(json_buffer) - 1);
            json_buffer[sizeof(json_buffer) - 1] = '\0';
            free(json_str);
        } else {
            snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"JSON generation failed\"}");
        }

        cJSON_Delete(root);
        return json_buffer;
    }

    /* Step 1: Scan all PST entries */
    for (uint32_t psn = 0; psn < MAX_PST; psn++) {
        PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(m->cpu, psn);

        /* Skip empty entries */
        if (pst.index_mode == 0 && pst.physical_pfn == 0) continue;

        /* Calculate physical address range (ONE PAGE = 2KB, not 128MB segment!) */
        uint32_t phys_start = pst.physical_pfn << PGSHIFT;  /* PFN * 2048 */
        uint32_t phys_end = phys_start + NBPG;  /* +2048 bytes (1 page) */

        /* Clamp to actual memory size */
        if (phys_end > m->memory_size) phys_end = m->memory_size;
        if (phys_start >= m->memory_size) continue;

        /* Step 2: Find which domain/segment uses this PSN */
        int found_domain = -1;
        int found_segment = -1;
        uint32_t virtual_start = 0;
        int writable = 0;
        int public = 0;

        /* Scan all domains */
        for (uint32_t domain = 0; domain < MAXDOM && found_domain < 0; domain++) {
            /* Check program capabilities */
            for (int seg = 0; seg < MAXSEG; seg++) {
                uint16_t pc = nd500_mmu_get_program_capability(m->cpu, domain, seg);
                if ((pc & PC_PSN) == psn) {
                    found_domain = domain;
                    found_segment = seg;
                    /* Calculate virtual address: segment << 27 */
                    virtual_start = ((uint32_t)seg) << SGSHIFT;
                    writable = 1;  /* Program segments usually writable (code) */
                    public = (pc & PC_DIR) ? 0 : 1;
                    break;
                }
            }

            /* Check data capabilities if not found in program */
            if (found_domain < 0) {
                for (int seg = 0; seg < MAXSEG; seg++) {
                    uint16_t dc = nd500_mmu_get_data_capability(m->cpu, domain, seg);
                    if ((dc & DC_PSN) == psn) {
                        found_domain = domain;
                        found_segment = seg;
                        virtual_start = ((uint32_t)seg) << SGSHIFT;
                        writable = (dc & DC_WRP) ? 0 : 1;  /* DC_WRP=0 means writable */
                        public = (dc & DC_PAC) ? 1 : 0;
                        break;
                    }
                }
            }
        }

        /* Add block */
        const char* mode_str = (pst.index_mode == PS_AZI) ? "AZI" :
                               (pst.index_mode == PS_ASI) ? "ASI" : "ADI";

        add_block(phys_start, phys_end, found_domain, found_segment, virtual_start,
                  psn, pst.index_mode, writable, public);
    }

    /* Step 3: Sort blocks by physical address */
    qsort(g_blocks, g_block_count, sizeof(MemoryBlock), compare_blocks);

    /* Step 4: Build JSON using cJSON */
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "total_memory", m->memory_size);

    cJSON* blocks_array = cJSON_CreateArray();

    /* Add mapped blocks */
    for (int i = 0; i < g_block_count; i++) {
        cJSON* block = cJSON_CreateObject();
        cJSON_AddNumberToObject(block, "phys_start", g_blocks[i].phys_start);
        cJSON_AddNumberToObject(block, "phys_end", g_blocks[i].phys_end);
        cJSON_AddNumberToObject(block, "size", g_blocks[i].size);

        if (g_blocks[i].domain >= 0) {
            cJSON_AddNumberToObject(block, "domain", g_blocks[i].domain);
            cJSON_AddNumberToObject(block, "segment", g_blocks[i].segment);
            cJSON_AddNumberToObject(block, "virtual_start", g_blocks[i].virtual_start);
            cJSON_AddNumberToObject(block, "psn", g_blocks[i].psn);

            const char* mode_str = (g_blocks[i].mode == PS_AZI) ? "AZI" :
                                   (g_blocks[i].mode == PS_ASI) ? "ASI" : "ADI";
            cJSON_AddStringToObject(block, "mode", mode_str);
            cJSON_AddBoolToObject(block, "writable", g_blocks[i].writable);
            cJSON_AddBoolToObject(block, "public", g_blocks[i].public);

            /* Check if this block is accessible from the filter domain */
            if (filter_domain >= 0 && filter_domain <= 255) {
                int accessible = psn_accessible_from_domain(m->cpu, g_blocks[i].psn, filter_domain);
                cJSON_AddBoolToObject(block, "accessible_from_filter", accessible);
            } else {
                /* No filter - always accessible */
                cJSON_AddBoolToObject(block, "accessible_from_filter", 1);
            }
        } else {
            cJSON_AddNullToObject(block, "domain");
            cJSON_AddBoolToObject(block, "accessible_from_filter", 0);
        }

        cJSON_AddItemToArray(blocks_array, block);
    }

    cJSON_AddItemToObject(root, "blocks", blocks_array);

    /* Convert to string */
    char* json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        strncpy(json_buffer, json_str, sizeof(json_buffer) - 1);
        json_buffer[sizeof(json_buffer) - 1] = '\0';
        free(json_str);
    } else {
        snprintf(json_buffer, sizeof(json_buffer), "{\"error\":\"JSON generation failed\"}");
    }

    cJSON_Delete(root);
    return json_buffer;
}

/* Public API: Memory map for all domains */
const char* nd500_dbg_memory_map_json(Nd500Machine* m) {
    return nd500_dbg_memory_map_json_filtered(m, -1);
}

/* Public API: Memory map filtered by specific domain */
const char* nd500_dbg_memory_map_for_domain_json(Nd500Machine* m, int domain) {
    return nd500_dbg_memory_map_json_filtered(m, domain);
}
