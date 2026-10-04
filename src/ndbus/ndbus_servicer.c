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
#include <stdlib.h>
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

uint32_t ndbus_servicer_process_context_byte(const NdbusServicer *sv, uint16_t x5cpu)
{
    if (sv == NULL || sv->context_area_base == 0u || x5cpu >= NDBUS_SERVICER_MAX_PROCESSES)
    {
        return 0u;
    }
    return sv->context_area_base + NDBUS_SERVICER_CTX_STRIDE +
           ((uint32_t)x5cpu * NDBUS_SERVICER_CTX_STRIDE);
}

int ndbus_servicer_read_message_x5cpu(const NdbusServicer *sv, uint32_t msg_byte)
{
    if (sv == NULL || sv->pool == NULL || msg_byte == 0u)
    {
        return -1;
    }
    return (int)read16(sv, msg_word(msg_byte, NDBUS_MSG_X5CPU));
}

/* ---- THE MICRO-FUNCTION TABLE: the hardware's vectored dispatch ------------
 *
 * The microcode does NOT compare MICFU against a list. It strips bit 15, range
 * checks what is left against 0..77B and indexes a 64-ENTRY DISPATCH TABLE
 * (ACCP-OCTOBUS-COMMAND-TABLE-2026-08-02.md and the mailbox catalogue's
 * "N5STA check -> CPU-target check -> MICFU -> vectored dispatch").
 *
 * This is that table. It replaces three things that were each a separate hand
 * written list of the same codes - is_continue, is_start_class and the dispatch
 * switch - so a function added in one and forgotten in the others is no longer
 * possible. That had already happened: 26B was absent from all three, fell to
 * the default arm, and was answered 5ERANSWER, so the process it belonged to was
 * never resumed.
 *
 * TWO THINGS THE TABLE FIXES THAT NEITHER EMULATOR DID.
 *
 * BIT 15 IS A FLAG, NOT PART OF THE FUNCTION NUMBER, and the microcode strips it
 * before dispatching; the range check is 0..77B. Both emulators switched on the
 * raw halfword and neither range checked, so a flagged message the hardware
 * dispatches normally was answered 5ERANSWER by both. The carve outranks both
 * emulators, so this is a shared defect rather than a port gap.
 *
 * THE GRADE MATTERS AND IT IS NOT FULL. The source is
 * ND500-MAILBOX-MESSAGE-CATALOG.md:209, which marks this [V/D] - part verified,
 * part DERIVED - not [V]. So the strip and the bound are the best reading of the
 * microcode available and not a byte-level certainty. They are implemented
 * because the alternative is demonstrably wrong (a flagged continue answered
 * 5ERANSWER leaves its process parked for ever), but if a message ever turns up
 * that needs bit 15 kept, this is the line to come back to and the grade is the
 * reason it might be.
 *
 * Out of range is kept distinct from "in range but not implemented here":
 * anything unrecognised used to fall through the switch's default, which
 * conflates the two, and they want different answers.
 *
 * A code we do not implement is NDBUS_MICFU_CLASS_NONE, which is the honest
 * value: the table says what the hardware would dispatch, not what we handle. */

#define NDBUS_MICFU_TABLE_SIZE 64u   /* 0..77B, the microcode's own bound */
#define NDBUS_MICFU_FLAG_BIT   0x8000u /* stripped before dispatch */

typedef struct
{
    const char *name;   /**< the ND mnemonic, for a log line that reads */
    uint8_t     cls;    /**< NDBUS_MICFU_CLASS_* */
} NdbusMicfuEntry;

/* Only the codes this emulator knows are filled in; the rest are NONE, which is
 * a statement about our coverage and not about the machine. */
