/*
 * Copied verbatim (apart from this note) from the pcc-nd500 tree,
 * src/include/nd500/desc.h, so nd500x carries the DESC layout itself
 * instead of reaching outside the repository. Keep the two in step.
 */
/*
 * DESCRIPTION-FILE:DESC - byte-level entry layout
 *
 * Source: NDInsight repository, SINTRAN/File-Formats/DESCRIPTION-FILE-FORMAT.md
 *         and SINTRAN/File-Formats/desc-format.json, whose segment-entry
 *         offsets come from NDInsight
 *         SINTRAN/ND500/nd-500-mon/CARVE-ANSWER-DESC-FIELD-OFFSETS-2026-08-11.md
 *
 * DESCRIPTION-FILE:DESC is a per-SINTRAN-user index for the OLD ND-500(0)
 * domain format (:PSEG/:DSEG/:LINK files, not the newer self-contained
 * :DOM format - see nd500/dom.h for that one). It lets the old Linkage-
 * Loader (NLL) resolve a bare domain name to its files and bookkeeping.
 *
 * VERIFICATION STATUS (2026-08-17)
 *
 * The segment-entry field offsets below are no longer guesses. They were
 * read out of the ND-500 Loader/Debug Monitor's own code (MON-DEBUG:PROG
 * J04), which reads one raw 192-byte segment entry into a fixed buffer at
 * bank-1 word 037705B and then prints each field immediately after
 * printing that field's own name. Each label print is followed by exactly
 * one field load, so the label-to-offset pairing is not open to
 * interpretation. Each entry in the table below cites the loading
 * instruction's address in that program.
 *
 * An earlier version of this header said an exhaustive byte scan had found
 * no match for PLB/PSIZE/DLB/DSIZE at any offset. That scan could not have
 * worked, and the reason is the single most important fact about this
 * format:
 *
 *     THE SIZE FIELDS STORE THE LAST BYTE INDEX, NOT A BYTE COUNT.
 *
 *     PLB + PSIZE + 1 = .pseg file size
 *     DLB + DSIZE + 1 = .dseg file size
 *
 * Satisfied by 48 checks over 24 segment entries in 13 DESC files carried by
 * 13 unrelated vendor floppies (twelve products, 1982-1989), each derived size
 * checked against the real .pseg/.dseg byte count from the image directory.
 * Zero mismatches. Most of that corpus was collected AFTER the offsets were
 * fixed, so it is evidence the conclusion was not fitted to.
 *
 * Take those file sizes from the image directory listing, never from extracted
 * copies: a file with a non-zero byte count but zero allocated pages extracts
 * as empty and then looks exactly like a format anomaly.
 *
 * The scan was looking for the file sizes themselves,
 * which are never present. The same inclusive-last-index convention appears
 * twice more in the monitor's own reader (it reads the 192-byte segment
 * record with a length word of 277B = 191, and the 56-byte domain entry
 * with 67B = 55), so this is the house style of the program that owns the
 * file rather than a coincidence.
 *
 * Note that the monitor only ever displays these fields - it never adjusts
 * them. The +1 above is derived from the files, not from read-side code.
 * The writer is NLL (the ND-500 domain LINKAGE-LOAD-H02). Its :LINK
 * serializer was carved on 2026-08-17 and its SMAX call confirms the same
 * inclusive-last-index convention on a DIFFERENT file - byte count = max byte
 * pointer + 1 - but the code that writes these particular DESC size fields
 * was not located. Treat the +1 as file-derived, cross-supported.
 *
 * The two items this header used to list as unverified are both settled, by
 * the same method, in NDInsight
 * SINTRAN/ND500/nd-500-mon/CARVE-ANSWER-FOUR-OPEN-QUESTIONS-2026-08-17.md:
 *   - The domain-entry fields past DNAME are now code-proven and are exposed
 *     below. Two of the manual's placements were wrong; see that table.
 *   - Segment-entry bytes 74-84: THE MANUAL WAS RIGHT, and this header's
 *     earlier "two counted byte strings" reading is retracted. Word 37B is
 *     COMSEGNO, a count of common segments, not a character count - the
 *     monitor prints 37B itself under its own "$Comsegno: " label, and the
 *     same count bounds four parallel arrays, one of them a uint16 array and
 *     one of them 12-byte elements. A character count cannot bound those.
 *
 * What is still NOT verified, and is therefore still omitted below:
 *   - Process Entry internals beyond "1 byte", and the entire Symbol Entry
 *     layout.
 *   - The segment-entry flags word at byte 60 - individual bits are tested at
 *     015743B-016052B but no bit meaning was decoded.
 *
 * Do NOT add unverified offsets below without also updating desc-format.json's
 * "verified" markers - the point of the split is that callers can trust every
 * field this header exposes.
 */

