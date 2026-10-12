/**
 * @file ndbus_msgqueue.h
 * @brief The X5BEX message chain: the queue a doorbell tells a CPU to walk.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The doorbell says "there is work"; THIS is the work. X5BEX in a CPU's
 * extension block is the head of a singly-linked chain of message blocks in
 * shared memory, each one carrying the offset of the next in its LINK field.
 * Because the answer to a ring is to walk the whole chain, a coalesced ring
 * loses nothing - see ndbus_mailbox.h.
 *
 * LINKS ARE WINDOW-RELATIVE BYTE OFFSETS on the octobus generation. RetroCore
 * records the transport seam plainly: "octobus = byte identity, ND500 = <<1",
 * and applying the ND-500 word shift on the octobus path put a slot at twice the
 * address the microcode wrote. The same convention X5FIF uses. We only build the
 * octobus generation, so links are byte offsets throughout and no shift exists
 * anywhere in this file - if one ever appears, it is a bug.
 *
 * -1 TERMINATES, and it is also what an empty X5BEX holds. Zero does NOT: offset
 * 0 is a legal place for a message block, so a chain terminated with 0 would
 * walk into the mailbox header.
 *
 * MESSAGE BLOCK FIELDS, as WORD offsets (byte = word * 2). From the microcode
 * decode and the byte-verified SWMSG field dossier, via RetroCore's
 * N5MessageOffsets:
 *
 *     0-1   LINK    next block, or -1
 *     2     N5STA   message status
 *     3     SENDE   sender; the watchdog uses -1
 *     4     X5CPU   5RECE: the receiver PROCESS number. Misnamed, see below
 *     5     X5ACT   5CCLR: the CPU's X5CCL when the process last ran. Misnamed
 *     6     MICFU   micro-function
 *     7     N500A / SWFUN   OVERLAY - see the warning below
 *     10B   N500A_LO / SWRST OVERLAY - see the warning below
 *     11B   STOPR   stop reason / N100A / ACPRO, direction-dependent
 *     12B   NUMPA   number of parameters
 *     13B   MCNO    mon-call number / NRBYT byte count, direction-dependent
 *     14B   MSWMC   swapper mon-call field
 *     16B   TRAPN   trap number
 *     37B   SMCNO   saved mon-call number
 *     101B  SWPFU   swapper function code
 *     103B  SWPST   swapper status
 *
 * THE OVERLAY AT 7 AND 10B IS A TRAP, and RetroCore records that it cost a full
 * day. Words 7 and 10B look like the high and low halves of one 32-bit ND-500
 * address, and reading them that way is correct ONLY for the RESIRD/RESIWR
 * block-copy arm. In the swapper direction 10B is its own field, SWRST, and the
 * real swapper reads it as a SEGMENT NUMBER and range-checks it. A reader that
 * always treats 7-10B as one address gets a plausible wrong answer on every
 * swapper request. Which arm applies is decided by MICFU, so the two are named
 * separately here and no helper reads them as a pair.
 */

#ifndef NDBUS_MSGQUEUE_H
#define NDBUS_MSGQUEUE_H

#include "ndbus_mailbox.h"

