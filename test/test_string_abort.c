/*
 * test_string_abort.c - a string instruction that page-faults commits nothing
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

/*
 * SMOVE, SFILL, SMATCH and SCOTR each run a loop over string elements and then
 * commit I1/I2 and the status flags. A page fault on an element access aborts
 * the instruction; it is restarted after page-in. The restart only works if
 * the first attempt left I1, I2 and the flags as they were.
 *
 * Reference: $RETROCORE/Emulated.HW/ND/CPU/ND500/Instructions/STRING/Smove.cs
 * lines 105-110 return before the commit, and
 * $RETROCORE/Emulated.HW/ND/CPU/ND500/CpuND500.UncaughtFaults.cs
 * (UnwindOnAbortedAccess) unwinds every faulted access to the instruction
 * boundary, which gives SFILL, SMATCH and SCOTR the same behaviour.
 *
 * Measured failure this pins: LED-CONV, by smove at 0x080308D7, 20 bytes, the
 * source page not present. The first attempt committed I1 = I2 = 20, the
 * restart copied nothing, and the file name "DDBTABLES-E" was never built.
 *
 * Each case runs the instruction twice: once with the page missing (nothing
 * may change), once after the page is mapped (the full result must appear).
 * The second half is the control - a guard that simply refused to execute
 * would pass the first half and fail the second.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_tlb.h"

extern void nd500_instr_Smove(Nd500Cpu *cpu, const Nd500FetchedInstruction *fi);
extern void nd500_instr_Sfill(Nd500Cpu *cpu, const Nd500FetchedInstruction *fi);
extern void nd500_instr_Smatch(Nd500Cpu *cpu, const Nd500FetchedInstruction *fi);
extern void nd500_instr_Scotr(Nd500Cpu *cpu, const Nd500FetchedInstruction *fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", (name)); tests_passed++; } \
    else      { printf("  FAIL: %s\n", (name)); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (4u * 1024u * 1024u)
#define PSTP_AT       0x30000u
#define DITBASE_AT    0x38000u
#define INSTR_PC      0x00002000u

/* One mapped segment for descriptors, destinations and tables, and one
 * segment per case whose PST entry starts as zero (page fault). */
#define SEG_MAPPED    4
#define PSN_MAPPED    30u
#define PFN_MAPPED    0x98u
#define SEG_SMOVE     5
#define PSN_SMOVE     20u
#define PFN_SMOVE     0xA0u
#define SEG_SFILL     6
#define PSN_SFILL     21u
#define PFN_SFILL     0xA1u
#define SEG_SMATCH    7
#define PSN_SMATCH    22u
#define PFN_SMATCH    0xA2u
#define SEG_SCOTR     8
#define PSN_SCOTR     23u
#define PFN_SCOTR     0xA3u

#define TRAPN_PGF     38u    /* 46B */

/* Offsets inside the mapped segment's single page. */
#define OFF_DESC_A    0x010u
#define OFF_DESC_B    0x020u
#define OFF_DEST      0x100u
#define OFF_SUBSTR    0x180u
#define OFF_SOURCE1   0x1C0u
#define OFF_TABLE     0x400u   /* 256-byte translation table */

#define OFF_IN_FAULT  0x042u   /* where the data sits in the faulting segment */
#define MARKER        0xEEu

static int sink_calls = 0;
static uint16_t sink_last_trap = 0xFFFFu;

static int recording_sink(void *ctx, uint16_t trap_number,
                          uint32_t trapping_pc, uint32_t trap_address)
{
    (void)ctx;
    (void)trapping_pc;
    (void)trap_address;
    sink_calls++;
    sink_last_trap = trap_number;
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

static void set_pst(Nd500Machine *m, uint32_t psn, uint32_t mode, uint32_t pfn)
{
    put_be32(m, PSTP_AT + (psn * 4u), (mode << 30u) | pfn);
}

static uint32_t seg_addr(int segment, uint32_t offset)
{
    return ((uint32_t)segment << SGSHIFT) | offset;
}

static uint32_t phys(uint32_t pfn, uint32_t offset)
{
    return (pfn << PGSHIFT) + offset;
}

static void put_bytes(Nd500Machine *m, uint32_t addr, const char *text, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        nd500_bus_write8(m, addr + i, (uint8_t)text[i]);
    }
}

static void fill_bytes(Nd500Machine *m, uint32_t addr, uint8_t value, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        nd500_bus_write8(m, addr + i, value);
    }
}

static bool bytes_equal(Nd500Machine *m, uint32_t addr, const char *text, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (nd500_bus_read8(m, addr + i) != (uint8_t)text[i])
        {
            return false;
        }
    }
    return true;
}

