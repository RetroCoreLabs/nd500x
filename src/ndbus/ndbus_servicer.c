/**
 * @file ndbus_servicer.c
 * @brief The ND-5000 mailbox servicer. See ndbus_servicer.h for what it is and
 *        where every part of it was ported from.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndbus_servicer.h"

#include <stdio.h>
#include <string.h>

#include "ndbus_lock.h"
#include "ndbus_mailbox.h"

/** Value stored in X5SEM while the servicer holds it. The taken value is
 *  host-internal - RetroCore's IServicerHost says so - and only zero means free. */
#define SEM_TAKEN_VALUE 1u

/** Spins on X5SEM before answering unlocked anyway. LOCK_QUE is an unbounded
 *  test-and-set spin in the microcode; an emulator that hangs there hangs the
 *  whole machine, so the count is bounded and the give-up is logged. RetroCore
 *  uses 10000 at Nd500MicrocodeServicer.cs:4211. */
#define SEM_SPIN_LIMIT 10000

/* ---- the trap-config write watch -------------------------------------------
 *
 * SINTRAN writes a domain's trap-control fields into a process control block with
 * PHYSWR transfers before starting anything, and the containing 256-byte block is
 * the Domain Information Table's base. These are the offsets INSIDE that block
 * that those writes cover, from RetroCore Nd500MicrocodeServicer.cs
 * DitTrapConfigFirstOffset / DitTrapConfigLastOffset.
 */
#define DIT_TRAP_CONFIG_FIRST_OFFSET 0x96u
#define DIT_TRAP_CONFIG_LAST_OFFSET  0xC7u

/** Bytes of a process control block, which is also a DIT entry. */
#define PCB_BYTES 0x100u

/** Bytes per X5FIF ring slot. Verified in RetroCore against GIVEINT's own
 *  arithmetic (slot = ringbase + fill * 4). */
#define RING_SLOT_BYTES 4u

static void answer_message_in_place(NdbusServicer *sv, uint32_t msg_byte, uint16_t answer);

static void servicer_log(const NdbusServicer *sv, const char *message)
{
    if (sv->host.log != NULL)
    {
        sv->host.log(sv->host.ctx, message);
    }
}

/** One 16-bit word at a pool byte offset, big-endian, as the ND-100 sees it. */
static uint16_t read16(const NdbusServicer *sv, uint32_t byte_offset)
{
    return ndbus_pool_read16(sv->pool, byte_offset);
}

static bool write16(NdbusServicer *sv, uint32_t byte_offset, uint16_t value)
{
    return ndbus_pool_write16(sv->pool, byte_offset, value);
}

/** A message word's byte offset. Message fields are numbered in WORDS by the ND
 *  documents and by ndbus_msgqueue.h, so every access goes through this. */
static uint32_t msg_word(uint32_t msg_byte, uint32_t word)
{
    return msg_byte + (word * 2u);
}

/**
 * Resolve a chain LINK value to a pool byte offset.
 *
 * Identity, masked to 24 bits. This is RetroCore's octobus override
 * (OctobusND5000Station.cs:2150, `_mpm.Start + (linkValue & 0xFFFFFF)`) with the
 * pool base factored out, because every address in this file is already
 * pool-relative. The 24-bit mask is RetroCore's: the top byte of the stored
 * pointer is not part of the offset.
 */
static uint32_t resolve_link(uint32_t link_value)
{
    return link_value & 0xFFFFFFu;
}

/** The inverse: the value stored in a ring slot for a message at this offset.
 *  RetroCore OctobusND5000Station.cs:2154. */
static uint32_t to_link(uint32_t msg_byte)
{
    return msg_byte & 0xFFFFFFu;
}

bool ndbus_servicer_init(NdbusServicer *sv, NdbusPool *pool, const NdbusServicerHost *host)
{
    if (sv == NULL || pool == NULL)
    {
        return false;
    }

    memset(sv, 0, sizeof(*sv));
    sv->pool = pool;
    if (host != NULL)
    {
        sv->host = *host;
    }
    sv->micro_version = NDBUS_SERVICER_MICRO_VERSION_DEFAULT;
    sv->cpu_parameter = NDBUS_SERVICER_CPU_PARAMETER_DEFAULT;
    return true;
}

bool ndbus_servicer_set_header(NdbusServicer *sv, uint32_t header_byte)
{
    if (sv == NULL)
    {
        return false;
    }

    sv->header_base = header_byte;
    return true;
}

bool ndbus_servicer_set_context_area(NdbusServicer *sv, uint32_t area_byte)
{
    if (sv == NULL)
    {
        return false;
    }

    sv->context_area_base = area_byte;
    return true;
}

bool ndbus_servicer_set_cpu_identity(NdbusServicer *sv, uint16_t micro_version,
                                     uint16_t cpu_parameter)
{
    if (sv == NULL)
    {
        return false;
    }

    /* A ZERO IS NOT AN ANSWER. An unloaded or unpatched control store reads 0, and
     * a model byte with no verified CPUPAR mapping yields 0 - reporting either
     * would relabel the machine silently. Keep what is there instead. */
    if (micro_version != 0u)
    {
        sv->micro_version = micro_version;
    }
    if (cpu_parameter != 0u)
    {
        sv->cpu_parameter = cpu_parameter;
    }
    return true;
}

bool ndbus_servicer_set_pst_base(NdbusServicer *sv, uint32_t pst_byte)
{
    if (sv == NULL)
    {
        return false;
    }

    sv->pst_base = pst_byte;
    return true;
}

