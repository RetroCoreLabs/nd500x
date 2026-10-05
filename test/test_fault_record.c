/*
 * test_fault_record.c - what a fault leaves in the record an ND-100 reads
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

/*
 * Five behaviours ported from the RetroCore reference, each pinned here with a
 * check that fails without the port. Paths are relative to the RetroCore
 * checkout ($RETROCORE).
 *
 * 1. ONE PAGE FAULT PER INSTRUCTION (trap sink attached only).
 *    $RETROCORE/Emulated.HW/ND/CPU/ND500/CpuND500.MMU.cs:1617-1650. Once an
 *    instruction is aborted, a later page fault of the same instruction is
 *    recorded by the walk but not raised. Protect violations are not guarded.
 *
 * 2. THE PROTECT-VIOLATION RECORD (every lane). CpuND500.MMU.cs:921, 949, 967,
 *    1235, 1336. Each protect-violation site writes the physical segment and
 *    the read/write direction beside the fault code, so the trap record does
 *    not inherit them from an earlier walk.
 *
 * 3. ZERO CAPABILITY (trap sink attached only). CpuND500.MMU.cs:916-927. A
 *    zero capability is a protect violation even when the address is smaller
 *    than the memory size.
 *
 * 4. THE [STOP] LINE (embedded only). The line nd500_stop_on_pending_trap
 *    prints goes to stderr when nd500_embedded is set, because stdout is then
 *    the host guest's console.
 *
 * 5. OPCODE ZERO IS TRAP 41B (trap sink attached only).
 *    $RETROCORE/Emulated.HW/ND/CPU/ND500/CpuND500.Execute.cs:796-806.
 *
 * Every "trap sink attached only" case has a control WITHOUT a sink that pins
 * the free-running behaviour: a free-running nd500x must not change.
 *
 * NOT TESTED: the decode-failure branch of nd500_cpu_step (the port of
 * CpuND500.Execute.cs:669-676). nd500_decode_at returns non-zero only for a
 * NULL machine or output pointer, which nd500_cpu_step cannot pass, so there
 * is no input that reaches that branch.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <io.h>        /* dup(), dup2(), close() live here on Windows */
#else
#  include <unistd.h>    /* dup(), dup2(), close() */
#endif

#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_tlb.h"

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", (name)); tests_passed++; } \
    else      { printf("  FAIL: %s\n", (name)); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (4u * 1024u * 1024u)
#define PSTP_AT       0x30000u
#define DITBASE_AT    0x38000u
#define INSTR_PC      0x00002000u   /* the instruction "being executed" */
#define STALE_PSN     77u           /* what an earlier walk left behind */

/* Logical segments and the physical segments their capabilities name. */
#define SEG_FAULT_A   4      /* writable, PST entry zero -> page fault */
#define PSN_FAULT_A   9u
#define SEG_FAULT_B   5      /* writable, PST entry zero -> page fault */
#define PSN_FAULT_B   10u
#define SEG_READONLY  6      /* capability WITHOUT DC_WRP */
#define PSN_READONLY  11u
#define SEG_ASI       7      /* single indexing, its one page write protected */
#define PSN_ASI       12u
#define SEG_ADI       8      /* double indexing, its one page write protected */
#define PSN_ADI       13u
#define SEG_GATE      9      /* PROGRAM capability with the indirect bit */
#define SEG_NOCAP     10     /* no capability at all, address above memory */

/* Physical pages. None of them overlaps PSTP_AT (page 0x60) or DITBASE_AT
 * (page 0x70). */
#define PFN_READONLY  0x50u
#define PFN_ASI_TABLE 0x80u
#define PFN_ASI_DATA  0x90u
#define PFN_ADI_L1    0x82u
#define PFN_ADI_L2    0x84u
#define PFN_ADI_DATA  0x92u
#define PTE_READ_ONLY 0x80000000u   /* bit 31 of a last-level entry */

#define TRAPN_IIC     33u    /* 41B */
#define TRAPN_PV      36u    /* 44B */
#define TRAPN_PGF     38u    /* 46B */

static int sink_calls = 0;
static uint16_t sink_last_trap = 0xFFFFu;
static uint32_t sink_last_pc = 0xFFFFFFFFu;

/* Records the raise and returns 0, which is what the ND-100 side's own sink
 * does: the stop that follows is what gets reported. */
