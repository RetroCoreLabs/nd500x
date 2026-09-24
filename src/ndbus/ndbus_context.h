/**
 * @file ndbus_context.h
 * @brief The SAMSON context block: the register image a CPU is started from.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * An ND-5000 is not started by poking a PC. The ND-100 places a CONTEXT BLOCK in
 * shared memory and the microcode's NEWCNTXT loads the machine from it, so the
 * block is the interface: P, B and the register file go in, and the CPU comes up
 * running.
 *
 * WHERE THE BLOCKS ARE. The control-store cell OFFSET (0o20) is patched with a
 * pointer to the context block AREA when the control store is loaded -
 * ND-05.017.01 Appendix A.1: "A pointer to the start of the context block is
 * patched in location OFFSET (address 20) in the microprogram when loading the
 * control store". It carries a BYTE address. Each CPU's block is then at
 *
 *     area_base + 0x100 + 0x100 * X5CPU
 *
 * so the stride is 256 bytes and the first CPU's block is one stride in - the
 * same 1-based shape the mailbox uses, and for the same reason: slot 0 is not a
 * CPU's.
 *
 * LAYOUT. Byte offsets of 32-bit words, from the microcode decode
 * (CNTXT-BLOCK-DECODE, graded per field against the microword addresses that
 * save and load each one). 32-bit values are stored big-endian, HIGH HALFWORD
 * FIRST, like every other multi-word value in the pool.
 *
 * THE TRAP IN THIS STRUCTURE, and it is a big one:
 *
 *   **Not every field in the block is loaded from the block.** The registers
 *   marked DIT below are DOMAIN registers, and NEWCNTXT does not touch them -
 *   they are sourced from the Domain Information Table. TOS, LL, HL, THA, CES,
 *   CAS and the whole trap-enable group (OTE1/2, CTE1/2, MTE1/2, TEM1/2) fall in
 *   that class. Writing TOS into a context block and expecting the CPU to come
 *   up with that stack pointer does nothing at all, and nothing reports it - the
 *   CPU starts with whatever the DIT said. LL and HL are loaded by TRAPSET from
 *   DIT+0x40 and DIT+0x44.
 *
 *   So the fields worth filling for a bring-up are P, B, and whatever registers
 *   the program actually reads. Everything else can stay zero.
 */

#ifndef NDBUS_CONTEXT_H
#define NDBUS_CONTEXT_H

#include "ndbus_pool.h"

/** Bytes per context block, and the stride between two CPUs' blocks. */
#define NDBUS_CTX_STRIDE_BYTES 0x100u

/** Highest X5CPU a context block area addresses, matching the octobus slots. */
#define NDBUS_CTX_MAX_CPU 7

/* ---- loaded by NEWCNTXT from the block ---------------------------------- */
#define NDBUS_CTX_P      0x00u /**< program counter. The entry point */
#define NDBUS_CTX_L      0x04u /**< link register */
#define NDBUS_CTX_B      0x08u /**< base register - the local data base */
#define NDBUS_CTX_R      0x0Cu /**< record register */
#define NDBUS_CTX_I1     0x10u /**< X1 */
#define NDBUS_CTX_I2     0x14u /**< X2 */
#define NDBUS_CTX_I3     0x18u /**< X3 */
#define NDBUS_CTX_I4     0x1Cu /**< X4 */
#define NDBUS_CTX_A1     0x20u
#define NDBUS_CTX_A2     0x24u
#define NDBUS_CTX_A3     0x28u
#define NDBUS_CTX_A4     0x2Cu
#define NDBUS_CTX_E1     0x30u
#define NDBUS_CTX_E2     0x34u
#define NDBUS_CTX_E3     0x38u
#define NDBUS_CTX_E4     0x3Cu
#define NDBUS_CTX_STATUS 0x40u /**< status composite, redistributed on load */
#define NDBUS_CTX_SRF10  0x44u /**< trap/status bits */
#define NDBUS_CTX_SRF13  0x48u /**< low halfword also goes to MM,PS and MM,PHS */
#define NDBUS_CTX_CED    0x5Cu /**< current executing domain; also MM,DOM */
#define NDBUS_CTX_CAD    0x60u /**< current alternative domain; also MM,ADOM */
#define NDBUS_CTX_SC1    0x6Cu /**< scratch */
#define NDBUS_CTX_SC2    0x70u /**< scratch */

/* ---- present in the block but NOT loaded from it ------------------------
 *
 * DOMAIN registers, sourced from the Domain Information Table. NEWCNTXT does not
 * touch them. Defined so code that reads a block can name the cells, and so the
 * names exist to be warned about - not so they can be written and expected to
 * take effect.
 */