/**
 * GIVEINT's answer-ring insert, ported from Nd500MicrocodeServicer.cs
 * AnswerRingInsert: slot[X5FYL] := the answered message's pointer, then advance
 * X5FYL modulo X5MXF. X5HEN is the ND-100's drain index and is NOT touched.
 *
 * X5FIF IS A BYTE OFFSET, not a word address. SYS_DATAF at microcode 025636
 * copies header word 6 into srf[0o2002] with no shift and GIVEINT uses it
 * directly. A word address here puts every slot at half its true offset, which
 * lands back INSIDE the mailbox structure rather than faulting.
 */
static void answer_ring_insert(NdbusServicer *sv, uint32_t msg_byte)
{
    uint16_t hen = read16(sv, sv->header_base + NDBUS_MBX_X5HEN_WORD * 2u);
    uint16_t fyl = read16(sv, sv->header_base + NDBUS_MBX_X5FYL_WORD * 2u);
    uint16_t mxf = read16(sv, sv->header_base + NDBUS_MBX_X5MXF_WORD * 2u);
    uint32_t ring_hi = read16(sv, sv->header_base + NDBUS_MBX_X5FIF_WORD * 2u);
    uint32_t ring_lo = read16(sv, sv->header_base + (NDBUS_MBX_X5FIF_WORD + 1u) * 2u);
    uint32_t ring_value = (ring_hi << 16) | ring_lo;

    if (mxf == 0u || ring_value == 0u || ring_value == 0xFFFFFFFFu)
    {
        /* COUNTED ALWAYS, not only in a debug build. The insert is the one thing
         * that tells the ND-100 WHICH message was answered, so skipping it is not
         * a detail - RetroCore made this counter unconditional on 30-AUG-2026 for
         * exactly that reason. */
        sv->answer_ring_skipped_init++;
        if (sv->answer_ring_skipped_init == 1u)
        {
            char line[128];
            (void)snprintf(line, sizeof line,
                           "mailbox ring not initialised (X5MXF=%u X5FIF=0x%08X) - insert skipped",
                           (unsigned)mxf, (unsigned)ring_value);
            servicer_log(sv, line);
        }
        return;
    }

    uint16_t next_fyl = (uint16_t)((fyl + 1u >= mxf) ? 0u : (fyl + 1u));
    if (next_fyl == hen)
    {
        /* Ring full. The microcode skips the insert and STILL interrupts
         * (GIVEINT 025435 -> GIVEINT1), so the ND-100 drains on the interrupt.
         * Survivable, not invisible: if this ever fires the ND-100 is draining
         * slower than the answers arrive, which is a finding of its own. */
        sv->answer_ring_skipped_full++;
        if (sv->answer_ring_skipped_full == 1u)
        {
            char line[128];
            (void)snprintf(line, sizeof line,
                           "mailbox ring full (X5FYL=%u X5HEN=%u X5MXF=%u) - insert skipped, "
                           "answer still signalled",
                           (unsigned)fyl, (unsigned)hen, (unsigned)mxf);
            servicer_log(sv, line);
        }
        return;
    }

    uint32_t slot = resolve_link(ring_value) + ((uint32_t)fyl * RING_SLOT_BYTES);
    uint32_t stored = to_link(msg_byte);
    (void)write16(sv, slot, (uint16_t)(stored >> 16));
    (void)write16(sv, slot + 2u, (uint16_t)(stored & 0xFFFFu));
    (void)write16(sv, sv->header_base + NDBUS_MBX_X5FYL_WORD * 2u, next_fyl);
    sv->answer_ring_inserted++;
}

/* ---- the copy family ---------------------------------------------------------
 *
 * The B30 microcode dispatches DMEMRD/WR, RESIRD/WR, PHYSRD/WR and IMEMWR to ONE
 * direction-fixed copy engine over a single flat, byte-addressed window. The
 * D-space / I-space / physical / resident distinction is a space select on real
 * hardware and aliases in a flat model. Ported from RetroCore
 * Nd500MicrocodeServicer.cs PerformOctobusBlockCopy (line 1877).
 *
 * The parameter header is the calibrated three-word one, byte offsets from the
 * message base and NOT the word-numbered message fields:
 *     msg + 14   addrA, 32 bits - the ND-500 / target side (microcode SC3)
 *     msg + 18   addrB, 32 bits - the buffer side          (microcode SC7)
 *     msg + 22   byte count, 16 bits                       (microcode SC4)
 * A WRITE moves B to A; a READ moves A to B.
 */

/** Byte offset of addrA inside a copy-family message. */
#define COPY_ADDR_A_BYTE 14u
/** Byte offset of addrB inside a copy-family message. */
#define COPY_ADDR_B_BYTE 18u
/** Byte offset of the transfer's byte count inside a copy-family message. */
#define COPY_COUNT_BYTE  22u

/** Bytes per ND-500 page, for turning a physical segment table entry into an
 *  address. The PST entry holds the page in its low 14 bits; bits 15-14 are the
 *  mode. A zero entry means not present. */
#define PST_PAGE_BYTES 2048u
/** Mask selecting the page out of a physical segment table entry. */
#define PST_PAGE_MASK  0x3FFFu
/** ND-5000 physical-segment entries are WORD wide: mode in bits 31-30, page in
 *  29-0. The 14-bit ND-500 mask above belongs to the other generation. */
#define PST_PAGE_MASK_ND5000 0x3FFFFFFFu

static uint32_t read32(const NdbusServicer *sv, uint32_t byte_offset)
{
    uint32_t hi = read16(sv, byte_offset);
    uint32_t lo = read16(sv, byte_offset + 2u);
    return (hi << 16) | lo;
}

/**
 * Turn a physical segment plus an offset inside it into a pool byte offset.
 *
 * @return true when the segment table gave an answer. false when no table base is
 *         known or the entry is not present, in which case the caller must NOT
 *         quietly use the offset as a flat address without saying so.
 */