static const NdbusMicfuEntry s_micfu_table[NDBUS_MICFU_TABLE_SIZE] = {
    [NDBUS_MICFU_RMICV]   = { "3RMICV",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_SWMESS]  = { "3SWMESS", NDBUS_MICFU_CLASS_NONE   },
    [NDBUS_MICFU_DMEMRD]  = { "DMEMRD",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_DMEMWR]  = { "DMEMWR",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_CACHE]   = { "CACHE",   NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_RESIRD]  = { "RESIRD",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_RESIWR]  = { "RESIWR",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_WREG]    = { "3WREG",   NDBUS_MICFU_CLASS_NONE   },
    [NDBUS_MICFU_STARTP0] = { "STARTP0", NDBUS_MICFU_CLASS_START  },
    [NDBUS_MICFU_START]   = { "3START",  NDBUS_MICFU_CLASS_START  },
    [NDBUS_MICFU_MONCO]   = { "3MONCO",  NDBUS_MICFU_CLASS_CONTINUE },
    [NDBUS_MICFU_TRACO]   = { "3TRACO",  NDBUS_MICFU_CLASS_CONTINUE },
    [NDBUS_MICFU_WMONCO]  = { "3WMONCO", NDBUS_MICFU_CLASS_CONTINUE },
    [NDBUS_MICFU_PHYSRD]  = { "PHYSRD",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_PHYSWR]  = { "PHYSWR",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_IMEMRD]  = { "IMEMRD",  NDBUS_MICFU_CLASS_INLINE },
    [NDBUS_MICFU_IMEMWR]  = { "IMEMWR",  NDBUS_MICFU_CLASS_INLINE },

    /* REFUSED ON THIS GENERATION, AND THAT IS THE CORRECT ANSWER - not a gap.
     * 05 3SWMESS, 16B 3EXAR, 17B 3DEPR, 20B 3RREG, 21B 3WREG and 27B 3FITRNSF are
     * MSG_ILLEG in both B30 listings and SINTRAN never transmits them here; the
     * reference gates every one of them on `Generation == ND500`. They are named
     * so a future reader does not "fix" a refusal that is deliberate, and so a
     * 3022 port knows exactly which six to implement. */
    [NDBUS_MICFU_RPREG]   = { "3RPREG",  NDBUS_MICFU_CLASS_NONE   },
};

uint16_t ndbus_micfu_dispatch_code(uint16_t micfu_halfword)
{
    /* Strip the flag, then range check. Out of range returns the table size,
     * which no entry can have, so a caller cannot mistake it for a function. */
    uint16_t code = micfu_halfword & (uint16_t)~NDBUS_MICFU_FLAG_BIT;
    return (code < NDBUS_MICFU_TABLE_SIZE) ? code : (uint16_t)NDBUS_MICFU_TABLE_SIZE;
}

uint8_t ndbus_micfu_class(uint16_t micfu_halfword)
{
    uint16_t code = ndbus_micfu_dispatch_code(micfu_halfword);
    return (code < NDBUS_MICFU_TABLE_SIZE) ? s_micfu_table[code].cls
                                           : (uint8_t)NDBUS_MICFU_CLASS_NONE;
}

const char *ndbus_micfu_name(uint16_t micfu_halfword)
{
    uint16_t code = ndbus_micfu_dispatch_code(micfu_halfword);
    if (code >= NDBUS_MICFU_TABLE_SIZE)
    {
        return "out-of-range";
    }
    return (s_micfu_table[code].name != NULL) ? s_micfu_table[code].name : "unknown";
}

bool ndbus_micfu_is_continue(uint16_t micfu)
{
    return ndbus_micfu_class(micfu) == NDBUS_MICFU_CLASS_CONTINUE;
}