#ifndef ND500_DESC_H
#define ND500_DESC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read big-endian values from a byte pointer (same convention as nd500/dom.h) */
static inline uint32_t desc_read32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static inline uint16_t desc_read16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/*============================================================================
 * Entry sizes - CONFIRMED, and now also confirmed from the monitor's own
 * record readers (segment record length word 277B = 191 = 192-1, domain
 * entry length word 67B = 55 = 56-1).
 *============================================================================*/
#define DESC_PROCESS_ENTRY_SIZE  1     /* size stated by the manual, not independently verified */
#define DESC_DOMAIN_ENTRY_SIZE   56    /* CONFIRMED */
#define DESC_SEGMENT_ENTRY_SIZE  192   /* CONFIRMED */

/*============================================================================
 * File geometry - CONFIRMED from READ-DOMAIN-ENTRY at 013454B, which
 * computes a domain entry's file position from its index as
 *
 *     position = 56*index + 256*(index div 32 + 1)
 *
 * (multiply constant 70B = 56 at 013520B, divide by 40B = 32 and add 1 at
 * 013473B-013475B, shift left 8 = *256 at 013476B). That is 2048-byte pages,
 * each one a 256-byte header followed by 32 domain entries of 56 bytes:
 * 56*32 + 256 = 2048 exactly. Entry 0 therefore starts at byte 256.
 *
 * This also explains the old puzzle that the documented domain-entry fields
 * sum to 54 while real entries sit 56 bytes apart - the entry is 56 because
 * 32 of them plus the page header fill a page.
 *
 * The walker in the monitor loops indexes 0..253 (limit 376B = 254 at
 * 016504B).
 *============================================================================*/
#define DESC_PAGE_SIZE            2048
#define DESC_PAGE_HEADER_SIZE     256
#define DESC_DOMAINS_PER_PAGE     32
#define DESC_MAX_DOMAIN_INDEX     253   /* monitor's own loop limit is 254 exclusive */

/* File byte position of domain entry `index`. */
#define DESC_DOMAIN_ENTRY_POSITION(index) \
    ((uint32_t)(DESC_DOMAIN_ENTRY_SIZE) * (uint32_t)(index) + \
     (uint32_t)(DESC_PAGE_HEADER_SIZE) * ((uint32_t)(index) / (uint32_t)(DESC_DOMAINS_PER_PAGE) + 1u))

