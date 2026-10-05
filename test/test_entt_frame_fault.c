/*
 * A page fault on ENTT's own trap frame is reported as what it is: a DATA
 * WRITE in the frame's own physical segment.
 *
 * ENTT checks its frame pages with a non-faulting peek before it changes any
 * register. The peek records nothing, and raise_trap copies mmu_pgf_psn and
 * mmu_pgf_is_write - which are never cleared - into the trap record. Raising
 * the page fault directly after the peek therefore reported the PREVIOUS real
 * fault's segment and access class.
 *
 * MEASURED 05-OCT-2026 on the octobus lane (PLACE-DOMAIN CPU-STAT, RUN): the
 * program's stack-overflow handler ENTT at 0x0800415A faulted on its frame at
 * 0x08001800 and was reported to SINTRAN as "psn=11 mms=0x8000000F" - segment
 * 11 and READ, both left over from the instruction-side fault before it.
 * SINTRAN was asked for the wrong page, so the same park repeated 156 times.
 *
 * RetroCore's Entt.cs writes the frame with ordinary WriteMemory calls, so its
 * fault always comes out of the MMU walk. ENTT here now takes the fault through
 * nd500_mmu_translate as a data write, and this test pins the record it leaves.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"

extern void nd500_instr_Entt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE    (4 * 1024 * 1024)
#define PSTP_AT        0x30000u
#define DITBASE_AT     0x38000u
#define DATA_SEGMENT   4           /* logical segment holding THA and the frame */
#define FRAME_PSN      9u          /* its physical segment */
#define FRAME_PFN      0x50u       /* the ONE resident page, PS_AZI direct */
#define STALE_PSN      11u         /* what the previous fault left behind */
#define ENTT_AT        0x00007000u
#define TRAPPING_PC    0x00006000u
#define B_BEFORE       0x000001A4u

#define SEG_BASE       ((uint32_t)DATA_SEGMENT << SGSHIFT)
/* Frame base is THA + 256 and the frame is 220 bytes. The page is 0x800 bytes. */
#define THA_INSIDE     (SEG_BASE | 0x400u)   /* frame 0x500..0x5DC: all in page 0 */
#define THA_STRADDLE   (SEG_BASE | 0x680u)   /* frame 0x780..0x85C: crosses into page 1 */

static void set_const_word(Nd500OperandDecoded* op, uint32_t value)
{
    memset(op, 0, sizeof(*op));
    op->mode = ND500_ADDR_CONSTANT;
    op->data_len = 4;
    op->data[0] = (uint8_t)(value >> 24u);
    op->data[1] = (uint8_t)(value >> 16u);
    op->data[2] = (uint8_t)(value >> 8u);
    op->data[3] = (uint8_t)(value);
}

static Nd500FetchedInstruction make_entt(void)
{
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = ENTT_AT;
    fi.opcode = 0x00BC;               /* ENTT */
    fi.operand_count = 2;
    fi.data_type = ND500_DTYPE_WORD;
    set_const_word(&fi.operands[0], 0x20u);    /* local data size */
    set_const_word(&fi.operands[1], 0x100u);   /* stack demand */
    return fi;
}

/* "A trap was just dispatched and its handler's ENTT is the next instruction",
 * with the fault fields poisoned the way the previous real fault leaves them. */
static void arm_entt(Nd500Machine* m, Nd500Cpu* cpu, uint32_t tha)
{
    nd500_trap_clear();
    m->run_flag = 1;
    m->stop_reason = STOP_NONE;
    cpu->instr_aborted = 0;
    cpu->THA = tha;
    cpu->B = B_BEFORE;
    cpu->PC = ENTT_AT;
    cpu->trap_saved_PC = TRAPPING_PC;
    cpu->trap_number = 27;
    cpu->trap_dispatch_pending = 1;
    nd500_set_in_trap_handler(cpu, true);
    cpu->mmu_pgf_psn = STALE_PSN;
    cpu->mmu_pgf_is_write = 0;
    cpu->mmu_pgf_where = 0u;
    cpu->trap_saved_psn = 0u;
    cpu->trap_saved_is_write = 0;
}

int main(void)
{
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== ENTT frame page fault names its own segment and access ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* Guest tables: one writable data capability for DATA_SEGMENT in domain 0,
     * pointing at FRAME_PSN, whose PST entry is a single direct page. Anything
     * past that page does not translate. */
    cpu.PSTP = PSTP_AT;
    cpu.DITBASE = DITBASE_AT;
    cpu.dit_configured = 1;
    cpu.CED = 0;
    cpu.CAD = 0;

    uint32_t cap = DC_WRP | FRAME_PSN;
    uint32_t cap_addr = DITBASE_AT + 64u + ((uint32_t)DATA_SEGMENT * 2u);
    nd500_bus_write8(&m, cap_addr,      (uint8_t)((cap >> 8u) & 0xFFu));
    nd500_bus_write8(&m, cap_addr + 1u, (uint8_t)(cap & 0xFFu));

    uint32_t pste = ((uint32_t)PS_AZI << 30u) | FRAME_PFN;
    for (uint32_t i = 0; i < 4u; i++) {
        nd500_bus_write8(&m, PSTP_AT + (FRAME_PSN * 4u) + i,
                         (uint8_t)(pste >> (24u - (i * 8u))));
    }

    /* Allocate the CPU's own table storage first, the way test_mmu_translation
     * does with its first nd500_mmu_set_pst_entry call. Without it the walk
     * stops at "Tables not initialized" before it ever reaches the guest
     * tables above, and returns the address untranslated without a fault. */
    nd500_mmu_set_pst_entry(&cpu, 100, PS_AZI, 0x1234);

    nd500_mmu_enable(&cpu);
    m.mmu_enabled = 1;

    Nd500FetchedInstruction entt = make_entt();

    /* --- Control: the whole frame is resident. ENTT must complete, or the
     * "it faulted correctly" checks below would pass on a fixture in which
     * ENTT can never run at all. --- */
    arm_entt(&m, &cpu, THA_INSIDE);
    nd500_instr_Entt(&cpu, &entt);
    printf("  frame resident: trap=%d B=0x%08X (want 0x%08X)\n",
           nd500_trap_occurred(), cpu.B, THA_INSIDE + 256u);
    CHECK(!nd500_trap_occurred(), "resident frame: ENTT raises nothing");
    CHECK(cpu.B == THA_INSIDE + 256u, "resident frame: B is the trap frame base");

    /* --- The fault: the frame crosses into a page that does not translate. --- */
    arm_entt(&m, &cpu, THA_STRADDLE);
    nd500_instr_Entt(&cpu, &entt);
    printf("  frame straddles: trap=%d psn=%u is_write=%d fault_addr=0x%08X PC=0x%08X B=0x%08X\n",
           nd500_trap_occurred(), (unsigned)cpu.trap_saved_psn, cpu.trap_saved_is_write,
           cpu.trap_saved_fault_addr, cpu.PC, cpu.B);
    CHECK(nd500_trap_occurred(), "straddling frame: ENTT raises a fault");
    CHECK(cpu.trap_saved_psn == FRAME_PSN,
          "the record names the FRAME's physical segment, not the previous fault's");
    CHECK(cpu.trap_saved_is_write == 1, "the record says WRITE, not the previous fault's read");
    CHECK(cpu.trap_saved_fault_addr == (SEG_BASE | 0x800u),
          "the fault address is the first frame word in the absent page");
    CHECK(cpu.B == B_BEFORE, "B is untouched, so the retry sees the pre-trap frame");
    CHECK(cpu.PC == TRAPPING_PC, "P names the original trapping instruction");

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