bool ndbus_micfu_is_start_class(uint16_t micfu)
{
    uint8_t cls = ndbus_micfu_class(micfu);
    return cls == NDBUS_MICFU_CLASS_START || cls == NDBUS_MICFU_CLASS_CONTINUE;
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
    /* ALSO LOG ANY TRANSFER THAT LANDS IN THE SWAPPER'S SEGMENT-DESCRIPTOR TABLE,
     * however late it arrives. Those descriptors sit at ND-500 logical 0x08038000
     * with a stride of 100 bytes, and the monitor's own LIST-SEGMENT-TABLE-ENTRY
     * says physical segment 15B - decimal 13 - is an in-use 102B-page scratch
     * segment on swap file 0, while its descriptor on this side reads 32 zero
     * bytes. So the descriptor is never propagated, and the transfer that should
     * carry it is past the first few that the count-based gate prints. The window
     * is the table itself, not a count, because the interesting copy is by
     * definition the one a count stops printing. */
    const uint32_t desc_lo = 0x00038000u;
    const uint32_t desc_hi = desc_lo + 32u * 100u;
    const bool hits_desc_table = (dst >= desc_lo && dst < desc_hi);

    if (sv->copies_done <= NDBUS_SERVICER_COPY_LOG_LIMIT || hits_desc_table)
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
    case NDBUS_MICFU_IMEMRD:  /* 34B */
        /* READ members: target A -> buffer B. PHYSRD's A side is segment-relative;
         * RESIRD and IMEMRD carry a flat address.
         *
         * 34B IS A NAMESPACE COLLISION AND THE GENERATION SETTLES IT. The symbol
         * 3MONO is 0o34 on the ND-500, where it is answer-only, while 0o34 in the
         * B30's octobus copy family is IMEMRD. The reference resolved this with a
         * differential oracle - the microword CpuND5000's own IMEMWR-then-IMEMRD
         * round-trip is byte-exact - so on this generation 34B is the copy and on
         * the 3022 lane it would be the answer. We are the ND-5000 lane, so it is
         * the copy; a 3022 port must branch here rather than inherit this.
         *
         * It was previously absent and fell to the default arm, which answers
         * 5ERANSWER - so SINTRAN's instruction-memory READ back was refused while
         * the matching IMEMWR below was served, which is the asymmetry that makes
         * a verify-after-load fail with nothing obviously wrong. */
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
    /* THE START CLASS, AND 24B BELONGS IN IT.
     *
     * 22B STARTP0, 23B 3START, 24B 3MONCO and 25B 3TRACO all reach MSG_START in the
     * microcode: two of them begin a process and two of them CONTINUE one that is
     * parked, but all four hand the message to the same context-switch-and-run path.
     *
     * MEASURED 30-SEP-2026: with 24B missing from this list, the ND-5000 reported a
     * monitor call, SINTRAN performed it and answered with 24B, and this servicer
     * declined that answer as an unported micro-function - so the parked process was
     * never resumed and the monitor reported that the swapper stopped. The answer had
     * arrived; nothing was listening for it. */
    if (ndbus_micfu_is_start_class(micfu))
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
            uint32_t ctx_byte = ndbus_servicer_process_context_byte(sv, x5cpu);

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

    /* THE MESSAGE THE RECORD WENT ON, FIELD BY FIELD.
     *
     * SINTRAN's TRAPDECODER (MP-P2-N500.NPL:135332) decides what to do with a 46B
     * from TWO fields of this block that the ND-500 side never writes: it compares
     * the message against the swapper's own message, and it reads the RECEIVER and
     * errors out when that names the swapper process. Taking the wrong branch there
     * is fatal - "page fault in swapper", XRSTARTALL - and from the outside it is
     * indistinguishable from a record that never arrived. A count of posted
     * trap-stops cannot tell those apart, so name the block and the fields.
     *
     * Bounded to the first four, which is already more than one PLACE-DOMAIN needs. */
    if (sv->trap_stops_posted < 4u)
    {
        char line[200];
        (void)snprintf(line, sizeof line,
                       "trap-stop %oB on msg 0x%06X for X5CPU %u: N5STA=0x%04X SENDE=0x%04X "
                       "X5CPU=0x%04X X5ACT=0x%04X MICFU=0x%04X N500A=0x%04X SWRST=0x%04X "
                       "STOPR=0x%04X TRAPN=0x%04X",
                       (unsigned)trap_number, (unsigned)msg_byte, (unsigned)x5cpu,
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_N5STA)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_SENDE)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_X5CPU)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_X5ACT)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_MICFU)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_N500A)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_SWRST)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_STOPR)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_TRAPN)));
        servicer_log(sv, line);
    }

    sv->trap_stops_posted++;
    sv->last_trap_number = trap_number;
    sv->last_trap_pc = trapping_pc;
    sv->last_trap_address = trap_address;

    answer_message_in_place(sv, msg_byte, NDBUS_N5STA_ANSWER);
    return true;
}

/** @brief One row of the inline-copy set, with the grade its log line carries. */
typedef struct NdbusMonInlineEntry
{
    uint16_t    mon_number;
    const char *grade;
} NdbusMonInlineEntry;

/* THE SET IS THE TABLE. A chain of == tests is where a number gets added because
 * it looked adjacent; a table is a thing that can be read against the microcode. */
static const NdbusMonInlineEntry s_mon_inline_table[] = {
    { NDBUS_MON_504B_NOUTS,  "MEASURED live: arg[1] value = count, arg[2] address = buffer" },
    { NDBUS_MON_511B_DVIO,   "MICROCODE-VERIFIED outbound leg; DVIO return leg UNVERIFIED" },
    { NDBUS_MON_512B_A5XMSG, "MICROCODE-VERIFIED: copy routine 010662 reads no MON number" },
};