static int recording_sink(void *ctx, uint16_t trap_number,
                          uint32_t trapping_pc, uint32_t trap_address)
{
    (void)ctx;
    (void)trap_address;
    sink_calls++;
    sink_last_trap = trap_number;
    sink_last_pc = trapping_pc;
    return 0;
}

static void put_be16(Nd500Machine *m, uint32_t addr, uint32_t value)
{
    nd500_bus_write8(m, addr, (uint8_t)((value >> 8u) & 0xFFu));
    nd500_bus_write8(m, addr + 1u, (uint8_t)(value & 0xFFu));
}

static void put_be32(Nd500Machine *m, uint32_t addr, uint32_t value)
{
    for (uint32_t i = 0; i < 4u; i++)
    {
        nd500_bus_write8(m, addr + i, (uint8_t)((value >> (24u - (i * 8u))) & 0xFFu));
    }
}

static void set_data_cap(Nd500Machine *m, int segment, uint32_t cap)
{
    put_be16(m, DITBASE_AT + 64u + ((uint32_t)segment * 2u), cap);
}

static void set_program_cap(Nd500Machine *m, int segment, uint32_t cap)
{
    put_be16(m, DITBASE_AT + ((uint32_t)segment * 2u), cap);
}

static void set_pst(Nd500Machine *m, uint32_t psn, uint32_t mode, uint32_t pfn)
{
    put_be32(m, PSTP_AT + (psn * 4u), (mode << 30u) | pfn);
}

static uint32_t seg_addr(int segment, uint32_t offset)
{
    return ((uint32_t)segment << SGSHIFT) | offset;
}

/* "A new instruction at INSTR_PC, nothing pending", with the walk's latch
 * poisoned the way an earlier, unrelated fault leaves it. */
static void reset_case(Nd500Machine *m, Nd500Cpu *cpu, int stale_is_write)
{
    nd500_trap_clear();
    nd500_mmu_tlb_flush();
    nd500_set_in_trap_handler(cpu, false);
    cpu->trap_dispatch_pending = 0;
    cpu->instr_aborted = 0;
    cpu->PC = INSTR_PC;
    cpu->cur_instr_pc = INSTR_PC;
    cpu->THA = 0;
    cpu->OTE1 = 0;
    cpu->OTE2 = 0;
    cpu->MTE1 = 0;
    cpu->MTE2 = 0;
    cpu->mmu_pgf_where = 0u;
    cpu->mmu_pgf_psn = STALE_PSN;
    cpu->mmu_pgf_is_write = stale_is_write;
    cpu->trap_saved_psn = 0u;
    cpu->trap_saved_is_write = 0;
    cpu->trap_saved_info = 0u;
    cpu->trap_saved_fault_addr = 0u;
    m->run_flag = 1;
    m->stop_reason = STOP_NONE;
    sink_calls = 0;
    sink_last_trap = 0xFFFFu;
    sink_last_pc = 0xFFFFFFFFu;
}

