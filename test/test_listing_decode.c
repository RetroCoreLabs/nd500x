/**
 * test_listing_decode.c - a listing decodes without touching the machine
 *
 * The decoder works out every operand's effective address, and for
 * IND(b.x) that means reading a pointer from DATA memory. Execution does that
 * through nd500_mmu_translate, which may fault - correctly, the fault is the
 * machine's. A listing (disassembler, debugger, DAP) used the same path. When
 * the pointer could not be read a trap was raised; the trap state is sticky and
 * mmu_read8 returns 0 while it is set, so every later byte of the listing came
 * out as 00. Measured 2026-11-03 on BM-OPERATOR-B02:DOM: after
 * `1A CF 64 70 64 76 C5 40` (w move #...,IND(b.64)) at 0x100760EA the listing
 * printed `00 ???` from 0x100760F2 on, where the file holds `FD 3C 85 ...`.
 *
 * nd500_decode_at_peek reads through the trap-free nd500_mmu_peek_space.
 * These cases check that it raises no trap, decodes what follows, reads a
 * mapped IND pointer correctly, and fails cleanly on unmapped code.
 */
#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_tlb.h"
#include "../src/disasm/nd500_disasm.h"

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", (name)); tests_passed++; } \
    else      { printf("  FAIL: %s\n", (name)); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (4u * 1024u * 1024u)
#define PSTP_AT       0x30000u
#define DITBASE_AT    0x38000u

/* Logical segments. CODE and DATA are mapped; NODATA and NOCODE have no
 * capability at all. */
#define SEG_CODE      2
#define PSN_CODE      20u
#define PFN_CODE      0x50u
#define SEG_DATA      4
#define PSN_DATA      21u
#define PFN_DATA      0x58u
#define SEG_NODATA    3
#define SEG_NOCODE    5

#define CODE_OFFSET   0x100u
#define POINTER       0x12345678u

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

static uint32_t seg_addr(int segment, uint32_t offset)
{
    return ((uint32_t)segment << SGSHIFT) | offset;
}

static void clear_traps(Nd500Cpu *cpu)
{
    nd500_trap_clear();
    nd500_mmu_tlb_flush();
    cpu->instr_aborted = 0;
}

/* The BM-OPERATOR bytes: w move #'dpdv',IND(b.64) then w1 laddr r.20 */
static const uint8_t code_bytes[] = {
    0x1A, 0xCF, 0x64, 0x70, 0x64, 0x76, 0xC5, 0x40,
    0xFD, 0x3C, 0x85
};

/* ---- 1. the IND pointer is not mapped ----------------------------------- */
static void test_unmapped_pointer(Nd500Machine *m, Nd500Cpu *cpu)
{
    printf("Test: IND(b.64) with b in a segment that has no data capability\n");
    uint32_t pc = seg_addr(SEG_CODE, CODE_OFFSET);
    clear_traps(cpu);
    cpu->B = seg_addr(SEG_NODATA, 0u);

    Nd500FetchedInstruction fi;
    int rc = nd500_decode_at_peek(m, pc, &fi);
    CHECK(rc == 0 && fi.total_len == 8u, "listing decode: w move, 8 bytes");
    CHECK(!nd500_trap_occurred() && !cpu->instr_aborted, "listing decode: no trap, nothing aborted");
    CHECK(fi.operands[1].effective_address == 0u, "listing decode: unreadable pointer gives no address");

    rc = nd500_decode_at_peek(m, pc + 8u, &fi);
    CHECK(rc == 0 && fi.opcode == 0xFD3Cu && fi.total_len == 3u,
          "listing decode: the next instruction is FD 3C 85, not 00");
    CHECK(fi.bytes[0] == 0xFDu && fi.bytes[1] == 0x3Cu && fi.bytes[2] == 0x85u,
          "listing decode: its bytes are the ones in memory");

    char out[1024];
    size_t n = nd500_disasm_format_range(m, pc, (uint32_t)sizeof(code_bytes), out, sizeof(out));
    out[n < sizeof(out) ? n : sizeof(out) - 1u] = '\0';
    printf("%s", out);
    CHECK(strstr(out, "FD 3C 85") != NULL && strstr(out, "???") == NULL,
          "disassembly range: both instructions, no 00 ???");
    CHECK(!nd500_trap_occurred() && !cpu->instr_aborted, "disassembly range: no trap left behind");
    CHECK(nd500_mmu_peek_space(cpu, seg_addr(SEG_NODATA, 64u), 0u, 0) == 0xFFFFFFFFu,
          "listing: no mapping was created for the pointer's segment");

    /* The execution decode acts on the machine at this read: here it
     * demand-maps the data segment (in the BM-OPERATOR run it raised "No data
     * capability"). Either is right for execution and wrong for a listing,
     * which is why a listing must not take this path. */
    (void)nd500_decode_at(m, pc, &fi);
    CHECK(nd500_trap_occurred() || cpu->instr_aborted ||
          nd500_mmu_peek_space(cpu, seg_addr(SEG_NODATA, 64u), 0u, 0) != 0xFFFFFFFFu,
          "execution decode: the same read faults or maps - it changes the machine");
    clear_traps(cpu);
}