#define NDBUS_MON_INLINE_TABLE_SIZE \
    (sizeof s_mon_inline_table / sizeof s_mon_inline_table[0])

bool ndbus_mon_requires_inline_copy(uint16_t mon_number)
{
    for (size_t i = 0; i < NDBUS_MON_INLINE_TABLE_SIZE; i++)
    {
        if (s_mon_inline_table[i].mon_number == mon_number)
        {
            return true;
        }
    }
    return false;
}

uint32_t ndbus_servicer_inline_buffer_target(const NdbusServicer *sv, uint32_t msg_byte)
{
    if (sv == NULL || sv->pool == NULL)
    {
        return 0u;
    }

    uint32_t raw = ((uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MON_ABUFA_WORD)) << 16u)
                 |  (uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MON_ABUFA_WORD + 1u));

    /* A WORD address. See the header for the measurement that settles it. */
    return raw << 1u;
}

bool ndbus_servicer_write_inline_buffer(NdbusServicer *sv, uint32_t msg_byte,
                                        const uint8_t *source, uint32_t count)
{
    if (sv == NULL || sv->pool == NULL || source == NULL)
    {
        return false;
    }

    /* 010716 and 010717, the microcode's own guards. Over-size copies NOTHING. */
    if (count == 0u || count > NDBUS_MON_INLINE_MAX_BYTES)
    {
        return false;
    }

    uint32_t target = ndbus_servicer_inline_buffer_target(sv, msg_byte);
    if (target == 0u)
    {
        return false;
    }

    for (uint32_t i = 0; i < count; i += 2u)
    {
        /* Big-endian pack, matching every other halfword written here. An odd final
         * byte pairs with zero rather than reading past the buffer. */
        uint16_t hw = (uint16_t)((uint16_t)source[i] << 8u);
        if ((i + 1u) < count)
        {
            hw |= (uint16_t)source[i + 1u];
        }
        if (!write16(sv, target + i, hw))
        {
            return false;
        }
    }

    /* Tell SINTRAN the buffer is inline. Set explicitly rather than trusting
     * whatever was there - the bit happening to be set already is what made this
     * fail silently instead of loudly. */
    uint32_t miflag_byte = msg_byte - NDBUS_MON_MIFLAG_BACK_BYTES;
    uint16_t miflag = read16(sv, miflag_byte);
    (void)write16(sv, miflag_byte, (uint16_t)(miflag | NDBUS_MON_MIFLAG_WSMC));

    return true;
}

/**
 * The inline-copy arm of a monitor-call stop.
 *
 * Where the count and the pointer come from, read out of the copy routine at
 * 010662 rather than generalised from one measurement:
 *     010673  AM#20 := DLADDR      % arg[1]: its ADDRESS
 *     010674  AM#34 := DATA        % arg[1]: its VALUE    -> THE BYTE COUNT
 *     010701  AM#20 := DLADDR      % arg[2]: its ADDRESS
 *     010702  AL#34 := AM#20       % arg[2]: its ADDRESS  -> THE SOURCE BUFFER
 * and the loop uses exactly those two:
 *     010720  AL#11 := AM#34       % limit  = the count
 *     010725  DP    := AL#34+AM#11 % source = buffer + running index
 */