static bool bytes_all(Nd500Machine *m, uint32_t addr, uint8_t value, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (nd500_bus_read8(m, addr + i) != value)
        {
            return false;
        }
    }
    return true;
}

/* A string descriptor: element count, then the address of element 0. */
static void put_descriptor(Nd500Machine *m, uint32_t offset, uint32_t count, uint32_t address)
{
    put_be32(m, phys(PFN_MAPPED, offset), count);
    put_be32(m, phys(PFN_MAPPED, offset) + 4u, address);
}

/* "A new instruction at INSTR_PC, nothing pending." */
static void new_instruction(Nd500Machine *m, Nd500Cpu *cpu)
{
    nd500_trap_clear();
    nd500_mmu_tlb_flush();
    nd500_set_in_trap_handler(cpu, false);
    cpu->trap_dispatch_pending = 0;
    cpu->instr_aborted = 0;
    cpu->PC = INSTR_PC;
    cpu->cur_instr_pc = INSTR_PC;
    cpu->THA = 0;
    m->run_flag = 1;
    m->stop_reason = STOP_NONE;
    sink_calls = 0;
    sink_last_trap = 0xFFFFu;
}

static void make_fi(Nd500FetchedInstruction *fi, uint8_t operand_count)
{
    memset(fi, 0, sizeof(*fi));
    fi->address = INSTR_PC;
    fi->operand_count = operand_count;
    fi->data_type = ND500_DTYPE_BYTE;
    fi->target_register = 1;
}

static bool flag_set(const Nd500Cpu *cpu, uint32_t flag)
{
    return (cpu->ST1 & flag) != 0u;
}

/* ---- SMOVE --------------------------------------------------------------- */
static void test_smove(Nd500Machine *m, Nd500Cpu *cpu)
{
    static const char NAME[] = "DDBTABLES-E     :VTM";
    const uint32_t count = 20u;
    Nd500FetchedInstruction fi;

    printf("SMOVE: source page not present\n");
    put_descriptor(m, OFF_DESC_A, count, seg_addr(SEG_SMOVE, OFF_IN_FAULT));
    put_descriptor(m, OFF_DESC_B, count, seg_addr(SEG_MAPPED, OFF_DEST));
    fill_bytes(m, phys(PFN_MAPPED, OFF_DEST), MARKER, count);
    make_fi(&fi, 2);
    fi.operands[0].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_A);
    fi.operands[1].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_B);

    new_instruction(m, cpu);
    cpu->I[0] = 0;
    cpu->I[1] = 0;
    nd500_set_flag(cpu, ND500_FLAG_K);
    nd500_set_flag(cpu, ND500_FLAG_Z);
    uint32_t st1_before = cpu->ST1;
    nd500_instr_Smove(cpu, &fi);
    printf("  faulted: sink_calls=%d trap=%u I1=%u I2=%u ST1=0x%08X (before 0x%08X)\n",
           sink_calls, (unsigned)sink_last_trap, (unsigned)cpu->I[0], (unsigned)cpu->I[1],
           (unsigned)cpu->ST1, (unsigned)st1_before);
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PGF, "the fault is reported once, trap 46B");
    CHECK(cpu->I[0] == 0u && cpu->I[1] == 0u, "I1 and I2 are left at 0");
    CHECK(cpu->ST1 == st1_before, "the status flags are left alone");
    CHECK(bytes_all(m, phys(PFN_MAPPED, OFF_DEST), MARKER, count), "the destination is untouched");

    /* Page-in, then the restart. */
    put_bytes(m, phys(PFN_SMOVE, OFF_IN_FAULT), NAME, count);
    set_pst(m, PSN_SMOVE, PS_AZI, PFN_SMOVE);
    new_instruction(m, cpu);
    nd500_instr_Smove(cpu, &fi);
    printf("  restart: sink_calls=%d I1=%u I2=%u K=%d Z=%d\n", sink_calls,
           (unsigned)cpu->I[0], (unsigned)cpu->I[1],
           (int)flag_set(cpu, ND500_FLAG_K), (int)flag_set(cpu, ND500_FLAG_Z));
    CHECK(sink_calls == 0, "the restart does not fault");
    CHECK(bytes_equal(m, phys(PFN_MAPPED, OFF_DEST), NAME, count),
          "the restart copies all 20 bytes");
    CHECK(cpu->I[0] == count && cpu->I[1] == count, "I1 and I2 end at 20");
    CHECK(!flag_set(cpu, ND500_FLAG_K) && !flag_set(cpu, ND500_FLAG_Z),
          "source exhausted: K=0, Z=0");
}

