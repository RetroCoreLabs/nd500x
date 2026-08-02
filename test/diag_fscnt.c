/*
 * Unit test for MON 412B FSCNT file-backed segment mapping.
 * Calls nd500_mon_connect_file_as_segment on a known file, then reads the mapped
 * logical segment through the MMU and confirms the bytes match the file.
 *
 * Build: gcc -O2 -o build/bin/diag_fscnt test/diag_fscnt.c \
 *   -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *   build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *   build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include "testdata.h"

#define MEMSZ (16u*1024u*1024u)

extern int nd500_mon_connect_file_as_segment(void* cpu, void* machine, uint8_t domain,
    uint32_t requested_segment, uint32_t access_type, int writable,
    const char* host_path, uint32_t file_size_bytes, uint32_t* out_assigned_segment);

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : nd500_testdata("FraTor/test-real/test-real.nrf");

    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m, MEMSZ);
    nd500_cpu_init(&c, &m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c);
    /* Enable the data MMU so translate/peek runs the real page-table path. */
    nd500_mmu_enable_data(&c);

    uint32_t seg = 0;
    int rc = nd500_mon_connect_file_as_segment(&c, &m, 0xFF, /*req=*/0,
                 /*access_type=*/0, /*writable=*/1, path, 0, &seg);
    printf("connect rc=%d assigned segment=%u\n", rc, seg);
    if (rc != 0) { fprintf(stderr, "FAIL: connect returned %d\n", rc); return 1; }

    /* Read the file's first 32 bytes directly. */
    FILE* fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "cannot open %s\n", path); return 2; }
    unsigned char fbuf[32]; size_t n = fread(fbuf, 1, sizeof(fbuf), fp); fclose(fp);

    /* Read the same bytes through the MMU from the mapped logical segment. */
    uint32_t vbase = seg << 27;  /* logical seg field is the top 5 bits */
    printf("file[0..%zu]  vs  segment@0x%08X:\n", n, vbase);
    int mismatch = 0;
    printf("  file: "); for (size_t i=0;i<n;i++) printf("%02X ", fbuf[i]); printf("\n");
    printf("  segm: ");
    for (size_t i = 0; i < n; i++) {
        uint32_t phys = nd500_mmu_peek(&c, vbase + (uint32_t)i);
        uint8_t b = (phys == 0xFFFFFFFFu) ? 0xEE : nd500_bus_read8(&m, phys);
        printf("%02X ", b);
        if (b != fbuf[i]) mismatch++;
    }
    printf("\n");
    if (mismatch == 0) { printf("PASS: mapped segment bytes match the file.\n"); return 0; }
    fprintf(stderr, "FAIL: %d byte mismatches\n", mismatch);
    return 1;
}
