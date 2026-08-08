/*
 * Trap conformance - which trap does an instruction actually raise?
 *
 * WHY THIS EXISTS
 *
 * On 2026-08-07 an audit of all 176 trap call sites under src/cpu found 19
 * WRONG trap numbers (fixed in ee3ec62 and 694bac2). Every one survived
 * because nothing asserted which trap fired: test_instruction_validation runs
 * 40,064 generated cases and checks RESULTS only, so an instruction that
 * computed the right answer and then reported the wrong trap passed happily.
 *
 * The consequences were not theoretical. NDIX arms a specific set of traps
 * (kernel/MASTER/machine/trap.h: T_CMTE1 = 0xF413D800), and the bugs
 * systematically reported conditions the guest IGNORES using bits the guest
 * ACTS on:
 *
 *   - ADD2 and friends raised IVO (bit 11, armed) on integer overflow instead
 *     of O (bit 9, not armed), so a condition NDIX deliberately ignores was
 *     delivered as an Invalid Operation trap on every timer tick.
 *   - HCONV/BYCONR/HCONR/WCONR raised IOV (bit 16, armed) for conversion
 *     overflow - which fires on every char/short narrowing PCC emits.
 *   - BYCONV had been "fixed" by deleting its trap entirely, treating the
 *     symptom of a wrong trap number in its siblings.
 *
 * A test that asserted the exact bit would have caught all three. This is it.
 *
 *
 * HOW A RAISED TRAP IS OBSERVED  (cpu.c raise_trap, ~line 1255)
 *
 *   cpu->ST1 |= (uint32_t)(trapBit & 0xFFFFFFFF);   /_ bits 0..31  _/
 *   cpu->ST2 |= (uint32_t)(trapBit >> 32);          /_ bits 32..41 _/
 *
 * The status bit is set for EVERY trap. Dispatch is separate: raise_trap only
 * calls nd500_trap_set_state()/invoke_trap_handler() when the bit is enabled
 * in OTE/MTE. That split gives the two passes below.
 *
 *   Pass 1 - nothing armed (OTE = 0). No dispatch, so ST1/ST2 accumulate the
 *            raised bits and nothing else. Clear them first and the delta IS
 *            the exact set of conditions the instruction reported. This is the
 *            assertion that matters: equality, never "a trap happened".
 *
 *   Pass 2 - NDIX's real mask (T_CMTE1). Asserts whether the guest would
 *            actually see it. That is what separates a cosmetic mismatch from
 *            a guest-visible bug, and it is the number that mattered in all
 *            three cases above.
 *
 *
 * ONE THING TO KNOW ABOUT THE STATUS WORD
 *
 * The arithmetic FLAGS and the TRAP CONDITION bits are the same bits - there
 * is one status register, not two:
 *
 *   ND500_FLAG_O  (1u << 9)  == TRAP_O    (1ULL << 9)
 *   ND500_FLAG_DZ (1u << 12) == TRAP_DZ   (1ULL << 12)
 *   ND500_FLAG_FU (1u << 13) == TRAP_FU   (1ULL << 13)
 *   ND500_FLAG_FO (1u << 14) == TRAP_FO   (1ULL << 14)
 *
 * That is architecturally right - the status bit IS the trap condition, and
 * OTE decides whether it dispatches - but it means "the O flag is set" and
 * "TRAP_O was raised" are indistinguishable by reading ST1. So each case
 * states the whole expected status word under a care-mask rather than
 * pretending the two are separable. Z (5), C (6), S (7) and K (8) do not
 * collide with any trap bit and are asserted normally.
 *
 *
 * AUTHORITY
 *
 * Every case carries its citation. Where the manual is silent or the audit
 * could not settle a question, the case is marked KNOWN GAP and its expected
 * value is what the code does TODAY, with the disagreement spelled out - a
 * guess must never be encoded as an assertion.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

/* Instructions under test. Called directly, the way test_solo_traps.c drives
 * SOLO/TUTTI - decoding real instruction bytes would be testing the decoder. */