static bool resolve_physical_segment(const NdbusServicer *sv, uint16_t segment,
                                     uint32_t offset_in_segment, uint32_t *out_byte)
{
    if (sv->pst_base == 0u)
    {
        return false;
    }

    /* WORD ENTRIES, SO segment * 4. THE TABLE WIDTH IS A CPU-TYPE PROPERTY.
     *
     * This used to read a HALFWORD at segment * 2, citing the ND-500 microcode's
     * own `segment + segment` at 011460. That citation is from CONT-STORE-10611,
     * the 3022 ND-500 store - NOT the B30 this station fronts. The ND-500 has
     * halfword physical-segment and page-table entries (mode in bits 15-14, page
     * in 13-0); the ND-5000 has word-wide ones (mode in 31-30, page in 29-0). The
     * reference records the identical mix-up on its own side, where the width flag
     * was hardcoded to the ND-500 answer and "quietly told a 5000 it had halfword
     * tables".
     *
     * MEASURED 30-SEP-2026, and the two readings disagree on the same bytes: the
     * swapper's data page table dumped as 32-bit words gives entries 29-36 holding
     * pages 0x70-0x77, consecutive and sensible; the same bytes read as halfwords
     * alternate zero, 0x70, zero, 0x71, which is not a page table. The MMU walk in
     * nd500_mmu.c has always read this table at psn * 4. One table cannot have two
     * widths, and this was the copy that disagreed.
     *
     * The mode bits live in 31-30, so the page field is 30 bits and must NOT be
     * masked with the ND-500's 14-bit mask - that would fold a large page number
     * down to a small one that still looks plausible. */
    uint32_t entry = read32(sv, sv->pst_base + ((uint32_t)segment * 4u));
    uint32_t page = entry & PST_PAGE_MASK_ND5000;
    if (page == 0u)
    {
        return false;
    }

    *out_byte = (page * PST_PAGE_BYTES) + offset_in_segment;
    return true;
}

/**
 * One copy-family transfer.
 *
 * @param sv                 The servicer.
 * @param msg_byte           The message block.
 * @param write_to_nd500     true for a WRITE (buffer B -> target A), false for a
 *                           READ (target A -> buffer B).
 * @param a_is_segment_relative true when addrA is an offset inside the physical
 *                           segment named by MSWMC rather than a flat address.
 * @return true when the transfer was performed.
 */
static bool perform_block_copy(NdbusServicer *sv, uint32_t msg_byte, bool write_to_nd500,
                               bool a_is_segment_relative)
{
    uint32_t a_raw = read32(sv, msg_byte + COPY_ADDR_A_BYTE);
    uint32_t b_raw = read32(sv, msg_byte + COPY_ADDR_B_BYTE);
    uint32_t count = read16(sv, msg_byte + COPY_COUNT_BYTE);

    if (a_is_segment_relative)
    {
        /* PHYSRD/PHYSWR carry a physical segment in MSWMC and an offset inside it.
         * The B30's PHYSWR handler at 011453B walks the physical segment table
         * (011460 -> 007771, DP = PST base + segment * 2) before transferring, so
         * this is the microcode's behaviour and not an ND-500-only convention.
         *
         * A FAILED RESOLUTION MUST NOT SILENTLY BECOME A FLAT ADDRESS. That is the
         * defect this branch exists to avoid: RetroCore measured a 256-byte process
         * control block land 0xD9000 bytes from where the machine looks for it
         * because the segment was ignored, and the domain's first instruction fetch
         * then died with the correct capability sitting in memory the whole time.
         * So the fallback happens, because refusing the transfer outright would be
         * a different invention, but it is counted and logged. */
        uint16_t segment = read16(sv, msg_word(msg_byte, NDBUS_MSG_MSWMC));
        uint32_t resolved = 0u;
        if (resolve_physical_segment(sv, segment, a_raw, &resolved))
        {
            a_raw = resolved;
            sv->segment_resolved++;
        }
        else
        {
            sv->segment_unresolved++;
            if (sv->segment_unresolved == 1u)
            {
                char line[160];
                (void)snprintf(line, sizeof line,
                               "mailbox copy: segment-relative address UNRESOLVED seg=%u "
                               "off=0x%X - using it FLAT, which is very likely the wrong cell",
                               (unsigned)segment, (unsigned)a_raw);
                servicer_log(sv, line);
            }
        }
    }

    uint32_t src = write_to_nd500 ? b_raw : a_raw;
    uint32_t dst = write_to_nd500 ? a_raw : b_raw;

    /* Both ends have to be inside the pool. The station has no DMA fallback, so an
     * address outside it would otherwise read as zeros and write nowhere - which
     * looks exactly like a transfer that worked. */
    if (count != 0u &&
        (!ndbus_pool_contains(sv->pool, src, count) || !ndbus_pool_contains(sv->pool, dst, count)))
    {
        sv->copies_refused++;
        char line[160];
        (void)snprintf(line, sizeof line,
                       "mailbox copy: %u bytes 0x%06X -> 0x%06X leaves the pool - refused",
                       (unsigned)count, (unsigned)src, (unsigned)dst);
        servicer_log(sv, line);
        return false;
    }

    /* COPY EXACTLY THE BYTES ASKED FOR. NEVER ROUND THE COUNT UP TO A WORD.
     * RetroCore rounded to full 32-bit words here and measured what it cost: a
     * 2-byte capability write to a process control block also overwrote the NEXT
     * halfword, clearing a domain's data-write permission, after which its data
     * segment went read-only, a monitor-call answer could not be stored, the
     * resulting protection trap latched the wrong P, and the restart aimed the
     * domain at the swapper's code. One rounded count, four visible symptoms. The
     * count is authoritative: SINTRAN writes a 2-byte capability with a LIVE
     * capability in the next halfword, so the hardware cannot be rounding either. */
    uint32_t whole = count & ~1u;
    for (uint32_t i = 0; i < whole; i += 2u)
    {
        (void)write16(sv, dst + i, read16(sv, src + i));
    }

    if ((count & 1u) != 0u)
    {
        /* An odd trailing byte - SINTRAN does issue a count of 1. Read-modify-write
         * the containing halfword so the byte the count does NOT cover survives.
         * Big-endian, so an even address is the high byte of its halfword. */
        uint16_t src_word = read16(sv, src + whole);
        uint16_t dst_word = read16(sv, dst + whole);
        bool     high_half = (((dst + whole) & 1u) == 0u);
        uint16_t merged = high_half
                              ? (uint16_t)((src_word & 0xFF00u) | (dst_word & 0x00FFu))
                              : (uint16_t)((dst_word & 0xFF00u) | (src_word & 0x00FFu));
        (void)write16(sv, dst + whole, merged);
    }

    sv->copies_done++;
    sv->copy_bytes += count;

    /* SAY WHERE EVERY TRANSFER LANDED, for the first few. A count of arrivals says
     * a transfer happened; it cannot say whether it went where the guest meant, and
     * naming the cell is what turned "13 trap-config writes" into "13 page-table
     * entries" on the measured run of 30-SEP-2026. */
    if (sv->copies_done <= NDBUS_SERVICER_COPY_LOG_LIMIT)
    {
        char line[160];
        /* THE VALUE, NOT JUST THE ADDRESSES. Every transfer of a run reading the
         * same source cell is the shape of a loop that stages one value per
         * transfer, and in that case servicing the messages in a batch makes all of
         * them copy the LAST value written. Printing the first four bytes is what
         * tells those two apart. */
        (void)snprintf(line, sizeof line,
                       "mailbox copy #%lu: %u bytes 0x%06X -> 0x%06X, value 0x%04X%04X",
                       sv->copies_done, (unsigned)count, (unsigned)src, (unsigned)dst,
                       (unsigned)read16(sv, dst), (unsigned)read16(sv, dst + 2u));
        servicer_log(sv, line);
    }

    /* LEARN THE DIT BASE FROM THE WRITE ITSELF.
     *
     * The write has to be watched HERE, in the engine, and not in the PHYSWR arm of
     * the dispatch: RetroCore instrumented that arm first and measured zero hits
     * while its census counted thirteen PHYSWR, because the whole copy family is
     * routed to this one engine before the arm's own code runs.
     *
     * Aligned DOWN to the containing block rather than assumed to be zero, so this
     * stays correct if the table is ever placed elsewhere. The trap-config offsets
     * sit inside the first block, so the block that contains them IS the base. */
    if (write_to_nd500)
    {
        uint32_t offset_in_pcb = dst & (PCB_BYTES - 1u);
        if (offset_in_pcb >= DIT_TRAP_CONFIG_FIRST_OFFSET &&
            offset_in_pcb <= DIT_TRAP_CONFIG_LAST_OFFSET)
        {
            sv->dit_base = dst & ~(PCB_BYTES - 1u);
            sv->dit_writes_seen++;
        }
    }

    return true;
}

