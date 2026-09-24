/*
 * test_tlb_shootdown.c - the per-CPU translation cache and the cross-CPU
 *                        shootdown.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * With ONE CPU, flushing the cache inline on a write to a page-table page is a
 * correct crutch. With several it is broken: CPU 1 edits a page table and only
 * CPU 1's cache is dropped, leaving every other CPU running on translations that
 * edit just invalidated. The fix is a registry - a write sets flush_pending in
 * every attached cache, and each CPU drops its own at the top of its next
 * instruction.
 *
 * These tests drive nd500_tlb.h directly. They do not need a machine or an MMU
 * walk: what is under test is the registry, the shared bitmap and the flag, and
 * a test that needed a whole guest to reach them would not name the failure.
 */

#include <stdio.h>
#include <string.h>

#include "../src/cpu/nd500_tlb.h"

static int s_failed = 0;
static int s_checks = 0;

#define CHECK(cond, msg)                                                                 \
    do                                                                                   \
    {                                                                                    \
        s_checks++;                                                                      \
        if (!(cond))                                                                     \
        {                                                                                \
            printf("  FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);                   \
            s_failed++;                                                                  \
        }                                                                                \
    } while (0)

/* A cache primed by hand. nd500_tlb_init_once() reads the settings catalog, and
 * this test deliberately links no settings; priming here keeps the test to the
 * registry and the flag, which is what it is about. */
static void prime(Nd500Tlb *tlb)
{
    memset(tlb, 0, sizeof(*tlb));
    tlb->init = 1;
    tlb->on   = 1;
    for (uint32_t i = 0; i < ND500_TLB_SIZE; i++)
    {
        tlb->entries[i].tag = ND500_TLB_EMPTY;
    }
}

/* Put one recognisable entry in a cache so a flush is observable. */
static void fill_one(Nd500Tlb *tlb, uint32_t tag)
{
    tlb->entries[nd500_tlb_slot(tag)].tag = tag;
}

static bool holds(const Nd500Tlb *tlb, uint32_t tag)
{
    return tlb->entries[nd500_tlb_slot(tag)].tag == tag;
}

int main(void)
{
    printf("ND-500 TLB shootdown tests\n");
    printf("==========================\n\n");

    static Nd500Tlb cpu_a;
    static Nd500Tlb cpu_b;
    prime(&cpu_a);
    prime(&cpu_b);

    /* flush_pending must sit alone on a cache line, or the CPU threads
     * ping-pong one line on every page-table write and that costs more than the
     * flush it is avoiding. */
    CHECK((size_t)&cpu_a.flush_pending % ND500_CACHE_LINE == 0,
          "flush_pending is cache-line aligned");
    CHECK(offsetof(Nd500Tlb, entries) >= ND500_CACHE_LINE,
          "the entries do not share flush_pending's line");

    CHECK(nd500_tlb_register(&cpu_a), "CPU A attaches");
    CHECK(nd500_tlb_register(&cpu_b), "CPU B attaches");
    CHECK(nd500_tlb_register(&cpu_a), "attaching twice is accepted and not duplicated");
    CHECK(g_nd500_tlb_active != 0, "the hot-path gate is on while a cache is on");

    /* A walk on CPU A notes the page it read a table from. The bitmap is
     * SHARED - it describes the memory, not the CPU - so CPU B's write to that
     * same page must be seen. */
    const uint32_t table_phys = 0x48A000u;
    const uint32_t other_phys = 0x100000u;
    nd500_tlb_note_xlat_page(table_phys);

    uint32_t tag_a = nd500_tlb_tag(0x20000u, 3, 0);
    uint32_t tag_b = nd500_tlb_tag(0x30000u, 4, 1);
    fill_one(&cpu_a, tag_a);
    fill_one(&cpu_b, tag_b);
    CHECK(holds(&cpu_a, tag_a), "CPU A has a cached translation");
    CHECK(holds(&cpu_b, tag_b), "CPU B has a cached translation");

    /* A write somewhere that is not a page table changes nothing. */
    nd500_tlb_on_phys_write(other_phys);
    CHECK(cpu_a.flush_pending == 0, "an ordinary write does not signal CPU A");
    CHECK(cpu_b.flush_pending == 0, "nor CPU B");

    /* A write to the page-table page signals EVERY attached CPU, including the
     * writer - which CPU's thread is doing the writing is not even knowable
     * here, and the writer can hold a stale translation just as easily. */
    nd500_tlb_on_phys_write(table_phys);
    CHECK(cpu_a.flush_pending != 0, "the page-table write signals CPU A");
    CHECK(cpu_b.flush_pending != 0, "and CPU B");
    CHECK(holds(&cpu_a, tag_a), "but nothing is flushed inline");
    CHECK(holds(&cpu_b, tag_b), "on either CPU - the other thread's entries are not touched");

    /* Each CPU drops its own cache at the top of its next instruction. */
    CHECK(nd500_tlb_take_pending(&cpu_a), "CPU A takes the pending flush");
    CHECK(!holds(&cpu_a, tag_a), "and its cache is empty");
    CHECK(cpu_a.flush_pending == 0, "the flag is consumed");
    CHECK(!nd500_tlb_take_pending(&cpu_a), "a second check finds nothing");
    CHECK(holds(&cpu_b, tag_b), "CPU B is untouched until it checks for itself");
    CHECK(nd500_tlb_take_pending(&cpu_b), "CPU B takes its own pending flush");
    CHECK(!holds(&cpu_b, tag_b), "and its cache is empty too");

    /* A byte anywhere in the 2KB page counts, not just the first one. */
    fill_one(&cpu_a, tag_a);
    nd500_tlb_on_phys_write(table_phys + 2047u);
    CHECK(cpu_a.flush_pending != 0, "the last byte of the page signals too");
    (void)nd500_tlb_take_pending(&cpu_a);
    fill_one(&cpu_a, tag_a);
    nd500_tlb_on_phys_write(table_phys + 2048u);
    CHECK(cpu_a.flush_pending == 0, "the first byte of the NEXT page does not");

    /* The coarse path - image load, the debugger, the allocator - flushes every
     * cache immediately, because all of them can change what ANY CPU would
     * translate. */
    fill_one(&cpu_a, tag_a);
    fill_one(&cpu_b, tag_b);
    nd500_mmu_tlb_flush();
    CHECK(!holds(&cpu_a, tag_a), "the coarse flush empties CPU A immediately");
    CHECK(!holds(&cpu_b, tag_b), "and CPU B");

    /* DCTSB / PCTSB are one CPU's own instruction and clear only that CPU. */
    fill_one(&cpu_a, tag_a);
    fill_one(&cpu_b, tag_b);
    nd500_tlb_flush_one(&cpu_a);
    CHECK(!holds(&cpu_a, tag_a), "flush_one empties the named cache");
    CHECK(holds(&cpu_b, tag_b), "and leaves the other CPU alone");

    /* A detached CPU must stop receiving shootdowns - and the registry must not
     * keep a pointer to it. */
    nd500_tlb_unregister(&cpu_b);
    cpu_b.flush_pending = 0;
    nd500_tlb_on_phys_write(table_phys);
    CHECK(cpu_a.flush_pending != 0, "the attached CPU still receives the shootdown");
    CHECK(cpu_b.flush_pending == 0, "the detached one does not");
    nd500_tlb_unregister(&cpu_b); /* must be safe twice */
    (void)nd500_tlb_take_pending(&cpu_a);

    /* The registry is bounded, and the refusal is LOUD: a CPU that silently
     * failed to attach would never receive a shootdown and would run on stale
     * translations forever, which surfaces much later as impossible memory
     * contents. */
    static Nd500Tlb extra[ND500_TLB_MAX_CPUS + 2];
    int             attached = 1; /* cpu_a is still on */
    for (int i = 0; i < ND500_TLB_MAX_CPUS + 2; i++)
    {
        prime(&extra[i]);
        if (nd500_tlb_register(&extra[i]))
        {
            attached++;
        }
    }
    CHECK(attached == ND500_TLB_MAX_CPUS, "the registry fills to its bound and refuses more");

    /* NULL is safe everywhere: a CPU whose allocation failed has tlb == NULL and
     * simply walks every access. */
    CHECK(!nd500_tlb_register(NULL), "registering NULL is refused");
    CHECK(!nd500_tlb_take_pending(NULL), "taking on NULL is safe");
    nd500_tlb_flush_one(NULL);
    nd500_tlb_unregister(NULL);
    CHECK(true, "flushing and detaching NULL are safe");

    printf("\n%d check(s), %d failed\n", s_checks, s_failed);
    if (s_failed != 0)
    {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