/* ---- 1. one page fault per instruction ---------------------------------- */
static void test_one_fault_per_instruction(Nd500Machine *m, Nd500Cpu *cpu)
{
    const uint32_t va_a = seg_addr(SEG_FAULT_A, 0x010u);
    const uint32_t va_b = seg_addr(SEG_FAULT_B, 0x020u);
    const uint32_t va_ro = seg_addr(SEG_READONLY, 0x004u);

    printf("1. one page fault per instruction\n");

    /* Control, NO sink: both walks raise, exactly as before the port. */
    (void)nd500_cpu_set_trap_sink(cpu, NULL, NULL);
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, va_a, 0, 0);
    CHECK(cpu->trap_saved_psn == PSN_FAULT_A, "no sink: the first fault is raised");
    (void)nd500_mmu_translate(cpu, va_b, 1, 0);
    CHECK(cpu->trap_saved_psn == PSN_FAULT_B,
          "no sink: the second fault of the same instruction is raised too, as before");

    /* Sink attached: the first fault is raised and aborts the instruction. */
    (void)nd500_cpu_set_trap_sink(cpu, recording_sink, NULL);
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, va_a, 0, 0);
    printf("  first:  sink_calls=%d psn=%u addr=0x%08X aborted=%u\n", sink_calls,
           (unsigned)cpu->trap_saved_psn, (unsigned)cpu->trap_saved_fault_addr,
           (unsigned)cpu->instr_aborted);
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PGF, "sink: the first fault is trap 46B");
    CHECK(cpu->instr_aborted != 0u, "sink: the instruction is marked aborted");
    CHECK(cpu->trap_saved_psn == PSN_FAULT_A && cpu->trap_saved_fault_addr == va_a,
          "sink: the record names the first fault's segment and address");

    /* A later operand access of the SAME instruction faults on another
     * segment, as a write. It must not be raised. */
    (void)nd500_mmu_translate(cpu, va_b, 1, 0);
    printf("  second: sink_calls=%d psn=%u addr=0x%08X is_write=%d\n", sink_calls,
           (unsigned)cpu->trap_saved_psn, (unsigned)cpu->trap_saved_fault_addr,
           cpu->trap_saved_is_write);
    CHECK(sink_calls == 1, "sink: the second page fault of the instruction is NOT raised");
    CHECK(cpu->trap_saved_psn == PSN_FAULT_A, "sink: the record still names the first segment");
    CHECK(cpu->trap_saved_fault_addr == va_a, "sink: and the first fault address");
    CHECK(cpu->trap_saved_is_write == 0, "sink: and the first fault's direction (read)");
    CHECK(cpu->trap_saved_info == MMW_PFZPST, "sink: and the first fault's location code");
    /* What the reference leaves in the live latch in that branch: the walk's
     * segment and direction stand (SetMmuFault ran), the code is cleared. */
    CHECK(cpu->mmu_pgf_where == 0u, "sink: the live fault code is cleared, as the reference");
    CHECK(cpu->mmu_pgf_psn == PSN_FAULT_B && cpu->mmu_pgf_is_write == 1,
          "sink: the live segment and direction are the second walk's reading");

    /* A protect violation is deliberately NOT guarded (CpuND500.MMU.cs:1643). */
    (void)nd500_mmu_translate(cpu, va_ro, 1, 0);
    CHECK(sink_calls == 2 && sink_last_trap == TRAPN_PV,
          "sink: a protect violation of an aborted instruction IS still raised");

    /* The NEXT instruction: nd500_cpu_step clears instr_aborted at its top. */
    nd500_trap_clear();
    m->stop_reason = STOP_NONE;
    cpu->instr_aborted = 0;
    sink_calls = 0;
    (void)nd500_mmu_translate(cpu, va_b, 1, 0);
    CHECK(sink_calls == 1 && cpu->trap_saved_psn == PSN_FAULT_B,
          "sink: the next instruction's page fault is raised normally");
}

