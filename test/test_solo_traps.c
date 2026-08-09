/*
 * SOLO / TUTTI and the two process-switch traps, DT (bit 30) and DE (bit 31).
 *
 * Before this was implemented, SOLO only printed a debug line: it never set
 * PSD (ST1 bit 4). That single omission looked like TWO unimplemented traps,
 * because the conditions DT and DE detect could not arise in a machine where
 * the process switch was never actually disabled. NDIX enables both traps
 * (kernel/MASTER/machine/trap.h: T_CMTE1 = 0xF413D800 and T_KOTE1 = 0xD601D800
 * each have bits 30 and 31 set) and vectors them (machine/locore.c:689-690), so
 * it was asking for a signal the emulator could not give.
 *
 * What the manual (ND-05.009.4, ch.6.5.4 and ch.16.1-16.2) requires, and what
 * each case below pins:
 *
 *   - "The process switch disable bit is only modifiable by the SOLO and TUTTI
 *     instructions."
 *   - DT: PSD held for more than 256 cycles. "In the ND-5000 implementation
 *     they are macroinstruction cycles", so the unit is executed instructions.
 *   - "In privilege mode there is no limitation to the duration of a SOLO
 *     operation." A privileged SOLO must NEVER time out - NDIX's kernel sits in
 *     SOLO across context switches for far longer than 256 instructions, and
 *     would trap on every one of them if this were got wrong.
 *   - DE: "non-ignorable traps (such as page fault) that require process
 *     switching must not occur. If they do occur, they cause a disable process
 *     switch error trap condition."
 *   - "Ignorable trap conditions are ignored in SOLO-TUTTI sequences regardless
 *     of enabling of these traps."
 *
 * The DT case drives check_solo_timeout() directly rather than executing 257
 * real instructions: the timeout is defined in cycles, so advancing the counter
 * is the honest way to express it, and it keeps the test about the trap instead
 * of about whichever instruction was chosen to burn cycles.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

extern void nd500_instr_Solo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Tutti(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE (4 * 1024 * 1024)

/* SOLO and TUTTI take no operands, so a zeroed fetched-instruction with an
 * address is all either one needs. */
static Nd500FetchedInstruction no_operands(uint32_t at) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof fi);
    fi.address = at;
    fi.operand_count = 0;
    return fi;
}