/**
 * Execute one MICFU.
 *
 * @return true when the code was understood, which decides ANSWER(3) against
 *         5ERANSWER(4). A code the real ND-5000 marks MSG_ILLEG must return
 *         false - that is the hardware's answer, not a gap in this port.
 */
static bool execute_micfu(NdbusServicer *sv, uint32_t msg_byte, uint16_t micfu)
{
    switch (micfu)
    {
    case NDBUS_MICFU_CACHE:
        /* 12B MSG_CACHE: conditional cache clears, selected by bits in one
         * halfword parameter. THERE IS NOTHING TO INVALIDATE HERE - no ND-5000
         * cache or TSB is modelled - so acknowledging is the complete correct
         * behaviour, and it is what the real microcode's MSG_END shows at B30
         * @015640. Ported from Nd500MicrocodeServicer.cs:3308.
         *
         * REJECTING IT ABORTS THE SWAPPER LOAD SILENTLY. RetroCore live-traced
         * SINTRAN sending this with parameter 147717B right after the swapper
         * image goes down - flush the caches before starting freshly written
         * code - and records that a 5ERANSWER here makes "Loading Swapper"
         * repeat forever with no error printed. Measured here 30-SEP-2026: it is
         * the ONLY code SINTRAN sent that this servicer declined. */
        return true;

    case NDBUS_MICFU_START:
    case NDBUS_MICFU_TRACO:
    case NDBUS_MICFU_STARTP0:
        /* 22B MSG_STARTP0: start process 0, the swapper. Answered the way the
         * microcode's MSG_END answers it, and the CPU is NOT started here.
         * RetroCore's carve of this same SINTRAN-L image found the swapper is
         * started through the MICRO-CLOCK / control-store path with no 22B
         * observed at all, and wiring 22B to a process start was tried twice
         * there and reverted. Ported from Nd500MicrocodeServicer.cs:3314.
         *
         * 23B 3START and 25B 3TRACO share this arm for the SAME reason and only as
         * the DECLINED path: a start the process host took never reaches here, it
         * returned earlier with the message left WAITING. Reaching this point means
         * no CPU took it, and the answer is the no-CPU answer. */
        return true;

    case NDBUS_MICFU_DMEMRD:  /* 10B */
    case NDBUS_MICFU_DMEMWR:  /* 11B */
        /* DATA-MEMORY TRANSFERS ARE NOT PHYSICAL TRANSFERS, and routing them to the
         * copy engine is a named defect rather than an approximation.
         *
         * Every other member of this family carries a physical, or segment-relative,
         * ND-500 address in addrA. DMEMRD and DMEMWR carry a LOGICAL DATA address in
         * the RUNNING PROCESS'S context - RP-P2-N500.NPL 130475 assigns it from
         * X.ISTRA - so it has to go through that process's data MMU, which only a
         * host with a CPU behind it can do.
         *
         * WHAT FALLING BACK COSTS, measured by RetroCore as its defect B12 and
         * pinned by MailboxCopyTests: SINTRAN asked for a file-name descriptor at
         * logical 0x08001478 during a forwarded MON 50B, the raw physical reading of
         * that address lies far outside the 8 MB window, the copy returned zeros,
         * SINTRAN got a null descriptor pointer and reported "SEGMENT NOT
         * MODIFIABLE". On the write half NC read its command line one byte at a time
         * for ever because each byte landed at physical 0x27F instead of the
         * process's own 0x27F, and the "NC:" prompt never appeared on any run.
         *
         * So with no logical-data host this REFUSES - 5ERANSWER(4) - and says so.
         * An answer of ANSWER(3) here would mean the physical fallback happened. */
        {
            char line[160];
            (void)snprintf(line, sizeof line,
                           "mailbox: MICFU %oB needs the running process's data MMU and no host "
                           "provides it - refused, NOT copied physically",
                           (unsigned)micfu);
            servicer_log(sv, line);
            sv->logical_copies_refused++;
        }
        return false;

    case NDBUS_MICFU_RESIRD:  /* 13B */
    case NDBUS_MICFU_PHYSRD:  /* 30B */
        /* READ members: target A -> buffer B. PHYSRD's A side is segment-relative;
         * RESIRD carries a flat address. */
        return perform_block_copy(sv, msg_byte, false, micfu == NDBUS_MICFU_PHYSRD);

    case NDBUS_MICFU_RESIWR:  /* 14B */
    case NDBUS_MICFU_PHYSWR:  /* 31B */
    case NDBUS_MICFU_IMEMWR:  /* 35B */
        /* WRITE members: buffer B -> target A. PHYSWR's A side is segment-relative.
         *
         * MEASURED 30-SEP-2026: 31B PHYSWR was the only code SINTRAN still sent
         * that this servicer declined once 12B CACHE was answered. */
        return perform_block_copy(sv, msg_byte, true, micfu == NDBUS_MICFU_PHYSWR);

    case NDBUS_MICFU_RMICV:
        /* 3RMICV answers TWO halfwords, microcode-verified at 015332-015334:
         * the version into the N500A slot (message word 7) and the CPU parameter
         * into word 10B. SINTRAN's RMVER consumes the version; the second
         * halfword is unconsumed but written because the microcode writes it.
         * Ported from Nd500MicrocodeServicer.cs:3366-3367. */
        (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_N500A), sv->micro_version);
        (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_SWRST), sv->cpu_parameter);
        return true;

    default:
        /* Everything else answers 5ERANSWER. For the codes the B30 image marks
         * MSG_ILLEG - 3SWMESS(05), 3FITRNSF(27B), 3RREG(20B), 3WREG(21B) - that
         * IS the ND-5000's answer. For the rest it is an honest "not ported";
         * micfu_counts says which ones actually arrive. */
        return false;
    }
}