/* ---- 2. the protect-violation record ------------------------------------ */
static void test_protect_violation_record(Nd500Machine *m, Nd500Cpu *cpu)
{
    printf("2. the protect-violation record (no sink - applies on every lane)\n");
    (void)nd500_cpu_set_trap_sink(cpu, NULL, NULL);

    /* Write to a segment whose capability lacks DC_WRP. */
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, seg_addr(SEG_READONLY, 0x004u), 1, 0);
    CHECK(nd500_trap_occurred() && cpu->trap_saved_info == MMW_PVWVIOL,
          "write without DC_WRP: protect violation, code PVWVIOL");
    CHECK(cpu->trap_saved_psn == PSN_READONLY, "write without DC_WRP: the segment is its own");
    CHECK(cpu->trap_saved_is_write == 1, "write without DC_WRP: the direction is WRITE");

    /* Single indexing. The read proves the fixture maps the page at all. */
    reset_case(m, cpu, 0);
    uint32_t pa = nd500_mmu_translate(cpu, seg_addr(SEG_ASI, 0x004u), 0, 0);
    CHECK(!nd500_trap_occurred() && pa == ((PFN_ASI_DATA << PGSHIFT) | 0x004u),
          "PS_ASI fixture: a read of the page translates");
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, seg_addr(SEG_ASI, 0x004u), 1, 0);
    CHECK(nd500_trap_occurred() && cpu->trap_saved_info == MMW_PVWVIOL,
          "PS_ASI read-only page: protect violation, code PVWVIOL");
    CHECK(cpu->trap_saved_psn == PSN_ASI, "PS_ASI read-only page: the segment is its own");
    CHECK(cpu->trap_saved_is_write == 1, "PS_ASI read-only page: the direction is WRITE");

    /* Double indexing. */
    reset_case(m, cpu, 0);
    pa = nd500_mmu_translate(cpu, seg_addr(SEG_ADI, 0x004u), 0, 0);
    CHECK(!nd500_trap_occurred() && pa == ((PFN_ADI_DATA << PGSHIFT) | 0x004u),
          "PS_ADI fixture: a read of the page translates");
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, seg_addr(SEG_ADI, 0x004u), 1, 0);
    CHECK(nd500_trap_occurred() && cpu->trap_saved_info == MMW_PVWVIOL,
          "PS_ADI read-only page: protect violation, code PVWVIOL");
    CHECK(cpu->trap_saved_psn == PSN_ADI, "PS_ADI read-only page: the segment is its own");
    CHECK(cpu->trap_saved_is_write == 1, "PS_ADI read-only page: the direction is WRITE");

    /* A fetch through an indirect (gate) program capability. The latch is
     * poisoned with WRITE and a segment; a fetch is a read and a gate names
     * no physical segment. */
    reset_case(m, cpu, 1);
    (void)nd500_mmu_translate(cpu, seg_addr(SEG_GATE, 0x004u), 0, 1);
    CHECK(nd500_trap_occurred() && cpu->trap_saved_info == (MMW_IND_SAME | MMW_INST),
          "indirect program capability: protect violation, code IND_SAME + MMINST");
    CHECK(cpu->trap_saved_psn == 0u, "indirect program capability: no physical segment");
    CHECK(cpu->trap_saved_is_write == 0, "indirect program capability: the direction is READ");

    /* A fetch with no capability at all, above the memory size so that the
     * free-running identity fallback does not apply. */
    reset_case(m, cpu, 1);
    (void)nd500_mmu_translate(cpu, seg_addr(SEG_NOCAP, 0x004u), 0, 1);
    CHECK(nd500_trap_occurred() && cpu->trap_saved_info == (MMW_ZEROCAP | MMW_INST),
          "zero capability: protect violation, code ZEROCAP + MMINST");
    CHECK(cpu->trap_saved_psn == 0u, "zero capability: no physical segment");
    CHECK(cpu->trap_saved_is_write == 0, "zero capability: the direction is READ, not stale");
}

/* ---- 3. zero capability below the memory size --------------------------- */
static void test_zero_capability_in_memory(Nd500Machine *m, Nd500Cpu *cpu)
{
    /* Segment 0, so the demand-segment allocator (segments 1..30) is not
     * involved, and non-zero, so it is not the Address Zero case. */
    const uint32_t va = 0x00001234u;

    printf("3. zero capability, address below the memory size\n");

    /* Control, NO sink: the identity fallback stands, no trap. */
    (void)nd500_cpu_set_trap_sink(cpu, NULL, NULL);
    reset_case(m, cpu, 0);
    uint32_t pa = nd500_mmu_translate(cpu, va, 1, 0);
    printf("  no sink: pa=0x%08X trap=%d\n", (unsigned)pa, nd500_trap_occurred());
    CHECK(!nd500_trap_occurred(), "no sink: no trap, as before");
    CHECK(pa == va, "no sink: the address is identity mapped, as before");

    /* Sink attached: protect violation, MMS code 8. */
    (void)nd500_cpu_set_trap_sink(cpu, recording_sink, NULL);
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, va, 1, 0);
    printf("  sink: sink_calls=%d trap=%u info=0x%X is_write=%d\n", sink_calls,
           (unsigned)sink_last_trap, (unsigned)cpu->trap_saved_info, cpu->trap_saved_is_write);
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PV, "sink: reported as trap 44B");
    CHECK(cpu->trap_saved_info == MMW_ZEROCAP, "sink: fault code is ZEROCAP");
    CHECK(cpu->trap_saved_is_write == 1 && cpu->trap_saved_psn == 0u,
          "sink: the record says WRITE and names no physical segment");
    CHECK(m->stop_reason == STOP_TRAP_PROTECTION_VIOLATION, "sink: the CPU is stopped on it");

    /* The fetch side too. */
    reset_case(m, cpu, 0);
    (void)nd500_mmu_translate(cpu, va, 0, 1);
    CHECK(sink_calls == 1 && cpu->trap_saved_info == (MMW_ZEROCAP | MMW_INST),
          "sink: a fetch with a zero program capability is ZEROCAP + MMINST");
}

/* ---- 4. where the [STOP] line goes -------------------------------------- */
typedef struct
{
    bool ran;
    bool on_stdout;
    bool on_stderr;
} StopLine;