static void mon_inline_copy(NdbusServicer *sv, uint32_t msg_byte, uint16_t mon_number,
                            uint32_t arg_count, const uint32_t *arg_addresses,
                            const uint32_t *arg_values)
{
    if (!ndbus_mon_requires_inline_copy(mon_number))
    {
        return;
    }

    char line[200];

    if (arg_count < 3u || arg_addresses == NULL || arg_values == NULL)
    {
        (void)snprintf(line, sizeof line,
                       "mailbox MON %oB inline buffer DECLINED: argc=%u - needs three arguments, "
                       "SINTRAN will read stale message content and print it",
                       (unsigned)mon_number, (unsigned)arg_count);
        servicer_log(sv, line);
        return;
    }

    uint32_t count = arg_values[1];
    if (count == 0u || count > NDBUS_MON_INLINE_MAX_BYTES)
    {
        (void)snprintf(line, sizeof line,
                       "mailbox MON %oB inline buffer DECLINED: count=%u outside 1..%u - the "
                       "microcode copies nothing in this case either",
                       (unsigned)mon_number, (unsigned)count,
                       (unsigned)NDBUS_MON_INLINE_MAX_BYTES);
        servicer_log(sv, line);
        return;
    }

    if (sv->host.read_nd500_data_bytes == NULL)
    {
        (void)snprintf(line, sizeof line,
                       "mailbox MON %oB inline buffer DECLINED: no read_nd500_data_bytes host - "
                       "SINTRAN will read stale message content and print it",
                       (unsigned)mon_number);
        servicer_log(sv, line);
        return;
    }

    uint8_t buffer[NDBUS_MON_INLINE_MAX_BYTES];
    if (!sv->host.read_nd500_data_bytes(sv->host.ctx, arg_addresses[2], buffer, count))
    {
        (void)snprintf(line, sizeof line,
                       "mailbox MON %oB inline buffer DECLINED: read of %u byte(s) at ND-500 "
                       "logical 0x%08X failed - SINTRAN will read stale content and print it",
                       (unsigned)mon_number, (unsigned)count, (unsigned)arg_addresses[2]);
        servicer_log(sv, line);
        return;
    }

    uint32_t target = ndbus_servicer_inline_buffer_target(sv, msg_byte);
    if (target == 0u)
    {
        (void)snprintf(line, sizeof line,
                       "mailbox MON %oB inline buffer NOT COPIED: ABUFA is zero, so the message "
                       "names no buffer",
                       (unsigned)mon_number);
        servicer_log(sv, line);
        return;
    }

    if (!ndbus_servicer_write_inline_buffer(sv, msg_byte, buffer, count))
    {
        (void)snprintf(line, sizeof line,
                       "mailbox MON %oB inline buffer FAILED: %u byte(s) to pool 0x%08X",
                       (unsigned)mon_number, (unsigned)count, (unsigned)target);
        servicer_log(sv, line);
        return;
    }

    char text[25];
    uint32_t shown = (count < 24u) ? count : 24u;
    for (uint32_t q = 0; q < shown; q++)
    {
        uint8_t b = buffer[q];
        text[q] = (b >= 0x20u && b < 0x7Fu) ? (char)b : '.';
    }
    text[shown] = '\0';

    (void)snprintf(line, sizeof line,
                   "mailbox MON %oB inline buffer COPIED n=%u src=0x%08X -> pool 0x%08X \"%s\"",
                   (unsigned)mon_number, (unsigned)count, (unsigned)arg_addresses[2],
                   (unsigned)target, text);
    servicer_log(sv, line);
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

    /* THE INLINE USER BUFFER, after the argument slots because the microcode's copy
     * routine reads arg[1]'s value and arg[2]'s address out of exactly those slots.
     * Without this SINTRAN takes the inline arm - it never sends 3RMED - and prints
     * whatever stale bytes are at ABUFA. */
    mon_inline_copy(sv, msg_byte, mon_number, arg_count, arg_addresses, arg_values);

    /* THE SWAPPER'S OWN REQUEST AND STATUS WORDS, for MON 377B only.
     *
     * SINTRAN's LNEWSWAP arm reads SWPST (word 0o103) and treats NON-ZERO as "error
     * answer from the swapper", propagating that code to the faulting process:
     *     X:=SWMSG; *AAX SWPST; LDATX
     *     IF A><0 THEN   % Error-answer from swapper?
     * Nothing on this side writes SWPST, so a non-zero value here is a swapper error
     * SINTRAN reports that we never intended - and it is invisible without printing
     * it. SWPFU names which request it is (1 LNEWSWAP, 2 LSWPAGE, ...).
     *
     * MEASURED on PLACE-DOMAIN CPU-STAT: the swapper asks for logical segment 13 -
     * the fresh, empty scratch segment GSWSP connected for the domain - twice, and
     * PST entry 13 is never backed, after which the monitor reports that the swapper
     * stopped. Whether SINTRAN read an error we left in SWPST is exactly what this
     * says. Bounded. */
    if (mon_number == 0377u && sv->mon_calls_posted < 24u)
    {
        /* THE CONNECT RECORD'S STATE FIELD, which decides whether a fresh segment
         * may GROW on its first write. Word 0o36 of the message; STATE is bits 13:10
         * of that halfword, and the swapper's grow-permit set is {13,14,15} - a
         * STATE outside it makes the segment non-growable, so a first write to an
         * empty segment is declined rather than backed.
         *
         * MEASURED here: psn 13, the scratch segment GSWSP connected for the domain,
         * is handed to LNEWSWAP and NEVER receives an LSWPAGE, while psn 10, 11 and
         * 12 each do. An empty segment has nothing to transfer, so growth is the only
         * way it can be backed, and the grow gate is where that is refused. */
        uint16_t r36 = read16(sv, msg_word(msg_byte, 0036u));
        uint32_t state = ((uint32_t)r36 >> 10) & 0x0Fu;
        char line[200];
        (void)snprintf(line, sizeof line,
                       "MON 377B swapper words: SWPFU=0x%04X SWPST=0x%04X SPFLA=0x%04X "
                       "r36=0x%04X STATE=0x%X growable=%d for X5CPU %u on msg 0x%06X",
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_SWPFU)),
                       (unsigned)read16(sv, msg_word(msg_byte, NDBUS_MSG_SWPST)),
                       (unsigned)read16(sv, msg_word(msg_byte, 0143u)),
                       (unsigned)r36, (unsigned)state,
                       (state == 13u || state == 14u || state == 15u) ? 1 : 0,
                       (unsigned)x5cpu, (unsigned)msg_byte);
        servicer_log(sv, line);
    }

    sv->mon_calls_posted++;
    sv->last_mon_number = mon_number;

    answer_message_in_place(sv, msg_byte, NDBUS_N5STA_ANSWER);
    return true;
}

