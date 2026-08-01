/*
 * ND-500 Page Used (PGU) and Written In Page (WIP) tables.
 *
 * Two hardware-maintained bitmaps, one bit per 2 KB physical page frame:
 *
 *   PGU - set when the page is touched by any instruction (read or write).
 *   WIP - set when the page is written into.
 *
 * The swapper reads them to decide what to evict and what must be flushed to
 * disk first. Six privileged instructions expose them: RPGU/RWIP read a bit or
 * a 16-bit group, ZPGU/ZWIP clear one bit, CPGU/CWIP clear the whole table.
 *
 * Why this is not optional. Both tables used to read back as all-zero, and the
 * comments called that "safe". It is not - zero is the AGGRESSIVE answer in
 * both directions:
 *
 *   - NDIX's pageout clock hand (kernel/MASTER/sys/vm_page.c:547) reclaims a
 *     page whenever RPGU says 0, so an always-zero PGU table makes the hand
 *     take EVERY page it inspects on the first sweep instead of giving
 *     recently-used pages a second chance.
 *   - `dirty(pte)` (kernel/MASTER/machine/pte.h:93) is
 *     `_rwip(pfnum) || pg_m`. An always-zero WIP table means a page the guest
 *     wrote through the MMU, without the kernel's own software pg_m being set,
 *     looks clean and is dropped instead of written back.
 *
 * ND-05.009.4 section 16.17 and 16.20: real hardware keeps SEPARATE program and
 * data tables, RPGU/RWIP return the logical OR of the two, and ZPGU/ZWIP clear
 * the bit in both. A single table per kind is therefore behaviourally identical
 * for every one of the six instructions, so that is what this keeps.
 *
 * Same sections: only the low 25 bits of the page number are significant, and
 * reading a bit that represents non-existing memory gives zero.
 */

#ifndef ND500_PAGE_BITS_H
#define ND500_PAGE_BITS_H

#include <stdint.h>

struct Nd500Machine;

/* Which table an operation addresses. */
typedef enum {
    ND500_PAGE_TABLE_PGU = 0,
    ND500_PAGE_TABLE_WIP = 1,
} Nd500PageTable;

/* Record a physical access. Sets the PGU bit for the page holding phys_addr,
 * and the WIP bit as well when is_write is non-zero. Silently ignores an
 * address outside physical memory. This is the hardware "set automatically"
 * side; call it once per successful translation. */
void nd500_page_bits_mark(struct Nd500Machine* m, uint32_t phys_addr, int is_write);

/* Read one bit. Returns 0 or 1; 0 for a page outside physical memory. */
uint32_t nd500_page_bits_read_bit(struct Nd500Machine* m, Nd500PageTable table, uint32_t page);

/* Read a 16-bit group. `group` is a page number divided by 16; bit k of the
 * result is page (group * 16 + k). Pages outside physical memory read as 0.
 *
 * INFERRED, NOT SPECIFIED: ND-05.009.4 defines the group form as "physical page
 * number/16" but never states which end of the 16-bit result holds the lowest
 * page. Lowest page in the least significant bit is the choice here. NDIX does
 * not exercise it - kernel/MASTER/machine/locore.c:1218 and :1241 use only the
 * single-bit form (`bi1 rwip` / `bi1 rpgu`) - so nothing currently depends on
 * the order being right. Settle it against microcode before trusting it. */
uint32_t nd500_page_bits_read_group(struct Nd500Machine* m, Nd500PageTable table, uint32_t group);

/* Clear one bit. A page outside physical memory is ignored. */
void nd500_page_bits_clear_bit(struct Nd500Machine* m, Nd500PageTable table, uint32_t page);

/* Clear the whole table. */
void nd500_page_bits_clear_all(struct Nd500Machine* m, Nd500PageTable table);

/* Release the bitmaps (does not touch guest RAM). Called when a machine is
 * destroyed or its memory re-sized; they rebuild lazily from memory_size. */
void nd500_page_bits_reset(struct Nd500Machine* m);

#endif /* ND500_PAGE_BITS_H */