static bool file_contains(FILE *f, const char *needle)
{
    char line[512];

    rewind(f);
    while (fgets(line, (int)sizeof line, f) != NULL)
    {
        if (strstr(line, needle) != NULL)
        {
            return true;
        }
    }
    return false;
}

/* Runs one step with a trap already pending, which is the path through
 * nd500_stop_on_pending_trap, and reports which stream the line reached. */
static StopLine run_pending_trap_stop(Nd500Machine *m, Nd500Cpu *cpu)
{
    StopLine seen = { false, false, false };
    FILE *out_file = tmpfile();
    FILE *err_file = tmpfile();

    if (out_file != NULL && err_file != NULL)
    {
        (void)fflush(stdout);
        (void)fflush(stderr);
        int saved_out = dup(fileno(stdout));
        int saved_err = dup(fileno(stderr));
        if (saved_out >= 0 && saved_err >= 0
            && dup2(fileno(out_file), fileno(stdout)) >= 0
            && dup2(fileno(err_file), fileno(stderr)) >= 0)
        {
            reset_case(m, cpu, 0);
            nd500_trap_set_state(TRAP_PGF, INSTR_PC, 0x08001800u, NULL);
            (void)nd500_cpu_step(cpu);
            seen.ran = true;
        }
        (void)fflush(stdout);
        (void)fflush(stderr);
        if (saved_out >= 0)
        {
            (void)dup2(saved_out, fileno(stdout));
            (void)close(saved_out);
        }
        if (saved_err >= 0)
        {
            (void)dup2(saved_err, fileno(stderr));
            (void)close(saved_err);
        }
        seen.on_stdout = file_contains(out_file, "<- failing instruction");
        seen.on_stderr = file_contains(err_file, "<- failing instruction");
    }
    if (out_file != NULL)
    {
        (void)fclose(out_file);
    }
    if (err_file != NULL)
    {
        (void)fclose(err_file);
    }
    nd500_trap_clear();
    return seen;
}

static void test_stop_line_stream(Nd500Machine *m, Nd500Cpu *cpu)
{
    printf("4. the [STOP] line of a pending trap\n");
    (void)nd500_cpu_set_trap_sink(cpu, recording_sink, NULL);

    nd500_embedded = 0;
    StopLine free_running = run_pending_trap_stop(m, cpu);
    CHECK(free_running.ran, "free-running: the stop path ran with both streams captured");
    CHECK(free_running.on_stdout && !free_running.on_stderr,
          "free-running: the line is on stdout, as before");

    nd500_embedded = 1;
    StopLine embedded = run_pending_trap_stop(m, cpu);
    nd500_embedded = 0;
    CHECK(embedded.ran, "embedded: the stop path ran with both streams captured");
    CHECK(!embedded.on_stdout, "embedded: nothing on stdout, which is the guest's console");
    /* NOR ON STDERR. In nd100x's interactive mode stderr is the guest's terminal
     * too - measured 05-OCT-2026, the line moved to stderr was still on screen -
     * and the embedding logs every park itself. */
    CHECK(!embedded.on_stderr, "embedded: nothing on stderr either - it is the same terminal");
}

/* ---- 6. no end-of-instruction trap for a page-faulted instruction -------- */
#define SEG_CODE      11     /* program segment holding one instruction and a handler */
#define PSN_CODE      14u
#define PFN_CODE      0x54u
#define CODE_OFFSET   0x010u
#define HANDLER_OFFSET 0x100u
#define THA_OFFSET    0x200u /* in SEG_READONLY, which is mapped for reading */

/* w add3 b.0x8,$0x80,r2 - reads the word at B+8. */
static const uint8_t ADD3_BYTES[] = { 0xFC, 0x69, 0x42, 0xCE, 0x00, 0x80, 0xD1 };

static void arm_pending_prt_case(Nd500Machine *m, Nd500Cpu *cpu)
{
    reset_case(m, cpu, 0);
    cpu->PC = seg_addr(SEG_CODE, CODE_OFFSET);
    cpu->cur_instr_pc = 0;
    cpu->B = seg_addr(SEG_FAULT_A, 0x100u);          /* B+8 is not mapped */
    cpu->THA = seg_addr(SEG_READONLY, THA_OFFSET);
    cpu->ST1 = (uint32_t)TRAP_PRT;                   /* Programmed Trap pending */
    cpu->ST2 = 0;
    cpu->OTE1 = (uint32_t)TRAP_PRT;                  /* and enabled */
    cpu->OTE2 = 0;
}