/* ---- SFILL --------------------------------------------------------------- */
static void test_sfill(Nd500Machine *m, Nd500Cpu *cpu)
{
    const uint32_t count = 12u;
    const uint8_t fill = 0x41u;
    Nd500FetchedInstruction fi;

    printf("SFILL: destination page not present\n");
    put_descriptor(m, OFF_DESC_A, count, seg_addr(SEG_SFILL, OFF_IN_FAULT));
    make_fi(&fi, 1);
    fi.operands[0].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_A);

    new_instruction(m, cpu);
    cpu->I[0] = fill;      /* by sfill with register 1: the fill value */
    cpu->I[1] = 0;
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_set_flag(cpu, ND500_FLAG_Z);
    uint32_t st1_before = cpu->ST1;
    nd500_instr_Sfill(cpu, &fi);
    printf("  faulted: sink_calls=%d trap=%u I2=%u ST1=0x%08X (before 0x%08X)\n", sink_calls,
           (unsigned)sink_last_trap, (unsigned)cpu->I[1], (unsigned)cpu->ST1,
           (unsigned)st1_before);
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PGF, "the fault is reported once, trap 46B");
    CHECK(cpu->I[1] == 0u, "I2 is left at 0");
    CHECK(cpu->ST1 == st1_before, "the status flags are left alone");

    fill_bytes(m, phys(PFN_SFILL, OFF_IN_FAULT), MARKER, count);
    set_pst(m, PSN_SFILL, PS_AZI, PFN_SFILL);
    new_instruction(m, cpu);
    nd500_instr_Sfill(cpu, &fi);
    printf("  restart: sink_calls=%d I2=%u K=%d\n", sink_calls, (unsigned)cpu->I[1],
           (int)flag_set(cpu, ND500_FLAG_K));
    CHECK(sink_calls == 0, "the restart does not fault");
    CHECK(bytes_all(m, phys(PFN_SFILL, OFF_IN_FAULT), fill, count),
          "the restart fills all 12 bytes");
    CHECK(cpu->I[1] == count, "I2 ends at 12");
    CHECK(flag_set(cpu, ND500_FLAG_K), "destination full: K=1");
}

/* ---- SMATCH -------------------------------------------------------------- */
static void test_smatch(Nd500Machine *m, Nd500Cpu *cpu)
{
    static const char TEXT[] = "xxABxxxx";
    const uint32_t count = 8u;
    Nd500FetchedInstruction fi;

    printf("SMATCH: searched string's page not present\n");
    put_bytes(m, phys(PFN_MAPPED, OFF_SUBSTR), "AB", 2u);
    put_descriptor(m, OFF_DESC_A, 2u, seg_addr(SEG_MAPPED, OFF_SUBSTR));
    put_descriptor(m, OFF_DESC_B, count, seg_addr(SEG_SMATCH, OFF_IN_FAULT));
    make_fi(&fi, 2);
    fi.operands[0].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_A);
    fi.operands[1].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_B);

    new_instruction(m, cpu);
    cpu->I[0] = 0;
    cpu->I[1] = 0;
    nd500_set_flag(cpu, ND500_FLAG_K);
    nd500_set_flag(cpu, ND500_FLAG_Z);
    uint32_t st1_before = cpu->ST1;
    nd500_instr_Smatch(cpu, &fi);
    printf("  faulted: sink_calls=%d trap=%u I2=%u ST1=0x%08X (before 0x%08X)\n", sink_calls,
           (unsigned)sink_last_trap, (unsigned)cpu->I[1], (unsigned)cpu->ST1,
           (unsigned)st1_before);
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PGF, "the fault is reported once, trap 46B");
    CHECK(cpu->I[1] == 0u, "I2 is left at 0, not moved to the end of the string");
    CHECK(cpu->ST1 == st1_before, "the status flags are left alone");

    put_bytes(m, phys(PFN_SMATCH, OFF_IN_FAULT), TEXT, count);
    set_pst(m, PSN_SMATCH, PS_AZI, PFN_SMATCH);
    new_instruction(m, cpu);
    nd500_instr_Smatch(cpu, &fi);
    printf("  restart: sink_calls=%d I2=%u Z=%d\n", sink_calls, (unsigned)cpu->I[1],
           (int)flag_set(cpu, ND500_FLAG_Z));
    CHECK(sink_calls == 0, "the restart does not fault");
    CHECK(cpu->I[1] == 2u && flag_set(cpu, ND500_FLAG_Z), "the restart finds AB at index 2, Z=1");
}