/* ---- 2. the IND pointer is mapped --------------------------------------- */
static void test_mapped_pointer(Nd500Machine *m, Nd500Cpu *cpu)
{
    printf("Test: IND(b.64) with b in a mapped data segment\n");
    clear_traps(cpu);
    cpu->B = seg_addr(SEG_DATA, 0u);
    put_be32(m, (PFN_DATA << PGSHIFT) + 64u, POINTER);

    Nd500FetchedInstruction fi;
    int rc = nd500_decode_at_peek(m, seg_addr(SEG_CODE, CODE_OFFSET), &fi);
    CHECK(rc == 0, "listing decode succeeds");
    CHECK(fi.operands[1].effective_address == POINTER, "listing decode: the pointer read through the data table");

    clear_traps(cpu);
    rc = nd500_decode_at(m, seg_addr(SEG_CODE, CODE_OFFSET), &fi);
    CHECK(rc == 0 && fi.operands[1].effective_address == POINTER,
          "execution decode: the same address, so both paths read the same table");
    CHECK(!nd500_trap_occurred(), "execution decode: no fault when the pointer is mapped");
}

/* ---- 3. the code itself is not mapped ----------------------------------- */
static void test_unmapped_code(Nd500Machine *m, Nd500Cpu *cpu)
{
    printf("Test: listing an address whose segment has no program capability\n");
    clear_traps(cpu);
    Nd500FetchedInstruction fi;
    int rc = nd500_decode_at_peek(m, seg_addr(SEG_NOCODE, 0u), &fi);
    CHECK(rc != 0, "listing decode reports failure");
    CHECK(!nd500_trap_occurred() && !cpu->instr_aborted, "listing decode: no trap");
}

int main(void)
{
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== listing decode ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);
    m.cpu = &cpu;

    /* Guest tables, as in test_fault_record.c */
    cpu.PSTP = PSTP_AT;
    cpu.DITBASE = DITBASE_AT;
    cpu.dit_configured = 1;
    cpu.CED = 0;
    cpu.CAD = 0;

    put_be16(&m, DITBASE_AT + ((uint32_t)SEG_CODE * 2u), PC_DIR | PSN_CODE);
    put_be32(&m, PSTP_AT + (PSN_CODE * 4u), ((uint32_t)PS_AZI << 30u) | PFN_CODE);
    put_be16(&m, DITBASE_AT + 64u + ((uint32_t)SEG_DATA * 2u), DC_WRP | PSN_DATA);
    put_be32(&m, PSTP_AT + (PSN_DATA * 4u), ((uint32_t)PS_AZI << 30u) | PFN_DATA);

    for (uint32_t i = 0; i < (uint32_t)sizeof(code_bytes); i++)
    {
        nd500_bus_write8(&m, (PFN_CODE << PGSHIFT) + CODE_OFFSET + i, code_bytes[i]);
    }

    /* The CPU's own table storage; without it the walk stops before the guest
     * tables (see test_fault_record.c). */
    nd500_mmu_set_pst_entry(&cpu, 100, PS_AZI, 0x1234);
    nd500_mmu_enable(&cpu);
    m.mmu_enabled = 1;

    test_unmapped_pointer(&m, &cpu);
    test_mapped_pointer(&m, &cpu);
    test_unmapped_code(&m, &cpu);

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