static void test_no_trap_after_aborted_instruction(Nd500Machine *m, Nd500Cpu *cpu)
{
    const uint32_t handler = seg_addr(SEG_CODE, HANDLER_OFFSET);
    const uint32_t instr = seg_addr(SEG_CODE, CODE_OFFSET);

    printf("6. a page-faulted instruction takes no end-of-instruction trap\n");

    set_program_cap(m, SEG_CODE, PSN_CODE);
    set_pst(m, PSN_CODE, PS_AZI, PFN_CODE);
    for (uint32_t i = 0; i < sizeof ADD3_BYTES; i++)
    {
        nd500_bus_write8(m, (PFN_CODE << PGSHIFT) + CODE_OFFSET + i, ADD3_BYTES[i]);
    }
    nd500_bus_write8(m, (PFN_CODE << PGSHIFT) + HANDLER_OFFSET, 0xBCu);   /* ENTT */
    /* The handler vector: THA + 4 * trap number, trap 29 = Programmed Trap. */
    put_be32(m, (PFN_READONLY << PGSHIFT) + THA_OFFSET + (29u * 4u), handler);

    /* Control, NO sink: unchanged - the fault stops nothing, the pending
     * Programmed Trap is dispatched at the end of the step. */
    (void)nd500_cpu_set_trap_sink(cpu, NULL, NULL);
    arm_pending_prt_case(m, cpu);
    bool stepped = nd500_cpu_step(cpu);
    printf("  no sink: stepped=%d P=0x%08X ST1=0x%08X reason=%d\n", (int)stepped,
           (unsigned)cpu->PC, (unsigned)cpu->ST1, (int)m->stop_reason);
    CHECK(stepped && cpu->PC == handler,
          "no sink: the pending trap is dispatched after the faulted instruction, as before");

    /* Sink attached: the page fault is the outcome of the step. The reference
     * returns before CheckPendingTraps for an instruction that never completed
     * ($RETROCORE/Emulated.HW/ND/CPU/ND500/CpuND500.Execute.cs:716-722). */
    (void)nd500_cpu_set_trap_sink(cpu, recording_sink, NULL);
    arm_pending_prt_case(m, cpu);
    stepped = nd500_cpu_step(cpu);
    printf("  sink: stepped=%d sink_calls=%d trap=%u P=0x%08X ST1=0x%08X reason=%d\n",
           (int)stepped, sink_calls, (unsigned)sink_last_trap, (unsigned)cpu->PC,
           (unsigned)cpu->ST1, (int)m->stop_reason);
    CHECK(!stepped, "sink: the step stops");
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PGF, "sink: on the page fault, trap 46B");
    CHECK(cpu->PC != handler, "sink: P is NOT vectored to the Programmed Trap handler");
    CHECK((cpu->ST1 & (uint32_t)TRAP_PRT) != 0u, "sink: the Programmed Trap stays pending");

    /* When the process is made active again the pending trap is taken first,
     * with the faulted instruction as the place to come back to. This is the
     * call the host makes at activation. */
    nd500_trap_clear();
    m->stop_reason = STOP_NONE;
    cpu->instr_aborted = 0;
    cpu->PC = instr;
    check_pending_traps(cpu, cpu->PC);
    printf("  activation: P=0x%08X resume=0x%08X\n", (unsigned)cpu->PC,
           (unsigned)cpu->trap_resume_PC);
    CHECK(cpu->PC == handler, "activation: P is the Programmed Trap handler");
    CHECK(cpu->trap_resume_PC == instr, "activation: the handler returns to the faulted instruction");

    nd500_trap_clear();
    nd500_set_in_trap_handler(cpu, false);
    cpu->trap_dispatch_pending = 0;
    cpu->ST1 = 0;
    cpu->OTE1 = 0;
    cpu->THA = 0;
}