/*
 * THE ND-100 / ND-5000 MESSAGE, FIELD BY FIELD: WHO WRITES IT, WHO READS IT, AND
 * WHAT THE VALUES MEAN
 *
 * Sources, and how each line below is marked:
 *   [SYM]  offset or value from the SINTRAN III VSX/500 L07 symbol file
 *          (N500-SYMBOLS). Octal there; octal here where it ends in B.
 *   [NPL]  read in the SINTRAN source, file and octal address given
 *          (CC-P2-N500, MP-P2-N500, RP-P2-N500, 5P-P2-MON60).
 *   [MIC]  read in the microprogram listing MICRO-5800-B30, octal address given.
 *   [MEAS] measured on a running SINTRAN, date given.
 *   [EMU]  a choice this emulator makes; the real machine does something else
 *          or more.
 *
 * 1. WHAT A MESSAGE IS
 *
 * One message block per ND-500 process, 200B words long [SYM 55MES], in the
 * shared memory. SINTRAN fills it in to ask the ND-5000 for something; the
 * microprogram writes the answer into the SAME block. A process has one block
 * for its whole life, so "the message of process 3" is always the same address
 * (MEAS 12-OCT-2026: process 0 the swapper at 0x8D30, process 1 at 0x8E30,
 * 2 at 0x8F30, 3 at 0x9030, 4 at 0x9130 - 0x100 bytes apart).
 *
 * All offsets are WORDS (16 bits) from the message base. Six words lie IN FRONT
 * of the base and have negative offsets.
 *
 * 2. THE WORDS IN FRONT OF THE BASE
 *
 *  -10B,-7  500TU  [SYM 177770B] Double word. CPU time the process has used, in
 *                  20 ms units. SINTRAN's clock routine adds 1 every 20 ms to the
 *                  process named in the mailbox cell X5PRO [NPL MP 500H2].
 *                  The ND-5000 side must never write here. A write of 1 into the
 *                  high word showed as "21 min 50.7 s" in PROCESS-STATUS
 *                  [MEAS 12-OCT-2026].
 *  -6       5CPUN  [SYM 177772B] The number of the CPU the message is ASSIGNED
 *                  to: 1 for station 070B, 2 for 071B, 3, 4. Written by SINTRAN
 *                  when it queues the message [NPL RP 127502, MP 133674]. Read by
 *                  the microprogram before it takes a message: equal to its own
 *                  number (control store cell 025B, SAMSON_CPU) -> take it
 *                  [MIC 015151-015154]. Not equal -> the microprogram looks at
 *                  5MSFL and at the other CPU and may take the message over by
 *                  writing its own number here [MIC 015155-015174].
 *                  [EMU] not equal -> the message is left alone. No take-over.
 *                  [MEAS 12-OCT-2026] 430 messages, every one had 5CPUN 1..4.
 *  -5       5PRIO  [SYM 177773B] Priority. Higher runs first. The swapper is
 *                  given 300 [NPL MP 133671]; a break sets 5BRKPRIOR
 *                  [NPL MP 141660]; the time slicer counts it up and down
 *                  [NPL MP 134213-134220]. SINTRAN keeps each CPU's queue sorted
 *                  on it [NPL CC 022566-022617] and compares it with the priority
 *                  of the executing process to choose between the doorbell and
 *                  kick 1 [NPL MP ACT50, 145451-145520]. The ND-5000 side does not
 *                  use it.
 *  -1       5MSFL  [SYM 177777B] Flag bits, numbered as SINTRAN numbers them
 *                  (bit 0 = least significant; octal bit numbers from [SYM]):
 *                    bit 15B (0x2000) 5IEXQUEUE  the message is in an execution
 *                                     queue. Set by ITO500XQ, cleared by IFM500XQ
 *                                     [NPL CC 022554, 022670].
 *                    bit 14B (0x1000) 5SYSRES    system-reserved process
 *                                     [NPL 5P 030620].
 *                    bit 13B (0x0800) 5CPUBOUND  the process may only run on the
 *                                     CPU in 5CPUN. Without it SINTRAN moves the
 *                                     message to the active CPU with the shortest
 *                                     queue [NPL RP 127411-127450].
 *                    bit 11B (0x0200) 52ESCSET, bit 10B (0x0100) 5IBRK  break /
 *                                     escape handling [NPL 5P 032644, 035165].
 *                    bit 6   (0x0040) 5ITMQUEUE  the message is in the time queue
 *                                     [NPL CC 022710].
 *                  [MEAS 12-OCT-2026] a user process after CHANGE-CPU: 0x6800
 *                  (in queue, CPU bound, and bit 0x4000 whose name is UNKNOWN).
 *                  The swapper: 0x3000 (in queue, system reserved, NOT bound - so
 *                  the swapper moves between CPUs). The watchdog message: 0x3800, which is what
 *                 SINTRAN builds: 5SYSRES + 5CPUBOUND, priority 77776B, one per
 *                 CPU [NPL RP 131507-131526].
 *                  The ND-5000 side only reads this word.
 *
 * 3. THE WORDS FROM THE BASE
 *
 *   0,1     LINK   [SYM 0,1] Double word. The next message in the queue, as a
 *                  byte offset in the shared memory; -1 (0xFFFFFFFF) ends the
 *                  queue. Written by SINTRAN only, under the semaphore X5SEM
 *                  [NPL CC ITO500XQ 022620-022653]. The microprogram follows it
 *                  [MIC MSG_NEXT 017455-017457].
 *   2       N5STA  [SYM 2] Where the message is in its life. Low 13 bits:
 *                    1 MSGN500   for the ND-5000, not taken yet. The ONLY value
 *                                the microprogram takes [MIC MSG_LINK1 015141-44].
 *                    2 WAITING   taken, the process is executing or parked.
 *                    3 ANSWER    answered, for SINTRAN.
 *                    4 5ERANSWER answered with an error.
 *                  Top 3 bits (0xE000) are power-fail flags; whoever writes the
 *                  low bits keeps them. The microprogram's OCB_CLNUP writes 2 back
 *                  to 1 when a kick takes the process off the CPU [MIC 025570].
 *   3       SENDE  [SYM 3] The process that sent it. -1 for SINTRAN's watchdog
 *                  message [NPL RP 131513].
 *   4       5RECE  [SYM 4] The RECEIVER: the ND-500 process number the message is
 *                  for. 0 is the swapper. This is the number the microprogram
 *                  writes to X5PRO when it starts the process [MIC 015207].
 *                  NAMING TRAP: the emulators call this word "X5CPU". X5CPU is a
 *                  different thing - word 4 of the MAILBOX EXTENSION BLOCK, where
 *                  1 means "microprogram active" [SYM X5CPU=4, MPACT=1]. The two
 *                  have the same offset in two different structures. It is a
 *                  process number, never a CPU number; the CPU number is 5CPUN.
 *   5       5CCLR  [SYM 5] The value the CPU's cache-clear counter X5CCL had when
 *                  this process last ran there. SINTRAN subtracts the CPU's
 *                  current X5CCL from it; not zero means the process has to be
 *                  placed again [NPL MP 145347-145357]. NAMING TRAP: the
 *                  emulators call this word "X5ACT". X5ACT is word 5 of the
 *                  extension block, the doorbell. The ND-5000 side does not touch
 *                  the message's word 5.
 *   6       MICFU  [SYM 6] What the ND-5000 is asked to do. The values and their
 *                  routines are listed with the servicer. The ones that put a
 *                  process on the CPU: 22B start process 0, 23B 3START,
 *                  24B 3MONCO (continue after a monitor call), 25B 3TRACO
 *                  (continue after a trap), 26B 3WMONCO. The ones that only move
 *                  data: 1 3RMICV, 10B/11B DMEMRD/DMEMWR (read/write the memory
 *                  of the process named in 5RECE - the microprogram loads that
 *                  process's context first [MIC 015336] but does NOT execute it),
 *                  13B/14B, 30B/31B, 34B/35B.
 *   7,10B   N500A  [SYM 7] ND-500 address of a transfer, double word; in a
 *                  swapper message the two words are SWFUN and SWRST instead.
 *   11B     N100A / STOPR / ACPRO / KFLIP  [SYM 11B, four names] One word, four
 *                  meanings. Going to the ND-5000 in a transfer: ND-100 address.
 *                  Coming back after a stop: the stop reason (1 = monitor call,
 *                  2 = trap). In a continue: the K flag to give the process.
 *   12B     NUMPA  [SYM 12B] Going back: number of parameters of the monitor
 *                  call. In a continue: a mask of which results to write back.
 *   13B     NRBYT / MCNO / FUNCV  [SYM 13B, three names] Transfer: byte count.
 *                  Stop: the monitor call number. Continue: the function value.
 *   14B     MSWMC  [SYM 14B] swapper monitor call field.
 *   16B     TRAPN  [SYM 16B] trap number of a trap stop.
 *   37B     SMCNO  [SYM 37B] SINTRAN's saved copy of MCNO [NPL MP 1302].
 *   147B    PLINK  [SYM 147B] Link to the PREVIOUS message, SINTRAN's own
 *                  [NPL CC 022633-022643]. The ND-5000 side does not use it.
 *
 * 4. THE QUEUES AND THE MAILBOX
 *
 * There is one mailbox header for the machine and one extension block per CPU,
 * 256 bytes apart (extension block of CPU n = header + n * 256).
 *
 * Header [SYM]:   0 X5SEM   the semaphore every queue change is made under.
 *                           SINTRAN takes it with TSET / TSETP [NPL CC SLOCK
 *                           023667], the microprogram with LOCK_QUE [MIC 025442].
 *                 3 X5HEN, 4 X5FYL, 5 X5MXF, 6-7 X5FIF  the ring the answered
 *                           messages are handed back to SINTRAN in.
 *                 47B X5RES who holds X5SEM: -1 is the ND-100 [NPL CC 024003].
 * Extension block [SYM]:
 *                 0-1 X5BEX  head of THIS CPU's execution queue, -1 = empty.
 *                 2-3 X5NAC  the next CPU's extension block.
 *                 4   X5CPU  1 (MPACTIVE) = this CPU's microprogram runs. SINTRAN
 *                           only gives work to a CPU that has 1 here [NPL MP
 *                           145435, RP 127436]. [MEAS 12-OCT-2026] it is 1 without
 *                           the emulator writing it.
 *                 5   X5ACT  the doorbell: SINTRAN writes 0 to say "walk your
 *                           queue" when the CPU is idle [NPL MP ACT51 145500].
 *                 6   X5PRO  the process this CPU executes, -1 idle, written by
 *                           the microprogram [MIC PRNOWR 025416].
 *                 7   X5STA  the CPU's octobus station.
 *                 10B X5CLR, 11B X5CCL  cache-clear request and its counter.
 *
 * Each CPU has a queue of its own (X5BEX), sorted on 5PRIO, and its length is in
 * SINTRAN's own LEXQUEUE. A message is in exactly one queue.
 *
 * 5. THE LIFE OF A MESSAGE, AND WHAT THE EMULATOR MUST HONOUR
 *
 *   a. SINTRAN picks the CPU: the one in 5CPUN if 5CPUBOUND, else the active CPU
 *      with the shortest queue, and writes 5CPUN [NPL RP 127404-127510].
 *   b. Under X5SEM it links the message into that CPU's queue (ITO500XQ) and
 *      sets N5STA = 1.
 *   c. If that CPU is idle (X5PRO = -1) it writes X5ACT = 0. If the CPU executes
 *      a process of lower priority it sends kick 1 [NPL MP ACT50-ACT52].
 *   d. The microprogram, idle or kicked, walks its queue under LOCK_QUE. On a
 *      kick it first takes the executing process off the CPU: saves its
 *      context, writes its message back to N5STA = 1 [MIC ACTIVATE 024723].
 *   e. It takes the first message with N5STA = 1 and its own number in 5CPUN,
 *      writes N5STA = 2 and does what MICFU says. If that starts a process the
 *      walk ENDS there: the microprogram executes the process and is not in its
 *      message loop again until the process stops.
 *   f. At a stop it saves the context, writes the stop record into the message,
 *      N5STA = 3, puts the message in the ring and interrupts the ND-100.
 *
 * What follows from this for an emulator with several CPUs:
 *   - A walk must not take a message whose 5CPUN is another CPU's. A walk can
 *     REACH such a message, because SINTRAN relinks the queues while a CPU
 *     walks and this emulator does not hold X5SEM for the whole walk.
 *   - A walk must stop when one of its messages has started a process.
 *   - A process is not tied to a CPU. The swapper is not CPU bound, so its
 *     message can go to another CPU's queue between two stops. Everything a
 *     process needs to continue must therefore be where every CPU finds it:
 *     the context block in the shared memory, and what the block does not
 *     carry in one table for all CPUs. A CPU that still holds an old copy of a
 *     process must notice that another CPU has executed it since, and reload.
 *   - DMEMRD and DMEMWR load the named process's context so that its addresses
 *     translate. That must not release the process to execute.
 */

