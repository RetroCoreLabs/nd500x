/*
 * diag_pool_snapshot.c - read a captured MPM-5 pool without booting anything
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHY THIS EXISTS
 *
 * Every observation on the ND-5000 / octobus lane costs a full SINTRAN boot -
 * roughly eight minutes to the monitor prompt. One investigation produced 132
 * driver scripts and 251 logs for that reason, and the cost is what turns a
 * two-minute question into an hour.
 *
 * The expensive part is the boot, not the subject. Everything that lane argues
 * about lives in the shared pool: the mailbox and its per-CPU extension blocks,
 * the process message blocks, the physical segment table, the Domain
 * Information Tables and the swapper's own segment descriptors. Capture the
 * pool once at the point of interest and the same state can be read, and
 * re-read, in milliseconds.
 *
 * Capturing (in the nd100x checkout, which owns the bridge):
 *
 *     MFBUS_SNAPSHOT_PATH=<file> MFBUS_SNAPSHOT_AT_MON=15 ./build/bin/nd100x ...
 *
 * MFBUS_SNAPSHOT_AT_MON is the monitor call to capture at, counted per CPU. The
 * run logs the file it wrote, or logs that the write failed; it never fails
 * silently, because a snapshot that silently did not happen is the same trap as
 * a diagnostic that silently stopped reporting.
 *
 * MEASURED: a capture taken at monitor call 15 of a PLACE-DOMAIN CPU-STAT run
 * reproduced the swapper's segment descriptors exactly - psn 11 holding 11
 * pages, psn 12 holding 4 with STATE 7, psn 13 empty - in 7 milliseconds. The
 * live run that first produced those numbers took eight minutes.
 *
 * WHAT IT DOES NOT DO. It snapshots the POOL. The ND-100's own memory,
 * registers and disk are not in it, so a question about SINTRAN's side still
 * needs a boot. This removes the boot for ND-500-side and mailbox questions,
 * which is where it was being spent.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ndbus_pool.h"
#include "ndbus_msgqueue.h"

/* The pool size the octobus configuration uses. A snapshot taken from a pool of
 * a different size is REFUSED by the loader rather than misread, so a mismatch
 * here produces an error and not a page of confident wrong numbers. */
#define DIAG_POOL_BYTES (8u * 1024u * 1024u)

/* The swapper's per-segment descriptors. Logical 0x08038000 on the ND-500 side;
 * the stride is 100 DECIMAL bytes, anchored by the carve's own citation of PSN
 * 14 at 0x08038578, since 0x08038000 + 14*100 is exactly that. The packed STATE
 * word sits at descriptor offset 4 and decodes as (halfword >> 10) & 0xF.
 *
 * The POOL offset of the table depends on where the swapper's data segment is
 * mapped, so it is given rather than guessed: pass it on the command line. The
 * value measured on the runs this harness was written for is the default below.
 */
#define DIAG_DESC_STRIDE 100u
#define DIAG_DESC_DEFAULT_PA 0x0005F3E8u   /* psn 10's descriptor, measured */

static void dump_descriptors(const NdbusPool *pool, uint32_t base_pa, uint32_t first_psn)
{
    printf("\nSwapper segment descriptors (base pa=0x%08X, stride %u decimal)\n",
           base_pa, DIAG_DESC_STRIDE);
    printf("  psn   pa          pages  desc+4  STATE  grow  (STATE in {13,14,15} may grow)\n");
    for (uint32_t i = 0; i < 8u; i++)
    {
        uint32_t psn = first_psn + i;
        uint32_t pa = base_pa + i * DIAG_DESC_STRIDE;
        if (pa + 8u > pool->size)
        {
            break;
        }
        uint32_t pages = ndbus_pool_read32(pool, pa);
        uint32_t w4 = ndbus_pool_read16(pool, pa + 4u);
        uint32_t state = (w4 >> 10) & 0x0Fu;
        printf("  %3u   0x%08X  %5u  0x%04X  0x%X    %d%s\n",
               psn, pa, pages, w4, state,
               (state >= 13u && state <= 15u) ? 1 : 0,
               (pages == 0u && w4 == 0u) ? "   <- EMPTY, no descriptor built" : "");
    }
}