bool ndbus_servicer_process_message(NdbusServicer *sv, uint32_t msg_byte)
{
    if (sv == NULL || sv->pool == NULL)
    {
        return false;
    }

    uint32_t sta_offset = msg_word(msg_byte, NDBUS_MSG_N5STA);
    uint16_t sta = read16(sv, sta_offset);
    uint16_t sta_pf_bits = (uint16_t)(sta & NDBUS_N5STA_PF_MASK);

    /* THE GATE. Only MSGN500(1) is ours. A node sitting in an ND-100-side
     * swapper state - SWPPI(6), PSWWA(7) - is walked past untouched, which is
     * what the real B30 microcode does; RetroCore pins that with the hardware
     * twin MailboxIdleTests.MailboxScan_ServicesOnlyMsgn500Nodes. Answering such
     * a node looks like a fix for the live swapper stall and is not one. */
    if ((uint16_t)(sta & NDBUS_N5STA_MASK) != NDBUS_N5STA_TO_ND500)
    {
        sv->nodes_not_ours++;
        return false;
    }

    uint16_t micfu = read16(sv, msg_word(msg_byte, NDBUS_MSG_MICFU));

    /* WAITING(2) while executing, power-fail bits preserved. */
    (void)write16(sv, sta_offset, (uint16_t)(sta_pf_bits | NDBUS_N5STA_WAITING));

    /* Tally here, at the single point where MICFU is known and BEFORE the
     * dispatch. RetroCore counted after its switch and every code whose arm
     * returned early was invisible in the histogram while other instruments said
     * it had arrived. */
    if (micfu < NDBUS_SERVICER_MICFU_COUNTS)
    {
        sv->micfu_counts[micfu]++;
    }
    sv->messages_processed++;

    /* EVERY MESSAGE, IN ARRIVAL ORDER, with the same fields RetroCore's own message
     * log prints - MICFU, X5CPU, STOPR and the message address. Deliberately the
     * same shape so the two emulators' traces can be diffed line for line, which is
     * the technique RetroCore records as having found its 3DEPR defect. Logged here,
     * at the single point where MICFU is known and before the dispatch, because an
     * arm that returns early would otherwise never be recorded. */
    {
        char line[160];
        (void)snprintf(line, sizeof line,
                       "RECV MICFU=%oB X5CPU=%u STOPR=0x%04X msg=0x%06X",
                       (unsigned)micfu,
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_X5CPU)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_STOPR)),
                       (unsigned)msg_byte);
        servicer_log(sv, line);
    }

    /* START-CLASS MESSAGES GO TO THE PROCESS HOST, NOT THE MICFU SWITCH.
     *
     * 22B STARTP0, 23B 3START and 25B 3TRACO are the start class; 23B and 25B share
     * MSG_START in the microcode. A host that TAKES the start leaves the message
     * WAITING and it is NOT answered here - the process's own stop answers it
     * later. Answering it here would tell SINTRAN a process had run and finished
     * when none had started.
     *
     * The context block address is the ND-05.017.01 Appendix A.1 formula:
     *     area + 400B + process_no * 400B
     * 400B = 256 bytes, and the leading 400B is the always-dummy first block. The
     * process number is X5CPU, which is ZERO-BASED - see the two-numberings warning
     * in ndbus_mailbox.h, because the mailbox's own CPUNO is one-based and passing
     * one where the other belongs puts a process's registers in its neighbour's
     * block without faulting.
     *
     * Ported from Nd500MicrocodeServicer.cs's StartProcess arm. */
    if (micfu == NDBUS_MICFU_STARTP0 || micfu == NDBUS_MICFU_START ||
        micfu == NDBUS_MICFU_TRACO)
    {
        sv->starts_seen++;

        /* Declare the base BEFORE the start, because the process's trap handler,
         * stack limits and top-of-stack come out of that table and a start that ran
         * without it would fault somewhere unrelated. Guarded on the write COUNT:
         * a base of zero is legitimate and indistinguishable from "not learned" by
         * value alone. */
        if (sv->dit_writes_seen > 0u && sv->host.declare_dit_base != NULL)
        {
            sv->host.declare_dit_base(sv->host.ctx, sv->dit_base);
            char line[128];
            (void)snprintf(line, sizeof line,
                           "mailbox: DIT base 0x%06X declared, learned from %lu trap-config "
                           "write(s)",
                           (unsigned)sv->dit_base, sv->dit_writes_seen);
            servicer_log(sv, line);
        }

        if (sv->host.start_process != NULL && sv->context_area_base != 0u)
        {
            uint16_t x5cpu = read16(sv, msg_word(msg_byte, NDBUS_MSG_X5CPU));
            uint32_t ctx_byte = sv->context_area_base + NDBUS_SERVICER_CTX_STRIDE +
                                ((uint32_t)x5cpu * NDBUS_SERVICER_CTX_STRIDE);

            if (sv->host.start_process(sv->host.ctx, msg_byte, micfu, ctx_byte))
            {
                sv->starts_taken++;
                /* REMEMBER THIS PROCESS'S MESSAGE. A trap in the started process is
                 * answered on this very message, in place - see
                 * ndbus_servicer_answer_trap_stop(). Recorded per X5CPU and never
                 * cleared, so a process that faults twice running still has one. */
                if (x5cpu < NDBUS_SERVICER_MAX_PROCESSES)
                {
                    sv->process_msg[x5cpu] = msg_byte;
                }
                /* Left WAITING on purpose, and NOT answered. Returning false says
                 * "nothing was answered"; the chain walk carries on regardless
                 * because it captured the link before serving this node. */
                return false;
            }
        }

        /* No host, no context area, or the host declined. Answer the way a machine
         * with no CPU behind the station answers, and COUNT it - a declined start
         * that is answered OK anyway is how a run looks healthy while no process
         * ever ran. */
        sv->starts_declined++;
        if (sv->starts_declined == 1u)
        {
            char line[160];
            (void)snprintf(line, sizeof line,
                           "mailbox: start MICFU %oB declined (context area 0x%06X) - answered "
                           "without starting anything",
                           (unsigned)micfu, (unsigned)sv->context_area_base);
            servicer_log(sv, line);
        }
    }

    bool understood = execute_micfu(sv, msg_byte, micfu);

    uint16_t answer = (uint16_t)(understood ? NDBUS_N5STA_ANSWER : NDBUS_N5STA_ERROR_ANSWER);
    if (!understood)
    {
        sv->messages_declined++;

        /* SAY WHICH CODE WAS DECLINED, once per code. This is how the next
         * micro-function to port gets chosen: SINTRAN sends what it sends, and a
         * 5ERANSWER that is never reported leaves the histogram readable only
         * from a debugger. Once per code, because a declined message is usually
         * retried forever and an unconditional line would bury the log. */
        if (micfu >= NDBUS_SERVICER_MICFU_COUNTS || sv->micfu_counts[micfu] == 1u)
        {
            char line[128];
            /* OCTAL WITH THE B, because every ND document writes MICFU octal and
             * a decimal number with a B on it names a different micro-function -
             * decimal 10 is 12B CACHE, not "10B". */
            (void)snprintf(line, sizeof line,
                           "mailbox: MICFU %oB (%u decimal) not ported - answered "
                           "5ERANSWER(4)",
                           (unsigned)micfu, (unsigned)micfu);
            servicer_log(sv, line);
        }
    }

    answer_message_in_place(sv, msg_byte, answer);

    return true;
}