/* ---- 5. opcode zero ------------------------------------------------------ */
static void test_opcode_zero(Nd500Machine *m, Nd500Cpu *cpu)
{
    printf("5. opcode zero at P\n");

    /* MMU off: the fetch is a plain physical read and nothing else can fault. */
    nd500_mmu_disable(cpu);
    m->mmu_enabled = 0;
    nd500_bus_write8(m, INSTR_PC, 0x00u);
    (void)nd500_dbg_set_trap_invalid(1);

    /* Control, NO sink: the private invalid-instruction stop, P untouched. */
    (void)nd500_cpu_set_trap_sink(cpu, NULL, NULL);
    reset_case(m, cpu, 0);
    bool stepped = nd500_cpu_step(cpu);
    CHECK(!stepped && m->stop_reason == STOP_INVALID_INSTRUCTION_00,
          "no sink: stops as an invalid instruction 0x00, as before");
    CHECK(cpu->PC == INSTR_PC, "no sink: P does not move, as before");

    /* Sink attached: Illegal Instruction Code, trap 41B. */
    (void)nd500_cpu_set_trap_sink(cpu, recording_sink, NULL);
    reset_case(m, cpu, 0);
    stepped = nd500_cpu_step(cpu);
    printf("  sink: stepped=%d sink_calls=%d trap=%u pc=0x%08X reason=%d P=0x%08X P1=0x%08X\n",
           (int)stepped, sink_calls, (unsigned)sink_last_trap, (unsigned)sink_last_pc,
           (int)m->stop_reason, (unsigned)cpu->PC, (unsigned)cpu->P1);
    CHECK(!stepped, "sink: the step stops");
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_IIC, "sink: reported once, as trap 41B");
    CHECK(sink_last_pc == INSTR_PC, "sink: naming the instruction's own address");
    CHECK(m->stop_reason == STOP_TRAP_ILLEGAL_INSTRUCTION,
          "sink: the stop reason is an illegal instruction trap");
    CHECK(m->stop_addr == INSTR_PC && cpu->P1 == INSTR_PC, "sink: stop address and P1 name it too");
    CHECK(cpu->PC == INSTR_PC + 1u, "sink: P is past the opcode byte, as the reference leaves it");
    nd500_trap_clear();
}

int main(void)
{
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== fault record ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);
    m.cpu = &cpu;

    /* Guest tables: the walk uses them once a DIT is declared and PSTP is set. */
    cpu.PSTP = PSTP_AT;
    cpu.DITBASE = DITBASE_AT;
    cpu.dit_configured = 1;
    cpu.CED = 0;
    cpu.CAD = 0;

    set_data_cap(&m, SEG_FAULT_A, DC_WRP | PSN_FAULT_A);
    set_data_cap(&m, SEG_FAULT_B, DC_WRP | PSN_FAULT_B);
    set_pst(&m, PSN_FAULT_A, PS_AZI, 0u);            /* zero entry: no mapping */
    set_pst(&m, PSN_FAULT_B, PS_AZI, 0u);

    set_data_cap(&m, SEG_READONLY, PSN_READONLY);    /* no DC_WRP */
    set_pst(&m, PSN_READONLY, PS_AZI, PFN_READONLY);

    set_data_cap(&m, SEG_ASI, DC_WRP | PSN_ASI);
    set_pst(&m, PSN_ASI, PS_ASI, PFN_ASI_TABLE);
    put_be32(&m, PFN_ASI_TABLE << PGSHIFT, PTE_READ_ONLY | PFN_ASI_DATA);

    set_data_cap(&m, SEG_ADI, DC_WRP | PSN_ADI);
    set_pst(&m, PSN_ADI, PS_ADI, PFN_ADI_L1);
    put_be32(&m, PFN_ADI_L1 << PGSHIFT, PFN_ADI_L2);
    put_be32(&m, PFN_ADI_L2 << PGSHIFT, PTE_READ_ONLY | PFN_ADI_DATA);

    set_program_cap(&m, SEG_GATE, PC_IND | 0x0005u);

    /* Allocate the CPU's own table storage, the way the other MMU tests do.
     * Without it the walk stops at "Tables not initialized" before it reaches
     * the guest tables above and returns the address untranslated, no fault. */
    nd500_mmu_set_pst_entry(&cpu, 100, PS_AZI, 0x1234);

    nd500_mmu_enable(&cpu);
    m.mmu_enabled = 1;

    test_one_fault_per_instruction(&m, &cpu);
    test_protect_violation_record(&m, &cpu);
    test_zero_capability_in_memory(&m, &cpu);
    test_stop_line_stream(&m, &cpu);
    test_no_trap_after_aborted_instruction(&m, &cpu);
    test_opcode_zero(&m, &cpu);      /* last: it switches the MMU off */

    (void)nd500_cpu_set_trap_sink(&cpu, NULL, NULL);
    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