/*============================================================================
 * Domain Entry - all 56 bytes, CODE-PROVEN
 *
 * Word 0 is not a "link location" in some abstract sense: it is the file
 * byte position of this domain's FIRST segment entry, read at 016412B and
 * passed straight to the record reader as a seek position. Segment entries
 * are a singly linked list, not an indexed array.
 *
 * The fields past DNAME come from the domain-entry printer at
 * 014035B-014520B in MON-DEBUG:PROG J04, same method as the segment entry:
 * one label print, then exactly one field load off the domain-entry buffer
 * at bank-1 word 037651B.
 *
 *   byte  word     width   field          label printed          load
 *   ----  -------  ------  -------------  ---------------------  -------
 *      0   0       32-bit  SEGLINK        "$Segptr:"             014325B
 *      4   2       16 by   DNAME          (name loop)            014061B
 *     20  12B       6 by   CHILDDOMAINS   "$Child domains   : "  014407B
 *     26  15B hi    8-bit  MOTHER         "$Owner:"              014157B
 *     27  15B lo    8-bit  CHILDINDEX     "  Childindex:"        014176B
 *     28  16B      16-bit  FLAGS+PRIOR    (bits, see below)      014216B
 *     30  17B      32-bit  STADR          "  Start address:"     014132B
 *     34  21B      32-bit  ENABLEINT      "$Enableint:"          014261B
 *     38  23B      32-bit  THA            "  THA:"               014277B
 *     42  25B      32-bit  SYSENABL       "  Sysenable:"         014312B
 *     46  27B      16-bit  (unprinted)    (none)                 (none)
 *     48  30B      32-bit  PBITMAP        "  PSEG use:"          014340B
 *     52  32B      32-bit  DBITMAP        "  DSEG use:"          014353B
 *
 * Words 0-33B = 56 bytes exactly, and the previously unexplained 2-byte
 * shortfall in the manual's field list is the unprinted word at byte 46.
 *
 * TWO OF THE MANUAL'S PLACEMENTS ARE WRONG:
 *   - PBITMAP/DBITMAP are at bytes 48/52, not 46/50.
 *   - procPrior and flag are not two separate bytes. They are one 16-bit
 *     word, with the flag bits at the top and an 8-bit priority spanning the
 *     byte boundary.
 *
 * CHILDINDEX at byte 27 does triple duty: it is also the iteration count for
 * the child-domain list (014375B) and the operand of the "$No child domains"
 * test (014357B).
 *
 * The bitmap reading is confirmed by two independent segment numbers: every
 * sample with STADR in segment 1 has PBITMAP = DBITMAP = 2, and NLL's own
 * entry has STADR 0xB0000DD1 (segment 22) with PBITMAP = DBITMAP = 2^22.
 * Nothing available exercises the child machinery, the priority field, or the
 * alton/occup flag bits - those are code-proven only, with no file witness.
 *============================================================================*/
#define DESC_DOMAIN_SEGLINK_OFFSET  0   /* uint32, CONFIRMED - file byte position of first segment entry, 0 = none */
#define DESC_DOMAIN_DNAME_OFFSET    4   /* up to 16 bytes ASCII, CONFIRMED */
#define DESC_DOMAIN_DNAME_SIZE      16
#define DESC_DOMAIN_DNAME_TERMINATOR 0x27  /* apostrophe, undocumented in the manual; absent when the name fills all 16 bytes */

#define DESC_DOMAIN_CHILDDOMAINS_OFFSET 20  /* 6 bytes, one child domain index each; CHILDINDEX is the count */
#define DESC_DOMAIN_CHILDDOMAINS_SIZE    6
#define DESC_DOMAIN_MOTHER_OFFSET       26  /* uint8 - 0xFF on every root domain in all 13 samples */
#define DESC_DOMAIN_CHILDINDEX_OFFSET   27  /* uint8 - also the child-list loop count */
#define DESC_DOMAIN_FLAGPRIOR_OFFSET    28  /* uint16 - flag bits + priority, see the macros below */
#define DESC_DOMAIN_STADR_OFFSET        30  /* uint32 - domain start address */
#define DESC_DOMAIN_ENABLEINT_OFFSET    34  /* uint32 */
#define DESC_DOMAIN_THA_OFFSET          38  /* uint32 */
#define DESC_DOMAIN_SYSENABL_OFFSET     42  /* uint32 */
#define DESC_DOMAIN_PBITMAP_OFFSET      48  /* uint32 - bit N set = program segment N in use */
#define DESC_DOMAIN_DBITMAP_OFFSET      52  /* uint32 - bit N set = data segment N in use */