uint8_t ndbus_servicer_read_nd100_byte(const NdbusServicer *sv, uint32_t byte_addr)
{
    if (sv == NULL || sv->pool == NULL)
    {
        return 0u;
    }
    /* The halfword that contains the byte, then the half of it that is the byte:
     * the EVEN byte is the high half, on both machines. One place only - a second
     * copy of these two lines is how a string comes out interleaved. */
    uint16_t w = read16(sv, byte_addr & ~1u);
    return ((byte_addr & 1u) == 0u) ? (uint8_t)(w >> 8) : (uint8_t)w;
}

bool ndbus_servicer_read_wmonco_block(NdbusServicer *sv, uint32_t msg_byte,
                                      NdbusWmoncoBlock *out)
{
    if (sv == NULL || sv->pool == NULL || out == NULL || msg_byte == 0u)
    {
        return false;
    }
    memset(out, 0, sizeof(*out));

    /* Offsets from the reference's decode of the microcode at 015752-016004
     * (Nd500MicrocodeServicer.cs, N5MicroFunction.WaitMonitorCall):
     *   26NRB  byte count           halfword 0o17
     *   26ADD  process address, 32b  halfwords 0o15-0o16, high half first
     *   ABUFA  ND-100 WORD address   halfwords 0o140-0o141, high half first
     * ABUFA is a WORD address - the microcode reaches +0xC0 through two chained
     * +0x60 MARG hops - so it is shifted left to reach bytes. */
    uint32_t nrb = read16(sv, msg_word(msg_byte, NDBUS_MSG_26NRB));
    out->dest = ((uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MSG_26ADD_HI)) << 16)
              |  (uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MSG_26ADD_LO));

    if (nrb >= 0x2000u)
    {
        /* NOT a refusal: the caller skips the copy, forces FUNCV to 0o174 and
         * sets K, and resumes the process anyway. */
        out->oversize = true;
        return true;
    }

    uint32_t src_word = ((uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MSG_ABUFA_HI)) << 16)
                      |  (uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MSG_ABUFA_LO));
    out->src_byte = src_word << 1;
    out->count    = nrb;
    return true;
}