extern void nd500_instr_Add2  (Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Div2  (Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Byconv(Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Hconv (Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Wconv (Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Getbi (Nd500Cpu*, const Nd500FetchedInstruction*);
extern void nd500_instr_Axi   (Nd500Cpu*, const Nd500FetchedInstruction*);

/* NDIX's trap-enable mask, kernel/MASTER/machine/trap.h:46.
 * ARMED:     11 IVO, 12 DZ, 14 FO, 15 BO, 16 IOV, 17 SIT, 20 BPT, 26 IX,
 *            28 STU, 29 PRT, 30 DT, 31 DE
 * NOT armed:  9 O, 13 FU, 18 BT, 19 CT, 21-25, 27 STO
 * T_CTEMM1 is 0, so a child domain cannot add bits to this. */
#define NDIX_T_CMTE1 0xF413D800ULL

#define MEMORY_SIZE (4 * 1024 * 1024)
#define TEST_PC     0x2000u

static int passed = 0, failed = 0, gaps = 0;

/* ------------------------------------------------------------------------ */

typedef struct TrapCase TrapCase;
struct TrapCase {
    const char *name;
    /* Build CPU state and the fetched instruction. */
    void      (*setup)(Nd500Cpu *cpu, Nd500FetchedInstruction *fi);
    /* The instruction itself. */
    void      (*exec)(Nd500Cpu *cpu, const Nd500FetchedInstruction *fi);

    /* Expected status word, ST1 | (ST2 << 32), asserted only under care_mask.
     * Both flags and trap bits live here - see the header note. */
    uint64_t    expect_status;
    uint64_t    care_mask;

    /* The trap this case is ABOUT, used for the NDIX-mask pass. 0 = the case
     * asserts that NO trap is raised. */
    uint64_t    trap_bit;

    /* Destination check. result_reg 1..4 names an integer register; 0 skips
     * the check (cases where the manual does not pin a result). */
    uint8_t     result_reg;
    uint32_t    expect_result;

    const char *authority;

    /* 1 = a documented gap: the manual asks for a trap the code does not
     * raise. expect_status describes TODAY'S behaviour so the suite stays
     * green; the mismatch is reported as KNOWN GAP, not as a pass. */
    int         known_gap;
    const char *gap_note;
};

/* ---- operand builders ---------------------------------------------------- */

static void reg_op(Nd500OperandDecoded *op, uint8_t reg) {
    memset(op, 0, sizeof *op);
    op->mode = ND500_ADDR_REGISTER;
    op->reg  = reg;
}

/* A fetched instruction with two register operands - enough for everything
 * here, and it keeps the test about the trap instead of about addressing. */
static void two_reg(Nd500FetchedInstruction *fi, uint16_t opcode,
                    Nd500DataType dtype, uint8_t a, uint8_t b) {
    memset(fi, 0, sizeof *fi);
    fi->address       = TEST_PC;
    fi->opcode        = opcode;
    fi->operand_count = 2;
    fi->data_type     = dtype;
    reg_op(&fi->operands[0], a);
    reg_op(&fi->operands[1], b);
}

/* ======================================================================== */
/* CONFIRMED CASES - the manual settles these                               */
/* ======================================================================== */

/* ---- ADD2, integer overflow -> O (bit 9) --------------------------------
 * This is the headline bug: it raised IVO (bit 11), which NDIX arms. */
static void s_add2_overflow(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0x0060, ND500_DTYPE_WORD, 1, 2);   /* W1 ADD2 I1,I2 */
    nd500_write_integer_register(cpu, 1, 0x7FFFFFFFu);
    nd500_write_integer_register(cpu, 2, 1u);
}

/* The same instruction with a result that fits must raise nothing at all.
 * Without this the suite could be satisfied by an instruction that traps
 * unconditionally. */
static void s_add2_ok(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0x0060, ND500_DTYPE_WORD, 1, 2);
    nd500_write_integer_register(cpu, 1, 100u);
    nd500_write_integer_register(cpu, 2, 23u);
}

/* Byte ADD2 overflows at a different boundary, and the narrow types are
 * exactly where PCC's generated code lives. */
static void s_add2_byte_overflow(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFC3C, ND500_DTYPE_BYTE, 1, 2);   /* BY1 ADD2 */
    nd500_write_integer_register(cpu, 1, 127u);
    nd500_write_integer_register(cpu, 2, 1u);
}

/* ---- DIV2, divide by zero -> DZ (bit 12) -------------------------------- */
static void s_div2_by_zero(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0x00A8, ND500_DTYPE_WORD, 1, 2);   /* W1 DIV2 */
    nd500_write_integer_register(cpu, 1, 100u);
    nd500_write_integer_register(cpu, 2, 0u);
}

/* ---- BYCONV, narrowing overflow -> O (bit 9) ----------------------------
 * The one that had its trap DELETED rather than corrected. 300 does not fit a
 * signed byte; the truncated result is still written (manual 15.2), and then
 * the trap is raised. */
static void s_byconv_overflow(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFD54, ND500_DTYPE_WORD, 1, 2);   /* W BYCONV I1 -> I2 */
    nd500_write_integer_register(cpu, 1, 300u);
    nd500_write_integer_register(cpu, 2, 0u);
}