/* Bit accessors for the packed word at byte 28. The three flag names are the
 * literal strings the monitor prints (bank 2 bytes 0x81AE/0x81B4/0x81BA) -
 * expansions like "ALT on" or "occupied" would be guesses, so they are not
 * written here. DINUSE is set on every used entry and no unused one across
 * all 13 sample files, which is how an in-use slot is told apart from an
 * empty one. */
#define DESC_DOMAIN_FLAG_ALTON(w)    (((uint16_t)(w) >> 15) & 1u)
#define DESC_DOMAIN_FLAG_DINUSE(w)   (((uint16_t)(w) >> 14) & 1u)
#define DESC_DOMAIN_FLAG_OCCUP(w)    (((uint16_t)(w) >> 13) & 1u)
#define DESC_DOMAIN_PRIOR(w)         (((uint16_t)(w) >> 5) & 0xFFu)  /* bits 5-12, 014216B */

/*============================================================================
 * Segment Entry - the CONFIRMED fields
 *
 * Offsets 88 onward are from MON-DEBUG:PROG J04. Column "load" is the
 * address of the instruction that loads the field, off the segment-entry
 * buffer at 037705B.
 *
 *   byte  word   width   field        label printed   load
 *   ----  -----  ------  -----------  --------------  -------
 *      0    0    32-bit  SEGLINK      (none)          016436B
 *     88   54B   32-bit  PLB          "$PLB:"         014575B
 *     92   56B   32-bit  PSIZE        "  Psize:"      014610B
 *     96   60B   32-bit  DLB          "$DLB:"         014636B
 *    100   62B   32-bit  DSIZE        "  Dsize:"      014651B
 *    104   64B   32-bit  DEBUGINFO    "$Debuginfo:"   014677B
 *    108   66B   32-bit  DLINKDATE    "  Dlinkdate:"  014712B
 *    112   70B   16-bit  ABSFIXAD     "  Absfixad: "  014750B
 *    114   71B   16-bit  LOWLOGFIX    "$Lowlogfix:"   014763B
 *    126   77B   16-bit  PLOLOGFIX    "Plologfix:"    014623B
 *    128  100B   16-bit  PUPLOGFIX    "Puplogfix:"    014664B
 *
 * The odd print order is because the monitor prints three fields per line;
 * "$" in a label string is its new-line marker.
 *
 * Byte 60 (word 36B) is a flags word - individual bits are tested at
 * 015743B-016052B - and is not exposed here because no bit meaning was
 * decoded.
 *
 * Byte 62 (word 37B) is COMSEGNO, the number of common segments, and it
 * bounds four parallel arrays. This was the region this header once called
 * "two counted byte strings"; that reading is retracted. The monitor prints
 * word 37B under its own "$Comsegno: " label at 015150B-015151B, and two of
 * the four arrays it bounds are not byte arrays at all - COMSEGADDR is
 * word-indexed (015233B AAX 40) and ADDSGELEM has 12-byte elements (015372B
 * MPY by the constant 6 words at 015553B). A character count cannot bound
 * those. The byte-array prints go through 013301B, the number printer that
 * also prints the domain child list - never through the string path 013177B.
 *
 * Maximum common segments is 5: all four arrays and the element block size
 * for 5, and 5 * 12 = 60 bytes is exactly the remainder of the 192-byte entry
 * from byte 132.
 *============================================================================*/
#define DESC_SEGMENT_SEGLINK_OFFSET 0   /* uint32, CONFIRMED - file byte position of the NEXT segment entry, 0 = end of chain */
#define DESC_SEGMENT_SNAME_OFFSET   4   /* up to 54 bytes ASCII, CONFIRMED */
#define DESC_SEGMENT_SNAME_SIZE     54
#define DESC_SEGMENT_SNAME_TERMINATOR 0x27  /* apostrophe, undocumented in the manual, discovered empirically */