/**
 * Write a message's answer status and ring the ND-100, the way the GIVEINT tail of
 * the microcode does.
 *
 * Extracted so the trap-stop answer below uses THE SAME tail as an ordinary
 * micro-function answer. Two copies of a semaphore-take, ring-insert and release
 * would be two places for the ring to drift out of step with X5FYL.
 *
 * The power-fail bits of N5STA (15-13) are preserved - they are not ours to clear.
 */
static void answer_message_in_place(NdbusServicer *sv, uint32_t msg_byte, uint16_t answer)
{
    uint32_t sta_offset = msg_word(msg_byte, NDBUS_MSG_N5STA);
    uint16_t sta_pf_bits = (uint16_t)(read16(sv, sta_offset) & NDBUS_N5STA_PF_MASK);

    if (sv->header_base == 0u)
    {
        /* No mailbox located: write the status and nothing else. A ring insert at
         * a guessed base corrupts the pool instead of failing. */
        sv->answer_no_header++;
        (void)write16(sv, sta_offset, (uint16_t)(sta_pf_bits | answer));
    }
    else
    {
        /* The GIVEINT tail, microcode 025422-025441: under X5SEM write N5STA,
         * insert into the ring at X5FYL, advance X5FYL mod X5MXF. Then the owner
         * sends the interrupt. */
        bool taken = false;
        for (int attempt = 0; attempt < SEM_SPIN_LIMIT; attempt++)
        {
            if (ndbus_tset16(sv->pool, sv->header_base + NDBUS_MBX_X5SEM_WORD * 2u,
                             SEM_TAKEN_VALUE))
            {
                taken = true;
                break;
            }
        }

        if (taken)
        {
            sv->answer_sem_taken++;
        }
        else
        {
            sv->answer_sem_not_taken++;
            /* REPORT THE VALUE. "never freed" alone cannot tell a semaphore the
             * ND-100 is holding from a header base that is not a header at all,
             * and those need opposite fixes. */
            char semline[128];
            (void)snprintf(semline, sizeof semline,
                           "mailbox answer: X5SEM at pool 0x%06X still holds 0x%04X - "
                           "answering unlocked",
                           (unsigned)(sv->header_base + NDBUS_MBX_X5SEM_WORD * 2u),
                           (unsigned)read16(sv, sv->header_base + NDBUS_MBX_X5SEM_WORD * 2u));
            servicer_log(sv, semline);
        }

        (void)write16(sv, sta_offset, (uint16_t)(sta_pf_bits | answer));
        answer_ring_insert(sv, msg_byte);

        if (taken)
        {
            (void)write16(sv, sv->header_base + NDBUS_MBX_X5SEM_WORD * 2u,
                          NDBUS_MBX_X5SEM_FREE);
        }
    }

    sv->messages_answered++;

    if (sv->host.answer_written != NULL)
    {
        sv->host.answer_written(sv->host.ctx, msg_byte);
    }
}