static void s_byconv_ok(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFD54, ND500_DTYPE_WORD, 1, 2);
    nd500_write_integer_register(cpu, 1, 100u);
    nd500_write_integer_register(cpu, 2, 0u);
}

/* ---- HCONV, float -> halfword overflow -> O (bit 9) ---------------------
 * Was IOV (bit 16), which NDIX arms. 70000 does not fit a signed halfword. */
static void s_hconv_overflow(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFD5A, ND500_DTYPE_HALFWORD, 1, 2);  /* F HCONV A1 -> I2 */
    nd500_write_float_register(cpu, 1, nd500_float_from_ieee754(70000.0f));
    nd500_write_integer_register(cpu, 2, 0u);
}

/* ---- GETBI, bit number out of range -> IOV (bit 16) ---------------------
 * The manual names the bit-field instructions as its OWN example of IOV
 * (6.5.3.1: "operand values exceeding the legal range, e.g. in the bit field
 * and call subroutine instructions"), so this family is the one place IOV is
 * unambiguously right. It was raising IVO. Bit 32 of a word is out of range;
 * the destination must be left alone. */
static void s_getbi_out_of_range(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFDD0, ND500_DTYPE_WORD, 1, 2);   /* W3 GETBI */
    fi->target_register = 3;
    nd500_write_integer_register(cpu, 1, 0xFFFFFFFFu);
    nd500_write_integer_register(cpu, 2, 32u);     /* bit 32: one past the end */
    nd500_write_integer_register(cpu, 3, 0xDEADu); /* must survive untouched */
}

static void s_getbi_ok(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFDD0, ND500_DTYPE_WORD, 1, 2);
    fi->target_register = 3;
    nd500_write_integer_register(cpu, 1, 0x00000008u);  /* bit 3 set */
    nd500_write_integer_register(cpu, 2, 3u);
    nd500_write_integer_register(cpu, 3, 0xDEADu);
}

/* ======================================================================== */
/* KNOWN GAPS - the manual asks for a trap the code does not raise           */
/*                                                                          */
/* Deliberately not fixed on 2026-08-07: adding a trap NDIX has ARMED        */
/* changes behaviour on a working guest and deserves its own testing rather  */
/* than being smuggled into a correctness sweep. These pin TODAY'S           */
/* behaviour so a silent change is still caught, and report the gap.         */
/* ======================================================================== */

/* ---- D WCONV: silently clamps ------------------------------------------
 * Wconv.c:0xFD60 clamps an out-of-range double to INT32_MAX/INT32_MIN and
 * raises nothing - it does not even set the O flag, which every sibling
 * conversion does. Manual 15.2 says conversion to a shorter type "may cause
 * integer overflow". */
static void s_wconv_double_clamp(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFD60, ND500_DTYPE_WORD, 1, 2);   /* D WCONV D1 -> I2 */
    nd500_write_double_register(cpu, 1, nd500_double_from_ieee754(1.0e18));
    nd500_write_integer_register(cpu, 2, 0u);
}

/* ---- AXI: 0 to a negative power ----------------------------------------
 * Axi.c detects the condition correctly (invalid_op) and then records it in
 * ND500_FLAG_K - bit 8, "destination full" - which is not an error flag at
 * all, and raises no trap.
 *
 * Note this CORRECTS the trap-test plan, which listed AXI as missing IOV
 * (bit 16). The file's own header, quoting ND-500 Reference Manual ch.12.1,
 * lists the trap conditions as "Floating overflow (FO), Floating underflow
 * (FU), Invalid operation (IVO)" and the data status bits as "invalid
 * operation -> IVO". So the right bit is IVO (11), not IOV (16) - and AXI is
 * missing three traps, not one: FO and FU are only flagged too. */
static void s_axi_zero_negative_power(Nd500Cpu *cpu, Nd500FetchedInstruction *fi) {
    two_reg(fi, 0xFCC0, ND500_DTYPE_FLOAT, 1, 2);  /* F1 AXI A1, I2 */
    fi->target_register = 1;
    nd500_write_float_register(cpu, 1, nd500_float_from_ieee754(0.0f));
    nd500_write_integer_register(cpu, 2, (uint32_t)-1);
}

/* ======================================================================== */