bool ndbus_servicer_read_monitor_result(NdbusServicer *sv, uint32_t msg_byte,
                                       NdbusMonResult *out)
{
    if (sv == NULL || sv->pool == NULL || out == NULL || msg_byte == 0u)
    {
        return false;
    }

    memset(out, 0, sizeof(*out));

    /* FUNCV, 32 bits, in the MCNO and MSWMC slots - the same halfwords that carried
     * the monitor number and part of the saved P on the way out. */
    out->funcv = ((uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MSG_MCNO)) << 16)
               |  (uint32_t)read16(sv, msg_word(msg_byte, NDBUS_MSG_MSWMC));

    /* KFLIP in the STOPR slot, where MOCALL/TRAPCODE lived on the way out. */
    out->kflip = read16(sv, msg_word(msg_byte, NDBUS_MSG_STOPR));

    /* NUMPA as a MASK, not a count. Bit k names parameter k. */
    out->mask = read16(sv, msg_word(msg_byte, NDBUS_MSG_NUMPA));

    for (uint32_t k = 0; k < NDBUS_MON_MAX_ARGS; k++)
    {
        if ((out->mask & (1u << k)) == 0u)
        {
            continue;
        }
        uint32_t addr_slot = msg_byte + NDBUS_MON_ARG_ADDR_BASE + (4u * k);
        uint32_t val_slot  = msg_byte + NDBUS_MON_ARG_VALUE_BASE + (4u * k);
        out->addresses[out->count] = ((uint32_t)read16(sv, addr_slot) << 16)
                                   |  (uint32_t)read16(sv, addr_slot + 2u);
        out->values[out->count]    = ((uint32_t)read16(sv, val_slot) << 16)
                                   |  (uint32_t)read16(sv, val_slot + 2u);
        out->count++;
    }

    /* THE RAW HEADER OF THE ANSWER, for the monitor numbers named in
     * NDBUS_MON_RESULT_WATCH. The decoded FUNCV/K/mask is a READING of these
     * words, and a reading can be wrong about where SINTRAN put something.
     *
     * MEASURED, and this is why: the cross-emulator record for MON 422B GSWSP on
     * an L-version pack is that SINTRAN answers K=1 with error 1013B, "illegal
     * monitor call number" - a refusal. Our side decoded K=0 and a segment
     * number of 0, which sent CPU-STAT down its success path on a meaningless
     * value instead of its error path at 0x08004609, where it would have
     * reported the refusal and exited. Either SINTRAN really accepted it here,
     * or K and the error code are carried in words this reader does not look at.
     * Only the raw words can tell those apart. 1013B is 0x020B.
     *
     * Bounded, and only for the watched numbers, so it cannot flood. */
    /* NOT KEYED ON THE MON NUMBER. The first version of this watched
     * sv->last_mon_number for 0422, and it never fired even on runs that
     * reached the call: that field is SHARED, and the swapper's own MON 377B
     * calls overwrite it between a domain's request being posted and its answer
     * being read, so by this point it reads 0377. An instrument keyed on state
     * another process owns measures the other process - the same trap as a
     * PC-only trace on a shared CPU structure.
     *
     * So dump every result read, bounded, and let the message address say whose
     * it is: 0x8D30 is the swapper's block and 0x8E30 a domain's. The bound is
     * large enough to reach a domain's calls, which arrive around the twentieth. */
    if (sv->mon_results_read < 24u)
    {
        char line[200];
        int n = (int)snprintf(line, sizeof line,
                              "mon-result RAW words 0..15 on msg 0x%06X (mask=0x%04X "
                              "count=%u):",
                              (unsigned)msg_byte, (unsigned)out->mask,
                              (unsigned)out->count);
        for (uint32_t w = 0; w < 16u && n > 0 && (size_t)n < sizeof line; w++)
        {
            n += snprintf(line + n, sizeof line - (size_t)n, " %04X",
                          (unsigned)read16(sv, msg_word(msg_byte, w)));
        }
        servicer_log(sv, line);
    }

    sv->mon_results_read++;
    return true;
}

/* REPORT A CHAIN WHOSE SHAPE CHANGED, every time, with no budget.
 *
 * The shape is the sequence of node addresses. SINTRAN relinks the chain as it
 * works - the reference's census shows the head's link moving off one node onto
 * another - so "which nodes are in the chain right now" is the fact that settles
 * whether a node we keep serving is one we should have stopped reaching. A budget
 * here would hide exactly the late relink that matters, so this reports the first
 * walk and then only CHANGES, which is a handful of lines in a whole run.
 */
static void chain_walk_done(NdbusServicer *sv, const uint32_t *shape, int hops,
                            uint32_t *last_shape, int *last_hops, int *walks)
{
    (*walks)++;

    if (hops > NDBUS_SERVICER_MAX_CHAIN)
    {
        hops = NDBUS_SERVICER_MAX_CHAIN;
    }

    bool same = (hops == *last_hops);
    for (int i = 0; same && i < hops; i++)
    {
        if (shape[i] != last_shape[i])
        {
            same = false;
        }
    }
    if (same)
    {
        return;
    }

    char line[260];
    int  n = snprintf(line, sizeof line, "mailbox chain SHAPE at walk #%d:", *walks);
    for (int i = 0; i < hops && n > 0 && (size_t)n < sizeof line; i++)
    {
        n += snprintf(line + n, sizeof line - (size_t)n, " 0x%06X", (unsigned)shape[i]);
        last_shape[i] = shape[i];
    }
    *last_hops = hops;
    servicer_log(sv, line);
}