/** A link value meaning "no next block", and an empty X5BEX. NOT zero. */
#define NDBUS_MSG_LINK_NONE 0xFFFFFFFFu

/* ---- message block field WORD offsets ----------------------------------- */
#define NDBUS_MSG_LINK_WORD  0u  /**< words 0-1, 32-bit byte offset of the next block */
#define NDBUS_MSG_N5STA      2u  /**< message status */
#define NDBUS_MSG_SENDE      3u  /**< sender; watchdog is -1 */
#define NDBUS_MSG_X5CPU      4u  /**< 5RECE: receiver PROCESS number, 0 = swapper. NOT a CPU number */
#define NDBUS_MSG_X5ACT      5u  /**< 5CCLR: the CPU's X5CCL when the process last ran. NOT the doorbell */
#define NDBUS_MSG_MICFU      6u  /**< micro-function; decides the 7/10B overlay arm */
#define NDBUS_MSG_N500A      7u  /**< ND-500 address HIGH half, or SWFUN. OVERLAY */
#define NDBUS_MSG_SWRST      8u  /**< 10B: address LOW half, or SWRST. OVERLAY */
#define NDBUS_MSG_STOPR      9u  /**< 11B */
#define NDBUS_MSG_NUMPA     10u  /**< 12B */
#define NDBUS_MSG_MCNO      11u  /**< 13B */
#define NDBUS_MSG_MSWMC     12u  /**< 14B */
#define NDBUS_MSG_TRAPN     14u  /**< 16B */
#define NDBUS_MSG_SMCNO     31u  /**< 37B */
#define NDBUS_MSG_SWPFU     65u  /**< 101B */
#define NDBUS_MSG_SWPST     67u  /**< 103B */