#define DESC_SEGMENT_PLB_OFFSET       88   /* uint32 - program segment lower bound */
#define DESC_SEGMENT_PSIZE_OFFSET     92   /* uint32 - LAST BYTE INDEX of the .pseg, not its size */
#define DESC_SEGMENT_DLB_OFFSET       96   /* uint32 - data segment lower bound */
#define DESC_SEGMENT_DSIZE_OFFSET    100   /* uint32 - LAST BYTE INDEX of the .dseg, not its size */
#define DESC_SEGMENT_DEBUGINFO_OFFSET 104  /* uint32 - varies across real entries (0, 4, 157323, 201075) */

/* CAUTION on the five below: the OFFSETS are proven (the monitor loads them
 * at the addresses in the table above), but the MEANINGS are only the
 * monitor's own printed labels. All five read zero in every one of the 26
 * real segment entries available, including shipped, linked products where a
 * link date ought to be set - while DEBUGINFO in the same entries does vary,
 * so the entries are not simply blank. So "DLINKDATE" is what the monitor
 * calls word 66B, not a decoded meaning. Do not present these as understood
 * values, and do not spend time confirming uniformly-zero readings - what
 * would settle them is a sample that sets one. */
#define DESC_SEGMENT_DLINKDATE_OFFSET 108  /* uint32 - labelled "Dlinkdate:"; zero in all 26 real entries */
#define DESC_SEGMENT_ABSFIXAD_OFFSET  112  /* uint16 - labelled "Absfixad:"; zero in all 26 real entries */
#define DESC_SEGMENT_LOWLOGFIX_OFFSET 114  /* uint16 - labelled "Lowlogfix:"; zero in all 26 real entries */
#define DESC_SEGMENT_PLOLOGFIX_OFFSET 126  /* uint16 - labelled "Plologfix:"; zero in all 26 real entries */
#define DESC_SEGMENT_PUPLOGFIX_OFFSET 128  /* uint16 - labelled "Puplogfix:"; zero in all 26 real entries */

/* The common-segment region, adjudicated 2026-08-17 in favour of the manual.
 * COMSEGNO bounds all four arrays; every one of the 13 sample DESC files has
 * COMSEGNO = 0, which is why file evidence alone could never have settled the
 * layout and the code had to. */
#define DESC_SEGMENT_MAX_COMMON_SEGMENTS 5
#define DESC_SEGMENT_COMSEGNO_OFFSET   62   /* uint16 - count of common segments, 015151B */
#define DESC_SEGMENT_COMSEGADDR_OFFSET 64   /* uint16[5] - 015233B, word-indexed */
#define DESC_SEGMENT_COMSEGSIZE_OFFSET 74   /* uint8[5] - 015270B; byte 79 is pad */
#define DESC_SEGMENT_N100SEGNO_OFFSET  80   /* uint8[5] - 015324B; byte 85 is pad */
#define DESC_SEGMENT_LOGFIX_OFFSET    130   /* uint16 - INDPLOG bits 10-15 (015164B), INDDLOG bits 5-9 (015200B) */
#define DESC_SEGMENT_ADDSGELEM_OFFSET 132   /* 5 elements of 12 bytes - 015372B; fills the entry to 192 */
#define DESC_SEGMENT_ADDSGELEM_SIZE    12

#define DESC_SEGMENT_INDPLOG(w)  (((uint16_t)(w) >> 10) & 0x3Fu)
#define DESC_SEGMENT_INDDLOG(w)  (((uint16_t)(w) >> 5)  & 0x1Fu)

/* The size rule. Both are exact for every entry checked so far; a caller
 * that has the real .pseg/.dseg file size can use these to confirm it is
 * reading a genuine segment entry rather than unrelated bytes. */
#define DESC_PSEG_FILE_SIZE(plb, psize)  ((uint64_t)(plb) + (uint64_t)(psize) + 1u)
#define DESC_DSEG_FILE_SIZE(dlb, dsize)  ((uint64_t)(dlb) + (uint64_t)(dsize) + 1u)

#ifdef __cplusplus
}
#endif

#endif /* ND500_DESC_H */