/* ---- SCOTR --------------------------------------------------------------- */
static void test_scotr(Nd500Machine *m, Nd500Cpu *cpu)
{
    const uint32_t count = 3u;
    Nd500FetchedInstruction fi;

    printf("SCOTR: second string's page not present\n");
    for (uint32_t i = 0; i < 256u; i++)
    {
        nd500_bus_write8(m, phys(PFN_MAPPED, OFF_TABLE) + i, (uint8_t)i);   /* identity */
    }
    put_bytes(m, phys(PFN_MAPPED, OFF_SOURCE1), "ABC", count);
    put_descriptor(m, OFF_DESC_A, count, seg_addr(SEG_MAPPED, OFF_SOURCE1));
    put_descriptor(m, OFF_DESC_B, count, seg_addr(SEG_SCOTR, OFF_IN_FAULT));
    make_fi(&fi, 3);
    fi.operands[0].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_A);
    fi.operands[1].effective_address = seg_addr(SEG_MAPPED, OFF_DESC_B);
    fi.operands[2].effective_address = seg_addr(SEG_MAPPED, OFF_TABLE);

    new_instruction(m, cpu);
    cpu->I[0] = 0;
    cpu->I[1] = 0;
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_set_flag(cpu, ND500_FLAG_Z);
    uint32_t st1_before = cpu->ST1;
    nd500_instr_Scotr(cpu, &fi);
    printf("  faulted: sink_calls=%d trap=%u I1=%u I2=%u ST1=0x%08X (before 0x%08X)\n",
           sink_calls, (unsigned)sink_last_trap, (unsigned)cpu->I[0], (unsigned)cpu->I[1],
           (unsigned)cpu->ST1, (unsigned)st1_before);
    CHECK(sink_calls == 1 && sink_last_trap == TRAPN_PGF, "the fault is reported once, trap 46B");
    CHECK(cpu->I[0] == 0u && cpu->I[1] == 0u, "I1 and I2 are left at 0");
    CHECK(cpu->ST1 == st1_before, "the status flags are left alone (no false 'different')");

    put_bytes(m, phys(PFN_SCOTR, OFF_IN_FAULT), "ABC", count);
    set_pst(m, PSN_SCOTR, PS_AZI, PFN_SCOTR);
    new_instruction(m, cpu);
    nd500_instr_Scotr(cpu, &fi);
    printf("  restart: sink_calls=%d I1=%u I2=%u K=%d Z=%d\n", sink_calls,
           (unsigned)cpu->I[0], (unsigned)cpu->I[1],
           (int)flag_set(cpu, ND500_FLAG_K), (int)flag_set(cpu, ND500_FLAG_Z));
    CHECK(sink_calls == 0, "the restart does not fault");
    CHECK(cpu->I[0] == count && cpu->I[1] == count, "I1 and I2 end at 3");
    CHECK(!flag_set(cpu, ND500_FLAG_K) && flag_set(cpu, ND500_FLAG_Z),
          "equal strings: K=0, Z=1");
}

int main(void)
{
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== string instruction abort ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);
    m.cpu = &cpu;

    cpu.PSTP = PSTP_AT;
    cpu.DITBASE = DITBASE_AT;
    cpu.dit_configured = 1;
    cpu.CED = 0;
    cpu.CAD = 0;

    set_data_cap(&m, SEG_MAPPED, DC_WRP | PSN_MAPPED);
    set_pst(&m, PSN_MAPPED, PS_AZI, PFN_MAPPED);

    set_data_cap(&m, SEG_SMOVE, DC_WRP | PSN_SMOVE);
    set_data_cap(&m, SEG_SFILL, DC_WRP | PSN_SFILL);
    set_data_cap(&m, SEG_SMATCH, DC_WRP | PSN_SMATCH);
    set_data_cap(&m, SEG_SCOTR, DC_WRP | PSN_SCOTR);
    set_pst(&m, PSN_SMOVE, PS_AZI, 0u);     /* zero entry: page fault */
    set_pst(&m, PSN_SFILL, PS_AZI, 0u);
    set_pst(&m, PSN_SMATCH, PS_AZI, 0u);
    set_pst(&m, PSN_SCOTR, PS_AZI, 0u);

    /* Allocate the CPU's own table storage, the way the other MMU tests do. */
    nd500_mmu_set_pst_entry(&cpu, 100, PS_AZI, 0x1234);

    nd500_mmu_enable(&cpu);
    m.mmu_enabled = 1;

    (void)nd500_cpu_set_trap_sink(&cpu, recording_sink, NULL);

    test_smove(&m, &cpu);
    test_sfill(&m, &cpu);
    test_smatch(&m, &cpu);
    test_scotr(&m, &cpu);

    (void)nd500_cpu_set_trap_sink(&cpu, NULL, NULL);
    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