static void dump_message(const NdbusPool *pool, uint32_t msg_byte)
{
    if (msg_byte + 0x90u > pool->size)
    {
        printf("\nmessage block 0x%06X is outside the pool\n", msg_byte);
        return;
    }
    printf("\nMessage block 0x%06X\n", msg_byte);
    printf("  N5STA=0x%04X SENDE=0x%04X X5CPU=0x%04X X5ACT=0x%04X MICFU=0x%04X\n",
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_N5STA * 2u),
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_SENDE * 2u),
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_X5CPU * 2u),
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_X5ACT * 2u),
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_MICFU * 2u));
    printf("  STOPR=0x%04X TRAPN=0x%04X\n",
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_STOPR * 2u),
           ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_TRAPN * 2u));

    /* SWPFU AND SWPST BELONG TO THE SWAPPER'S BLOCK ALONE, and printing them
     * for any block invites exactly the confidently-wrong reading this harness
     * exists to avoid: on a domain's block those offsets hold ordinary process
     * data and decode as plausible nonsense. Measured: a domain block at
     * 0x8E30 showed SWPFU=0x3D65 SWPST=0x5BE8, neither of which is a swap
     * function or a status. So say which block this is before showing them. */
    uint32_t x5 = ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_X5CPU * 2u);
    if (x5 == 0u)
    {
        printf("  SWPFU=0x%04X SWPST=0x%04X   (X5CPU 0 - the swapper's own block)\n",
               ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_SWPFU * 2u),
               ndbus_pool_read16(pool, msg_byte + NDBUS_MSG_SWPST * 2u));
        printf("  SWPFU 0 ESWPFATAL, 1 LNEWSWAP, 2 LSWPAGE, 4 LALLOPAGE, "
               "5 LDATREADY, 6 LCLTSB;\n");
        printf("  a NON-ZERO SWPST is read by SINTRAN as an error answer from "
               "the swapper.\n");
    }
    else
    {
        printf("  SWPFU/SWPST NOT SHOWN: X5CPU %u is not the swapper, and those\n"
               "  offsets carry ordinary process data on a domain's block.\n",
               x5);
    }
}

static void dump_pst(const NdbusPool *pool, uint32_t pstp_pa)
{
    if (pstp_pa == 0u || pstp_pa + 24u * 4u > pool->size)
    {
        return;
    }
    printf("\nPhysical segment table at pa=0x%08X (32-bit entries on the ND-5000)\n", pstp_pa);
    for (uint32_t row = 0; row < 3u; row++)
    {
        printf("  PST[%2u..%2u]:", row * 8u, row * 8u + 7u);
        for (uint32_t k = 0; k < 8u; k++)
        {
            printf(" %08X", ndbus_pool_read32(pool, pstp_pa + (row * 8u + k) * 4u));
        }
        printf("\n");
    }
    printf("  A ZERO entry is a page fault (ND-05.009.4 section 4.3), not page 0.\n");
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: %s <pool-snapshot> [desc_pa [first_psn [msg_byte [pstp_pa]]]]\n",
               argv[0]);
        printf("  desc_pa    pool offset of a segment descriptor (default 0x%08X)\n",
               DIAG_DESC_DEFAULT_PA);
        printf("  first_psn  the psn that descriptor belongs to (default 10)\n");
        printf("  msg_byte   a message block to decode, e.g. 0x8E30\n");
        printf("  pstp_pa    pool offset of the physical segment table\n");
        printf("\nCapture a snapshot from the nd100x side with\n");
        printf("  MFBUS_SNAPSHOT_PATH=<file> MFBUS_SNAPSHOT_AT_MON=<n>\n");
        return 2;
    }

    NdbusPool pool;
    if (!ndbus_pool_create(&pool, DIAG_POOL_BYTES))
    {
        printf("could not allocate a %u byte pool\n", DIAG_POOL_BYTES);
        return 1;
    }

    if (!ndbus_pool_snapshot_load(&pool, argv[1]))
    {
        /* REFUSED, not misread. The loader checks a magic and the pool size, so
         * this is a wrong or truncated file rather than a silently misaligned
         * one - which is the whole reason the header exists. */
        printf("snapshot REFUSED: %s is missing, is not a pool snapshot, or was "
               "taken from a pool whose size is not %u bytes\n",
               argv[1], DIAG_POOL_BYTES);
        ndbus_pool_destroy(&pool);
        return 1;
    }
    printf("loaded %s: %u bytes\n", argv[1], pool.size);

    uint32_t desc_pa = (argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 0)
                                  : DIAG_DESC_DEFAULT_PA;
    uint32_t first_psn = (argc > 3) ? (uint32_t)strtoul(argv[3], NULL, 0) : 10u;
    dump_descriptors(&pool, desc_pa, first_psn);

    if (argc > 4)
    {
        dump_message(&pool, (uint32_t)strtoul(argv[4], NULL, 0));
    }
    if (argc > 5)
    {
        dump_pst(&pool, (uint32_t)strtoul(argv[5], NULL, 0));
    }

    ndbus_pool_destroy(&pool);
    return 0;
}