/* THE CHAIN'S SHAPE, which no other instrument in this file reports.
 *
 * Ported from the reference's own chain-walk census. Without it a repeated serve
 * of one node is indistinguishable from the ND-100 re-queueing that node, and the
 * two have opposite causes: a stale link we keep following, against SINTRAN
 * genuinely asking again. The reference's census answered exactly that question
 * for message 0x42C130, which it saw in ToNd500 ONCE in 1197 node visits.
 *
 * Set NDBUS_CHAINDBG to a number of walks to report. Off by default, because the
 * line is one per hop and a scan happens on every mailbox pass.
 */
static int chain_debug_budget(void)
{
    static int budget = -1;
    if (budget < 0)
    {
        const char *e = getenv("NDBUS_CHAINDBG");
        budget = (e != NULL) ? atoi(e) : 0;
        if (budget < 0)
        {
            budget = 0;
        }
    }
    return budget;
}

bool ndbus_servicer_process_chain(NdbusServicer *sv, uint32_t head_byte)
{
    if (sv == NULL || sv->pool == NULL)
    {
        return false;
    }

    uint32_t msg_byte = head_byte;
    bool     any = false;

    static int    s_chain_walks;
    static uint32_t s_last_shape[NDBUS_SERVICER_MAX_CHAIN + 1];
    static int      s_last_hops = -1;
    uint32_t        shape[NDBUS_SERVICER_MAX_CHAIN + 1];
    int             hops = 0;
    bool            report = (s_chain_walks < chain_debug_budget());

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

        /* Read N5STA and MICFU BEFORE the serve: the serve overwrites N5STA, so a
         * line printed afterwards would report our own write rather than the state
         * the node was found in. */
        uint16_t pre_sta   = read16(sv, msg_word(msg_byte, NDBUS_MSG_N5STA));
        uint16_t pre_micfu = read16(sv, msg_word(msg_byte, NDBUS_MSG_MICFU));

        sv->nodes_walked++;
        bool served = ndbus_servicer_process_message(sv, msg_byte);
        any |= served;

        if (hops <= NDBUS_SERVICER_MAX_CHAIN)
        {
            shape[hops] = msg_byte;
        }
        hops++;

        if (report)
        {
            char cl[200];
            (void)snprintf(cl, sizeof cl,
                           "chain walk #%d hop %d @0x%06X N5STA=0x%04X MICFU=%oB "
                           "link=0x%08X %s",
                           s_chain_walks, hops - 1, (unsigned)msg_byte,
                           (unsigned)pre_sta, (unsigned)pre_micfu, (unsigned)link,
                           served ? "SERVED"
                                  : (((pre_sta & NDBUS_N5STA_MASK) == NDBUS_N5STA_TO_ND500)
                                     ? "ToNd500 but NOT served - taken by the host, "
                                       "answered by the process's own stop"
                                     : "skipped - not addressed to us"));
            servicer_log(sv, cl);
        }

        if (link == NDBUS_SERVICER_LINK_END)
        {
            chain_walk_done(sv, shape, hops, s_last_shape, &s_last_hops, &s_chain_walks);
            return any;
        }

        if (link == 0u)
        {
            /* Emulator guard, not microcode behaviour: the microcode tests only
             * -1, but SINTRAN zero-fills freed blocks, so a zero link means the
             * chain was torn down under the walk. */
            servicer_log(sv, "mailbox chain: LINK=0 - stopping walk");
            chain_walk_done(sv, shape, hops, s_last_shape, &s_last_hops, &s_chain_walks);
            return any;
        }

        msg_byte = resolve_link(link);
    }

    servicer_log(sv, "mailbox chain: walk hit the length guard - possible cycle");
    chain_walk_done(sv, shape, hops, s_last_shape, &s_last_hops, &s_chain_walks);
    return any;
}

uint32_t ndbus_servicer_nd100_bytes_per_unit(void)
{
    /* The octobus/5MPM convention: the operand is already a window-relative BYTE
     * offset. See the doc comment in ndbus_servicer.h. */
    return 1u;
}