bool ndbus_servicer_answer_trap_stop(NdbusServicer *sv, uint16_t x5cpu, uint16_t trap_number,
                                     uint32_t trapping_pc, uint32_t trap_address,
                                     uint32_t mms_status, uint16_t physical_segment)
{
    if (sv == NULL || sv->pool == NULL || x5cpu >= NDBUS_SERVICER_MAX_PROCESSES)
    {
        return false;
    }

    sv->trap_stops_attempted++;

    /* ANSWER ON THE FAULTING PROCESS'S OWN MESSAGE, and keep one per process.
     *
     * RetroCore measured both ways this goes wrong with a single "active message"
     * field. CLEARED: the field is cleared as soon as any message is answered, so a
     * process that faults twice in a row - answered, restarted, faults again -
     * found it zero and the second post was REFUSED; on CPU-STAT that was two
     * faults at one PC one byte apart, took=true then took=false, and the refusal
     * crashed the CPU because a page fault is classified fatal. WRONG: with two
     * processes live the field can be non-zero and name the OTHER process, which is
     * what rejected 46 monitor calls. A "fall back only when it is zero" test
     * cannot catch the second case, because it never is zero.
     *
     * The microcode has no such ambiguity - it answers the process's OWN activation
     * message in place, one per process (MESSBUFF) - so this keeps an array indexed
     * by X5CPU and never clears it. */
    uint32_t msg_byte = sv->process_msg[x5cpu];
    if (msg_byte == 0u)
    {
        /* A DECLINE MUST NOT BE SILENT. The CPU posts 46B at the process's entry
         * and SINTRAN is told it stopped for no reason - STOPR and TRAPN both zero
         * - so the swapper has no fault address to page in and the monitor reports
         * that the swapper stopped. */
        sv->trap_stops_declined++;
        char line[160];
        (void)snprintf(line, sizeof line,
                       "mailbox trap-stop DECLINED trap=%oB pc=0x%08X addr=0x%08X - no message "
                       "recorded for X5CPU %u",
                       (unsigned)trap_number, (unsigned)trapping_pc, (unsigned)trap_address,
                       (unsigned)x5cpu);
        servicer_log(sv, line);
        return false;
    }

    /* The header is identical on both generations; only the trap-dependent area
     * below differs. STOPR = 2 is TRAPCODE. */
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_STOPR), NDBUS_STOPR_TRAPCODE);

    /* The saved P goes in TWICE - halfwords 0o12-0o13 and again 0o14-0o15. */
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_NUMPA), (uint16_t)(trapping_pc >> 16u));
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_MCNO), (uint16_t)(trapping_pc & 0xFFFFu));
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_MSWMC), (uint16_t)(trapping_pc >> 16u));
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_MSWMC + 1u), (uint16_t)(trapping_pc & 0xFFFFu));

    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_TRAPN), trap_number);

    /* THE B30 RECORD, NOT THE ND-500 ONE. The two layouts differ and the
     * "identical layout" reading was corrected: on the B30 the fault logical
     * address is at 0o17-0o20 for ALL stop traps. */
    (void)write16(sv, msg_word(msg_byte, 0x0Fu), (uint16_t)(trap_address >> 16u));
    (void)write16(sv, msg_word(msg_byte, 0x10u), (uint16_t)(trap_address & 0xFFFFu));

    if (trap_number == NDBUS_TRAP_PAGE_FAULT)
    {
        /* 46B, TRAP_GEN4 layout: physical segment at 0o21, MMS status at 0o22-0o23. */
        (void)write16(sv, msg_word(msg_byte, 0x11u), physical_segment);
        (void)write16(sv, msg_word(msg_byte, 0x12u), (uint16_t)(mms_status >> 16u));
        (void)write16(sv, msg_word(msg_byte, 0x13u), (uint16_t)(mms_status & 0xFFFFu));
        sv->page_faults_posted++;
    }
    else
    {
        /* Every other stop trap, TRAP_GEN3 layout: MMS status at 0o21-0o22 and the
         * physical segment at 0o25. The physical address, WR, ASTS and BADAP slots
         * stay zero, which reads as "not collected" - the CPU does not expose
         * them yet, and writing a plausible value there would be worse. */
        (void)write16(sv, msg_word(msg_byte, 0x11u), (uint16_t)(mms_status >> 16u));
        (void)write16(sv, msg_word(msg_byte, 0x12u), (uint16_t)(mms_status & 0xFFFFu));
        (void)write16(sv, msg_word(msg_byte, 0x15u), physical_segment);
    }

    sv->trap_stops_posted++;
    sv->last_trap_number = trap_number;
    sv->last_trap_pc = trapping_pc;
    sv->last_trap_address = trap_address;

    answer_message_in_place(sv, msg_byte, NDBUS_N5STA_ANSWER);
    return true;
}

