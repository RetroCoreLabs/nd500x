#include <stdio.h>
#include <stdint.h>
#include "../src/cpu/nd500_mmu.h"

/*
 * Test program to verify MMU structure sizes match C# implementation
 * Run this after building to ensure correct memory layout
 */

int main(void) {
    printf("ND-500 MMU Structure Size Verification\n");
    printf("========================================\n\n");

    /* PhysicalSegmentTableEntry */
    printf("PhysicalSegmentTableEntry:\n");
    printf("  sizeof() = %zu bytes\n", sizeof(PhysicalSegmentTableEntry));
    printf("  Expected: ~8 bytes (1 byte index_mode + padding + 4 bytes pfn)\n");
    printf("  Status: %s\n\n", sizeof(PhysicalSegmentTableEntry) <= 12 ? "OK" : "WARNING");

    /* PageTableEntry */
    printf("PageTableEntry:\n");
    printf("  sizeof() = %zu bytes\n", sizeof(PageTableEntry));
    printf("  Expected: ~8 bytes (1 byte protection + padding + 4 bytes pfn)\n");
    printf("  Status: %s\n\n", sizeof(PageTableEntry) <= 12 ? "OK" : "WARNING");

    /* ProcessControlBlock */
    printf("ProcessControlBlock:\n");
    printf("  sizeof() = %zu bytes\n", sizeof(ProcessControlBlock));
    printf("  Expected: variable (C# class is not fixed size)\n");
    printf("  Note: C struct will be larger due to all fields being stored inline\n\n");

    /* Constants verification */
    printf("MMU Constants:\n");
    printf("  NBPG (page size) = %d bytes\n", NBPG);
    printf("  PGSHIFT = %d\n", PGSHIFT);
    printf("  MAXSEG = %d segments\n", MAXSEG);
    printf("  MAXDOM = %d domains\n", MAXDOM);
    printf("  MAX_PST = %d entries\n", MAX_PST);
    printf("  PCBSIZ = %d bytes\n", PCBSIZ);
    printf("\n");

    /* Memory requirements */
    printf("Memory Requirements:\n");
    printf("  PST Table: %d entries × %zu bytes = %zu KB\n",
           MAX_PST, sizeof(PhysicalSegmentTableEntry),
           (MAX_PST * sizeof(PhysicalSegmentTableEntry)) / 1024);
    printf("  PCB Table: %d domains × %zu bytes = %zu KB\n",
           MAXDOM, sizeof(ProcessControlBlock),
           (MAXDOM * sizeof(ProcessControlBlock)) / 1024);
    printf("  Total MMU memory: ~%zu KB\n",
           ((MAX_PST * sizeof(PhysicalSegmentTableEntry)) +
            (MAXDOM * sizeof(ProcessControlBlock))) / 1024);
    printf("\n");

    /* Indexing modes */
    printf("PST Indexing Modes:\n");
    printf("  PS_AZI (direct) = %d\n", PS_AZI);
    printf("  PS_ASI (single-level) = %d\n", PS_ASI);
    printf("  PS_ADI (two-level) = %d\n", PS_ADI);
    printf("\n");

    printf("All structure size checks complete!\n");

    return 0;
}