/* ---- the 26B (3WMONCO) overlay arm -------------------------------------
 *
 * MICFU selects what the middle of the block means, exactly as it already does
 * for N500A/SWRST above. On a 3WMONCO these three fields describe the answer-data
 * block the microcode copies into the process before it resumes, and they sit ON
 * TOP of MCNO and TRAPN - the same halfwords, a different meaning. Reading a 26B
 * block through the MCNO/TRAPN names, or a 24B block through these, gets a
 * plausible wrong number rather than an error, which is why both arms are named
 * instead of either being spelled out as a bare offset at the point of use.
 *
 * Offsets from the microcode at 015752-016004, cross-checked against the
 * reference's own decode (Nd500MicrocodeServicer.cs, N5MicroFunction.
 * WaitMonitorCall) and the swapper field dossier's 26ADD@0o15-0o16 / 26NRB@0o17.
 */
#define NDBUS_MSG_26ADD_HI  13u  /**< 15B: process address to copy to, HIGH half. Overlays MCNO */
#define NDBUS_MSG_26ADD_LO  14u  /**< 16B: ... LOW half. Overlays TRAPN */
#define NDBUS_MSG_26NRB     15u  /**< 17B: byte count; >= 0x2000 skips the copy */
#define NDBUS_MSG_ABUFA_HI  96u  /**< 140B: ND-100 WORD address of the source, HIGH half */
#define NDBUS_MSG_ABUFA_LO  97u  /**< 141B: ... LOW half. A WORD address - shift to get bytes */