#define NDBUS_CTX_DIT_TOS  0x4Cu /**< DIT-sourced */
#define NDBUS_CTX_DIT_LL   0x50u /**< DIT+0x40, loaded by TRAPSET */
#define NDBUS_CTX_DIT_HL   0x54u /**< DIT+0x44, loaded by TRAPSET */
#define NDBUS_CTX_DIT_THA  0x58u /**< DIT-sourced */
#define NDBUS_CTX_DIT_CES  0x64u /**< DIT-sourced */
#define NDBUS_CTX_DIT_CAS  0x68u /**< DIT-sourced */
#define NDBUS_CTX_DIT_OTE1 0x74u /**< DIT+0x16 / +0x26 via TRAPSET */
#define NDBUS_CTX_DIT_OTE2 0x78u /**< DIT-sourced */
#define NDBUS_CTX_DIT_CTE1 0x7Cu /**< DIT-sourced */
#define NDBUS_CTX_DIT_CTE2 0x80u /**< DIT-sourced */
#define NDBUS_CTX_DIT_MTE1 0x84u /**< DIT-sourced */
#define NDBUS_CTX_DIT_MTE2 0x88u /**< DIT-sourced */
#define NDBUS_CTX_DIT_TEM1 0x8Cu /**< DIT-sourced */
#define NDBUS_CTX_DIT_TEM2 0x90u /**< DIT-sourced */

/** One CPU's view of the context block area in the pool. */
typedef struct NdbusContext
{
    NdbusPool *pool;      /**< the shared pool the area lives in */
    uint32_t   area_byte; /**< pool BYTE offset of the area, from OFFSET (0o20) */
    int        x5cpu;     /**< this CPU's X5CPU number, 0-based */
} NdbusContext;

/**
 * @brief Point a context view at the block area in the pool.
 *
 * @param ctx       The view to fill.
 * @param pool      The shared pool.
 * @param area_byte Pool BYTE offset of the context block area - what the
 *                  control-store cell OFFSET (0o20) is patched with.
 * @param x5cpu     This CPU's X5CPU, 0 to NDBUS_CTX_MAX_CPU.
 * @return true on success; false having configured NOTHING when the pool is
 *         missing, when x5cpu is out of range, or when the block does not fit
 *         inside the pool.
 */
bool ndbus_context_attach(NdbusContext *ctx, NdbusPool *pool, uint32_t area_byte, int x5cpu);

/**
 * @brief Pool byte offset of this CPU's block: area + 0x100 + 0x100 * X5CPU.
 * @param ctx The view.
 * @return The offset, or 0 when the view is not attached.
 */
uint32_t ndbus_context_base(const NdbusContext *ctx);

/**
 * @brief Read one 32-bit field of this CPU's context block.
 * @param ctx   The view.
 * @param field An NDBUS_CTX_* byte offset.
 * @return The value, or 0 when unattached or outside the pool.
 */
uint32_t ndbus_context_read(const NdbusContext *ctx, uint32_t field);

/**
 * @brief Write one 32-bit field of this CPU's context block.
 *
 * Writing a DIT-sourced field is accepted and has NO EFFECT on a started CPU -
 * see the trap note at the top of this file. Use ndbus_context_field_is_loaded()
 * to tell the two classes apart.
 *
 * @param ctx   The view.
 * @param field An NDBUS_CTX_* byte offset.
 * @param value The 32-bit value, stored high halfword first.
 * @return true on success; false when unattached or outside the pool.
 */
bool ndbus_context_write(NdbusContext *ctx, uint32_t field, uint32_t value);

/**
 * @brief Whether NEWCNTXT actually loads this field from the block.
 *
 * @param field An NDBUS_CTX_* byte offset.
 * @return true for the register-file and status fields; false for the DOMAIN
 *         registers, which come from the Domain Information Table however the
 *         block is filled in.
 */
bool ndbus_context_field_is_loaded(uint32_t field);

/**
 * @brief Place a minimal context block for a bring-up: P and B, rest zeroed.
 *
 * These are the two fields a hand-assembled program needs - the entry point, and
 * a valid local data base for record-relative operands. Everything else is
 * cleared so a block left over from a previous run cannot leak into this one.
 *
 * @param ctx        The view.
 * @param entry_p    Value for P, the entry point.
 * @param local_base Value for B, the local data base.
 * @return true on success; false when unattached or the block does not fit.
 */
bool ndbus_context_place(NdbusContext *ctx, uint32_t entry_p, uint32_t local_base);

#endif /* NDBUS_CONTEXT_H */
