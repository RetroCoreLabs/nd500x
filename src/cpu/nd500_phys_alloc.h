/*
 * nd500_phys_alloc.h - physical page allocator
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * One allocator per machine, owning every 2 KB physical page frame. It replaces
 * two ad-hoc schemes that could not coexist:
 *
 *   - the DOM loader's fixed base (phys 0x800) plus a bump cursor, which placed
 *     EVERY loaded domain on the same physical pages, so a nested DOM overwrote
 *     its caller's resident image and the shell had to memcpy all of physical
 *     memory out and back around a UECOM call;
 *   - the MON segment allocator's monotonic watermark, which had no record of
 *     what the loader had taken and therefore re-derived a starting point by
 *     scanning guest page tables ("stop at the first zero PTE"). That scan is
 *     unsound for a sparse table and could not see PS_ADI data pages at all.
 *
 * Ownership instead of derivation: every page carries an owner tag, so the
 * allocator always knows what is in use without consulting guest-visible
 * tables, and a release frees exactly the pages that were taken.
 *
 * ARENAS make that release safe when domains nest. An arena is a LIFO scope:
 * push one before loading a domain, and popping it frees precisely the pages
 * allocated inside it. A domain that starts another domain (MON 317B UECOM, or
 * a new command) pushes an inner arena, so the inner program's pages come from
 * elsewhere in memory - the caller's image stays intact and both domains are
 * live at the same time. Pages reserved as PERMANENT (the kernel image, any
 * region placed before the first push) are never freed by a pop.
 */

#ifndef ND500_PHYS_ALLOC_H
#define ND500_PHYS_ALLOC_H

#include <stdint.h>

struct Nd500Machine;

/* Owner tags. Arena ids start at ND500_PHYS_ARENA_FIRST and increase with
 * nesting depth, so "owner >= id" means "allocated at or inside arena id". */
#define ND500_PHYS_FREE         0u
#define ND500_PHYS_PERM         1u
#define ND500_PHYS_ARENA_FIRST  2u

/* Release the allocator's bookkeeping (does not touch guest RAM). Called when a
 * machine is destroyed or re-created; the allocator re-initialises itself from
 * m->memory_size on the next use. */
void nd500_phys_alloc_reset(struct Nd500Machine* m);

/* Allocate `count` CONTIGUOUS page frames, charged to the innermost open arena
 * (or PERMANENT if none is open). Returns the base PFN, or 0 on failure - PFN 0
 * is never allocated because the PTE format uses PFN==0 to mean "not present".
 * With zero != 0 the pages are cleared before being handed out; page tables and
 * fresh data pages must use it, since guest RAM is only zeroed at power-on and a
 * recycled page otherwise still holds the previous owner's bytes. */
uint32_t nd500_phys_alloc_pages(struct Nd500Machine* m, uint32_t count, int zero);

/* Return pages to the pool. Freeing a page that is already free is a no-op. */
void nd500_phys_free_pages(struct Nd500Machine* m, uint32_t base_pfn, uint32_t count);

/* Mark an existing physical byte range as PERMANENTLY in use, so it is never
 * handed out and never reclaimed by an arena pop. Use for anything placed
 * outside the allocator (a kernel image loaded at a fixed address, an MMU setup
 * script's hand-built tables). Returns 0 on success, -1 if the range lies
 * outside memory or overlaps a page already owned by an arena. */
int nd500_phys_reserve(struct Nd500Machine* m, uint32_t byte_base, uint32_t byte_len);

/* Declare that physical memory from `byte_base` upward belongs to the GUEST, so
 * the allocator must never hand out a page there. Pass 0 to lift the limit.
 *
 * A guest that does its own page management (NDIX: FE_INIT hands it sfree..sphys
 * and it allocates, frees and ZEROES inside that range at will) has no idea the
 * emulator also allocates. Before this existed the two pools overlapped: NDIX was
 * told its pool began at 0x100000 while the emulator's demand-grown segments were
 * being placed from 0x180000 up, inside it. Under memory pressure NDIX recycled
 * and zeroed those pages, destroying the segment-8 page table.
 *
 * The limit is set by whoever states the contract - fe_init(), with the same
 * number it writes into IR_SFREE - so the two cannot drift apart. Allocation
 * above the limit fails outright (returns 0) instead of quietly taking guest
 * memory. nd500_phys_reserve() is NOT limited: an image loaded at a fixed high
 * address is a placement, not an allocation. */
void nd500_phys_set_guest_pool_base(struct Nd500Machine* m, uint32_t byte_base);
uint32_t nd500_phys_guest_pool_base(struct Nd500Machine* m);

/* Open a new allocation scope. Returns its arena id (>= ND500_PHYS_ARENA_FIRST),
 * or 0 if the allocator is unavailable or nesting is exhausted. */
uint32_t nd500_phys_arena_push(struct Nd500Machine* m);

/* Close the scope opened by `id` and free every page allocated at or inside it.
 * Popping an id that is not open is a no-op, so an error path may pop twice. */
void nd500_phys_arena_pop(struct Nd500Machine* m, uint32_t id);

/* Number of pages currently free / total, for diagnostics and tests. */
uint32_t nd500_phys_pages_free(struct Nd500Machine* m);
uint32_t nd500_phys_pages_total(struct Nd500Machine* m);

/* Owner tag of a page, for diagnostics and tests. */
uint32_t nd500_phys_page_owner(struct Nd500Machine* m, uint32_t pfn);

/* Highest page frame the allocator has ever handed out, +1 (0 if none). This is
 * the number that matters when deciding how much physical memory to withhold
 * from a guest that does its own allocation: demand segments GROW on fault
 * (nd500_segment_grow_on_fault), so the initial allocation is not the footprint.
 * NDIX is told it owns sfree..sphys and will reuse anything in that range the
 * emulator has quietly taken. */
uint32_t nd500_phys_high_water_pfn(struct Nd500Machine* m);

/* Print pages used/free and the high-water mark to stderr. Called at machine
 * teardown when ND500X_PHYSDBG is set; safe to call at any time. */
void nd500_phys_alloc_report(struct Nd500Machine* m);

#endif /* ND500_PHYS_ALLOC_H */