/**
 * @brief Read a message block's LINK: the byte offset of the next block.
 * @param pool      The shared pool.
 * @param msg_byte  Pool byte offset of the block.
 * @return The next block's byte offset, or NDBUS_MSG_LINK_NONE at the end.
 */
uint32_t ndbus_msg_link(const NdbusPool *pool, uint32_t msg_byte);

/**
 * @brief Set a message block's LINK.
 * @param pool     The shared pool.
 * @param msg_byte Pool byte offset of the block.
 * @param next     Byte offset of the next block, or NDBUS_MSG_LINK_NONE.
 * @return true on success; false when the block is outside the pool.
 */
bool ndbus_msg_set_link(NdbusPool *pool, uint32_t msg_byte, uint32_t next);

/**
 * @brief Read one 16-bit field of a message block.
 * @param pool      The shared pool.
 * @param msg_byte  Pool byte offset of the block.
 * @param word      An NDBUS_MSG_* word offset.
 * @return The field, or 0 when outside the pool.
 */
uint16_t ndbus_msg_read(const NdbusPool *pool, uint32_t msg_byte, uint32_t word);

/**
 * @brief Write one 16-bit field of a message block.
 * @param pool      The shared pool.
 * @param msg_byte  Pool byte offset of the block.
 * @param word      An NDBUS_MSG_* word offset.
 * @param value     The value.
 * @return true on success; false when outside the pool.
 */
bool ndbus_msg_write(NdbusPool *pool, uint32_t msg_byte, uint32_t word, uint16_t value);

/**
 * @brief Post a message block onto this CPU's X5BEX chain.
 *
 * Takes the bus lock, so it is mutually exclusive with a concurrent take and
 * with a guest TSET on X5SEM. The block is linked at the HEAD, which is O(1) and
 * is why the chain needs no tail pointer.
 *
 * @param mbx      The receiving CPU's mailbox view.
 * @param msg_byte Pool byte offset of the block to post.
 * @return true on success; false when unattached or the block is outside the
 *         pool.
 */
bool ndbus_msgqueue_post(NdbusMailbox *mbx, uint32_t msg_byte);

/**
 * @brief Take the next message block off this CPU's X5BEX chain.
 *
 * Takes the bus lock. Returns blocks in the reverse of the order they were
 * posted, because posting is at the head - which matters only to a test, since
 * the microcode walks the chain rather than relying on an order.
 *
 * @param mbx           The CPU's mailbox view.
 * @param out_msg_byte  Receives the block's byte offset. Untouched when empty.
 * @return true when a block was taken; false when the chain is empty.
 */
bool ndbus_msgqueue_take(NdbusMailbox *mbx, uint32_t *out_msg_byte);

/**
 * @brief How many blocks are on this CPU's chain.
 *
 * Walks the chain under the bus lock, and stops at @p max_walk links to bound a
 * chain that a bug has made circular - a cycle would otherwise hang the caller
 * with no indication of why.
 *
 * @param mbx      The CPU's mailbox view.
 * @param max_walk Largest number of links to follow.
 * @return The count, or max_walk when the chain is longer or circular.
 */
int ndbus_msgqueue_depth(const NdbusMailbox *mbx, int max_walk);

#endif /* NDBUS_MSGQUEUE_H */