static const TrapCase CASES[] = {
  { "ADD2 W: 0x7FFFFFFF + 1 overflows -> O", s_add2_overflow, nd500_instr_Add2,
    ND500_FLAG_O | ND500_FLAG_S, ND500_FLAG_O | ND500_FLAG_S | ND500_FLAG_Z
                                 | TRAP_IVO | TRAP_IOV | TRAP_DZ,
    TRAP_O, 1, 0x80000000u,
    "ND-05.009.4 ch.11; O is bit 9, IVO bit 11 - the ADD2 bug", 0, NULL },

  { "ADD2 W: 100 + 23 raises nothing", s_add2_ok, nd500_instr_Add2,
    0, ND500_FLAG_O | ND500_FLAG_Z | TRAP_IVO | TRAP_IOV | TRAP_DZ,
    0, 1, 123u,
    "a result in range must report no condition at all", 0, NULL },

  { "ADD2 BY: 127 + 1 overflows -> O", s_add2_byte_overflow, nd500_instr_Add2,
    ND500_FLAG_O, ND500_FLAG_O | TRAP_IVO | TRAP_IOV,
    TRAP_O, 1, 0x80u,
    "ND-05.009.4 ch.11; the narrow types are where PCC's code lives", 0, NULL },

  { "DIV2 W: divide by zero -> DZ", s_div2_by_zero, nd500_instr_Div2,
    ND500_FLAG_DZ, ND500_FLAG_DZ | ND500_FLAG_O | TRAP_IVO | TRAP_IOV,
    TRAP_DZ, 0, 0,
    "ND-05.009.4 ch.11 - DZ is bit 12", 0, NULL },

  { "BYCONV W->BY: 300 out of byte range -> O", s_byconv_overflow, nd500_instr_Byconv,
    ND500_FLAG_O, ND500_FLAG_O | TRAP_IVO | TRAP_IOV,
    TRAP_O, 2, 44u,   /* 300 & 0xFF = 0x2C; the truncated result IS written */
    "ND-05.009.4 15.2 - truncation 'may cause integer overflow'", 0, NULL },

  { "BYCONV W->BY: 100 fits, raises nothing", s_byconv_ok, nd500_instr_Byconv,
    0, ND500_FLAG_O | TRAP_IVO | TRAP_IOV,
    0, 2, 100u,
    "the ordinary char narrowing PCC emits must stay silent", 0, NULL },

  { "HCONV F->H: 70000 out of halfword range -> O", s_hconv_overflow, nd500_instr_Hconv,
    TRAP_O, TRAP_O | TRAP_IVO | TRAP_IOV,
    TRAP_O, 0, 0,
    "ND-05.009.4 15.2; was IOV (bit 16), which NDIX arms", 0, NULL },

  { "GETBI W: bit 32 out of range -> IOV", s_getbi_out_of_range, nd500_instr_Getbi,
    TRAP_IOV, TRAP_IOV | TRAP_IVO | ND500_FLAG_O,
    TRAP_IOV, 3, 0xDEADu,   /* destination must be left untouched */
    "ND-05.009.4 6.5.3.1 names bit-field instructions as THE IOV example", 0, NULL },

  { "GETBI W: bit 3 in range, raises nothing", s_getbi_ok, nd500_instr_Getbi,
    0, TRAP_IOV | TRAP_IVO | ND500_FLAG_O,
    0, 3, 1u,
    "a legal bit number must report no condition", 0, NULL },

  { "D WCONV: 1e18 out of word range -> O", s_wconv_double_clamp, nd500_instr_Wconv,
    ND500_FLAG_O, TRAP_O | TRAP_IVO | TRAP_IOV,
    TRAP_O, 2, 0x7FFFFFFFu,
    "ND-05.009.4 15.2; RetroCore's Wconv.cs raises it too", 0, NULL },

  { "AXI F: 0 ** -1 -> IVO", s_axi_zero_negative_power, nd500_instr_Axi,
    TRAP_IVO, ND500_FLAG_K | TRAP_IVO | TRAP_IOV | TRAP_FO | TRAP_FU,
    TRAP_IVO, 0, 0,
    "ND-500 Ref. ch.12.1 'invalid operation -> IVO'; RetroCore's Axi.cs agrees",
    0, NULL },
};

#define NCASES ((int)(sizeof CASES / sizeof CASES[0]))

/* ------------------------------------------------------------------------ */

/* Put the CPU in a known state: no pending trap, a clear status word, and a
 * trap-enable mask chosen by the caller. */