static void set_privileged(Nd500Cpu* cpu, int on) {
    if (on) cpu->ST1 |=  (uint32_t)(1U << ND500_ST_BIT_PIA);
    else    cpu->ST1 &= ~(uint32_t)(1U << ND500_ST_BIT_PIA);
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;
    Nd500FetchedInstruction fi = no_operands(0x2000u);

    printf("=== SOLO/TUTTI and the process-switch traps (DT, DE) ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* ---- SOLO sets PSD, TUTTI clears it ---------------------------------- */
    set_privileged(&cpu, 1);          /* neither is privileged; see Tutti.c */
    cpu.ST1 &= ~(uint32_t)ND500_FLAG_PSD;

    nd500_instr_Solo(&cpu, &fi);
    CHECK((cpu.ST1 & ND500_FLAG_PSD) != 0, "SOLO sets PSD");

    nd500_instr_Tutti(&cpu, &fi);
    CHECK((cpu.ST1 & ND500_FLAG_PSD) == 0, "TUTTI clears PSD");

    /* ---- A PRIVILEGED SOLO never times out -------------------------------
     * The manual is explicit, and NDIX depends on it completely. */
    set_privileged(&cpu, 1);
    cpu.ST2 = 0;
    nd500_instr_Solo(&cpu, &fi);
    cpu.instruction_count += 100000;          /* far past 256 */
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) == 0,
          "a privileged SOLO does not time out, however long it runs");
    CHECK((cpu.ST1 & ND500_FLAG_PSD) != 0,
          "and it is still in the SOLO region afterwards");
    nd500_instr_Tutti(&cpu, &fi);

    /* ---- An UNPRIVILEGED SOLO under the limit does not time out ---------- */
    set_privileged(&cpu, 1);
    nd500_instr_Solo(&cpu, &fi);
    set_privileged(&cpu, 0);                  /* the region runs unprivileged */
    cpu.instruction_count += 256;             /* exactly at the limit */
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) == 0,
          "256 cycles is within the limit - no timeout");

    /* ---- ...but past it, DT fires ---------------------------------------- */
    cpu.instruction_count += 1;               /* 257: one over */
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) != 0,
          "past 256 cycles an unprivileged SOLO raises DT");
    CHECK((cpu.ST1 & ND500_FLAG_PSD) == 0,
          "DT ends the SOLO region, so the handler does not re-time-out");

    /* ---- DE: a non-ignorable trap taken inside a SOLO region -------------
     * PGF stands in for "non-ignorable"; the manual names page fault as the
     * example. raise_trap is called directly so the test does not depend on
     * arranging a real MMU miss, which would be testing the MMU. */
    cpu.ST1 &= ~(uint32_t)(TRAP_DT | TRAP_DE);
    cpu.ST2 = 0;
    set_privileged(&cpu, 1);
    nd500_instr_Solo(&cpu, &fi);
    CHECK((cpu.ST1 & ND500_FLAG_PSD) != 0, "in a SOLO region before the fault");

    raise_trap(&cpu, TRAP_PGF, 0x2000u, 0x1234u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DE) != 0,
          "a page fault inside SOLO raises DE");

    /* ---- ...and the same fault OUTSIDE a SOLO region does not ------------ */
    nd500_instr_Tutti(&cpu, &fi);
    cpu.ST1 &= ~(uint32_t)TRAP_DE;
    cpu.ST2 = 0;
    raise_trap(&cpu, TRAP_PGF, 0x2000u, 0x1234u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DE) == 0,
          "the same page fault outside SOLO does not raise DE");

    /* ---- Ignorable traps are suppressed inside SOLO-TUTTI -----------------
     * "Ignorable trap conditions are ignored in SOLO-TUTTI sequences
     * regardless of enabling of these traps." The status bit stays set - the
     * instruction that produced it still recorded it - it simply must not
     * dispatch, because calling a handler IS the process switch SOLO exists to
     * prevent. THA is left at 0, so a dispatch attempt would be visible as a
     * halted machine. */
    cpu.ST1 &= ~(uint32_t)(TRAP_DT | TRAP_DE);
    cpu.ST2 = 0;
    cpu.THA = 0;
    m.run_flag = 1;
    nd500_trap_clear();
    set_privileged(&cpu, 1);
    nd500_instr_Solo(&cpu, &fi);

    cpu.ST1 |= (uint32_t)TRAP_DZ;             /* an ignorable trap, pending */
    cpu.OTE1 = (uint32_t)TRAP_DZ;             /* and enabled */
    check_pending_traps(&cpu, 0x2000u);
    CHECK(m.run_flag == 1 && !nd500_trap_occurred(),
          "an enabled ignorable trap does not dispatch inside SOLO");
    CHECK((cpu.ST1 & (uint32_t)TRAP_DZ) != 0,
          "its status bit is still recorded, ready for after TUTTI");

    nd500_instr_Tutti(&cpu, &fi);

    /* ---- A REPEATED SOLO must not restart the timer ----------------------
     * Manual ch.16.1: "Disable process switch timeout occurs if unprivileged
     * users attempt to repeat SOLO's."
     *
     * That sentence describes a consequence, not a separate check, and the
     * ND-5000 control store is what settles it: SOLO_0 (004524) arms the
     * region by OR-ing a modus-register bit (004531/004532) and TUTTI clears
     * that bit unconditionally (004536). An OR is a level set - re-arming an
     * armed bit changes nothing - and there is NO counter reset anywhere in
     * the SOLO path. So the timer runs from the FIRST SOLO of the region.
     *
     * Until 2026-08-09 Solo.c re-stamped solo_start_icount on every SOLO, so
     * unprivileged code could hold the process switch disabled forever by
     * issuing SOLO every 200 instructions. Both halves below matter: the
     * region must not trap early, and it must still trap at the real deadline.
     *
     * This also pins the opposite error. The C# side raises DT immediately on
     * a nested SOLO (Instructions/CONTROL/Solo.cs:53-58), which would trap two
     * cycles into a region the manual allows to run for 256 - the first CHECK
     * here is exactly that negative case. */
    cpu.ST1 &= ~(uint32_t)(TRAP_DT | TRAP_DE);
    cpu.ST2 = 0;
    set_privileged(&cpu, 1);
    nd500_instr_Solo(&cpu, &fi);              /* region opens here */
    set_privileged(&cpu, 0);                  /* and runs unprivileged */

    cpu.instruction_count += 200;             /* 200 elapsed - still legal */
    nd500_instr_Solo(&cpu, &fi);              /* the repeat */
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) == 0,
          "a repeated SOLO does not trap on the spot at 200 cycles");
    CHECK((cpu.ST1 & ND500_FLAG_PSD) != 0,
          "and the region is still open after the repeat");

    cpu.instruction_count += 100;             /* 300 total, past 256 */
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) != 0,
          "the repeat did not restart the timer - DT still fires at the "
          "original deadline");

    /* ---- TUTTI disarms: the next long stretch must not trap --------------
     * Microcode 004536 clears the modus bit unconditionally. After TUTTI there
     * is no region, so no amount of elapsed time may raise DT. */
    cpu.ST1 &= ~(uint32_t)(TRAP_DT | TRAP_DE);
    cpu.ST2 = 0;
    set_privileged(&cpu, 1);
    nd500_instr_Solo(&cpu, &fi);
    set_privileged(&cpu, 0);
    cpu.instruction_count += 100;
    nd500_instr_Tutti(&cpu, &fi);
    cpu.instruction_count += 100000;
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) == 0,
          "after TUTTI a long stretch does not raise DT");

    /* ---- Neither SOLO nor TUTTI is privileged ----------------------------
     * The manual marks privileged instructions with an explicit "Privileged
     * instruction" line in the Description (15.17 CLINIT, 16.13 DMON); 16.1
     * and 16.2 have no such line. Ch.16.1 also presumes unprivileged users run
     * SOLO - "Unprivileged users are not allowed to run in SOLO for more than
     * 256 cycles" - which would be dead text if SOLO were privileged. And the
     * ND-5000 control store has no privilege test in TUTTI_0 (004534-004537).
     *
     * nd500x guarded TUTTI with nd500_require_privilege() until 2026-08-09
     * while leaving SOLO unguarded, so unprivileged code could open a region
     * and then be refused the only instruction that closes it. */
    cpu.ST1 &= ~(uint32_t)(TRAP_DT | TRAP_DE | TRAP_IIC);
    cpu.ST2 = 0;
    nd500_trap_clear();
    set_privileged(&cpu, 0);                  /* unprivileged throughout */

    nd500_instr_Solo(&cpu, &fi);
    CHECK((cpu.ST1 & ND500_FLAG_PSD) != 0 && (cpu.ST1 & (uint32_t)TRAP_IIC) == 0,
          "an unprivileged SOLO opens the region and does not raise IIC");

    nd500_instr_Tutti(&cpu, &fi);
    CHECK((cpu.ST1 & ND500_FLAG_PSD) == 0 && (cpu.ST1 & (uint32_t)TRAP_IIC) == 0,
          "an unprivileged TUTTI closes it and does not raise IIC");

    /* ---- A CPU coming out of reset is not mid-SOLO -----------------------
     * Regression guard for 712ba00: nd500_cpu_reset() left instruction_count
     * and solo_start_icount alone, so a carried-over count against a zero
     * marker looked exactly like a region that had already overrun. It cost a
     * real debugging session in the validation harness, which resets the CPU
     * between cases but shares one process. */
    nd500_cpu_reset(&cpu);
    cpu.instruction_count += 300;             /* past 256, but no SOLO ran */
    check_solo_timeout(&cpu, 0x2000u);
    CHECK((cpu.ST1 & (uint32_t)TRAP_DT) == 0,
          "no DT after a reset when no SOLO ever ran");

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