bool ndbus_servicer_answer_monitor_call(NdbusServicer *sv, uint16_t x5cpu, uint32_t saved_p,
                                        uint16_t mon_number, uint32_t arg_count,
                                        const uint32_t *arg_addresses,
                                        const uint32_t *arg_values)
{
    if (sv == NULL || sv->pool == NULL || x5cpu >= NDBUS_SERVICER_MAX_PROCESSES)
    {
        return false;
    }

    sv->mon_calls_attempted++;

    uint32_t msg_byte = sv->process_msg[x5cpu];
    if (msg_byte == 0u)
    {
        sv->mon_calls_declined++;
        char line[160];
        (void)snprintf(line, sizeof line,
                       "mailbox monitor call DECLINED MON %oB P=0x%08X - no message recorded for "
                       "X5CPU %u",
                       (unsigned)mon_number, (unsigned)saved_p, (unsigned)x5cpu);
        servicer_log(sv, line);
        return false;
    }

    /* THE MICROCODE'S OWN SLOT LIMIT. CALL_MON checks the argument count against
     * sixteen, and the message has exactly sixteen address slots and sixteen value
     * slots. A larger count would write past them into whatever follows. */
    if (arg_count > NDBUS_MON_MAX_ARGS)
    {
        arg_count = NDBUS_MON_MAX_ARGS;
    }

    /* The saved P goes in the same pair of halfwords the copy family calls addrA:
     * 0o7 high, 0o10 low. */
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_N500A), (uint16_t)(saved_p >> 16u));
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_SWRST), (uint16_t)(saved_p & 0xFFFFu));

    /* STOPR = 1 is MOCALL - "this process stopped to make a monitor call", as
     * against TRAPCODE for a trap. NUMPA carries the argument count and MCNO the
     * monitor number. */
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_STOPR), NDBUS_STOPR_MOCALL);
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_NUMPA), (uint16_t)arg_count);
    (void)write16(sv, msg_word(msg_byte, NDBUS_MSG_MCNO), mon_number);

    /* Argument ADDRESSES at 0o40 + 2k, argument VALUES at 0o100 + 2k, both 32-bit,
     * so byte 0x40 + 4k and byte 0x80 + 4k. */
    for (uint32_t k = 0; k < arg_count; k++)
    {
        uint32_t addr_slot = msg_byte + NDBUS_MON_ARG_ADDR_BASE + (4u * k);
        uint32_t val_slot  = msg_byte + NDBUS_MON_ARG_VALUE_BASE + (4u * k);
        uint32_t a = (arg_addresses != NULL) ? arg_addresses[k] : 0u;
        uint32_t v = (arg_values != NULL) ? arg_values[k] : 0u;

        (void)write16(sv, addr_slot, (uint16_t)(a >> 16u));
        (void)write16(sv, addr_slot + 2u, (uint16_t)(a & 0xFFFFu));
        (void)write16(sv, val_slot, (uint16_t)(v >> 16u));
        (void)write16(sv, val_slot + 2u, (uint16_t)(v & 0xFFFFu));
    }

    sv->mon_calls_posted++;
    sv->last_mon_number = mon_number;

    answer_message_in_place(sv, msg_byte, NDBUS_N5STA_ANSWER);
    return true;
}

bool ndbus_servicer_process_chain(NdbusServicer *sv, uint32_t head_byte)
{
    if (sv == NULL || sv->pool == NULL)
    {
        return false;
    }

    uint32_t msg_byte = head_byte;
    bool     any = false;

    for (int n = 0; n < NDBUS_SERVICER_MAX_CHAIN; n++)
    {
        /* Read the LINK before serving. Once ANSWER(3) is visible the ND-100 may
         * free or relink the block, and answer_written interrupts it while this
         * walk is still going. The real microcode re-reads the link after MSG_END;
         * reading it first is an emulator safety choice with the same result for
         * any chain that is stable while queued. */
        uint32_t link_hi = read16(sv, msg_byte + NDBUS_MSG_LINK_WORD * 2u);
        uint32_t link_lo = read16(sv, msg_byte + (NDBUS_MSG_LINK_WORD + 1u) * 2u);
        uint32_t link = (link_hi << 16) | link_lo;

        sv->nodes_walked++;
        any |= ndbus_servicer_process_message(sv, msg_byte);

        if (link == NDBUS_SERVICER_LINK_END)
        {
            return any;
        }

        if (link == 0u)
        {
            /* Emulator guard, not microcode behaviour: the microcode tests only
             * -1, but SINTRAN zero-fills freed blocks, so a zero link means the
             * chain was torn down under the walk. */
            servicer_log(sv, "mailbox chain: LINK=0 - stopping walk");
            return any;
        }

        msg_byte = resolve_link(link);
    }

    servicer_log(sv, "mailbox chain: walk hit the length guard - possible cycle");
    return any;
}