static void arm(Nd500Cpu *cpu, Nd500Machine *m, uint64_t ote) {
    nd500_trap_clear();
    cpu->ST1  = 0;
    cpu->ST2  = 0;
    cpu->OTE1 = (uint32_t)(ote & 0xFFFFFFFFu);
    cpu->OTE2 = (uint32_t)(ote >> 32);
    cpu->MTE1 = 0;
    cpu->MTE2 = 0;
    cpu->DITBASE = 0;
    cpu->instr_aborted = 0;
    cpu->cur_instr_pc = TEST_PC;
    m->run_flag = 1;
}

static uint64_t status_of(const Nd500Cpu *cpu) {
    return (uint64_t)cpu->ST1 | ((uint64_t)cpu->ST2 << 32);
}

static void report(int ok, const TrapCase *c, const char *what) {
    if (ok) { printf("  PASS: %s [%s]\n", c->name, what); passed++; }
    else    { printf("  FAIL: %s [%s]\n        authority: %s\n",
                     c->name, what, c->authority); failed++; }
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;
    int i;

    printf("=== Trap conformance: which trap does each instruction raise? ===\n");

    memset(&m, 0, sizeof m);
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* ---- Pass 1: nothing armed. The status delta IS the raised set. ----- */
    printf("\n--- pass 1: exact condition bits (OTE = 0, nothing dispatches) ---\n");
    for (i = 0; i < NCASES; i++) {
        const TrapCase *c = &CASES[i];
        Nd500FetchedInstruction fi;
        uint64_t got, want;

        arm(&cpu, &m, 0);
        c->setup(&cpu, &fi);
        /* setup writes registers, which can touch flags - clear again so the
         * status word measured below belongs to the instruction alone. */
        cpu.ST1 = 0;
        cpu.ST2 = 0;
        c->exec(&cpu, &fi);

        got  = status_of(&cpu) & c->care_mask;
        want = c->expect_status & c->care_mask;
        report(got == want, c, "condition bits");
        if (got != want)
            printf("        expected 0x%016llX, got 0x%016llX (mask 0x%016llX)\n",
                   (unsigned long long)want, (unsigned long long)got,
                   (unsigned long long)c->care_mask);

        if (c->result_reg) {
            uint32_t r = nd500_read_integer_register(&cpu, c->result_reg);
            report(r == c->expect_result, c, "result");
            if (r != c->expect_result)
                printf("        I%u expected 0x%08X, got 0x%08X\n",
                       c->result_reg, c->expect_result, r);
        }
    }

    /* ---- Pass 2: NDIX's real mask. Would the guest actually see it? -----
     * This is the number that mattered: every bug in the audit was a
     * condition the guest ignores being reported on a bit the guest acts on.
     * THA is 0, so a dispatch has no handler to reach - nd500_trap_occurred()
     * is still the honest record of whether one was attempted. */
    printf("\n--- pass 2: guest visibility under NDIX T_CMTE1 = 0x%08llX ---\n",
           (unsigned long long)NDIX_T_CMTE1);
    for (i = 0; i < NCASES; i++) {
        const TrapCase *c = &CASES[i];
        Nd500FetchedInstruction fi;
        int want_visible, got_visible;

        arm(&cpu, &m, NDIX_T_CMTE1);
        c->setup(&cpu, &fi);
        cpu.ST1 = 0;
        cpu.ST2 = 0;
        c->exec(&cpu, &fi);

        want_visible = (c->trap_bit & NDIX_T_CMTE1) ? 1 : 0;
        got_visible  = nd500_trap_occurred() ? 1 : 0;
        report(got_visible == want_visible, c,
               want_visible ? "NDIX sees it" : "NDIX ignores it");
        if (got_visible != want_visible)
            printf("        expected visible=%d, got %d\n", want_visible, got_visible);
    }

    /* ---- The documented gaps ------------------------------------------- */
    printf("\n--- known gaps: the manual asks for a trap that is not raised ---\n");
    for (i = 0; i < NCASES; i++) {
        const TrapCase *c = &CASES[i];
        if (!c->known_gap) continue;
        gaps++;
        printf("  GAP:  %s\n        %s\n        authority: %s\n",
               c->name, c->gap_note, c->authority);
    }
    if (!gaps) printf("  (none)\n");

    printf("\n%d passed, %d failed, %d known gaps\n", passed, failed, gaps);
    nd500_machine_free(&m);
    return failed == 0 ? 0 : 1;
}
