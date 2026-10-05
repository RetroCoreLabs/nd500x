/*
 * A non-ignorable trap reaches the program's own handler only when it is
 * locally enabled. Otherwise it is reported to the host.
 *
 * Ported from RetroCore CpuND500.Trap.cs RaiseTrap, "THE LOCAL-TRAP-ENABLE
 * GATE", which reads the rule out of the control store (011034-011037 and
 * 011064): TE = OTE | MTE, bits 31 and 30 forced on, bits 8..0 forced off,
 * ANDed with the pending trap bit. Zero -> the trap is reported to the ND-100
 * and the handler vector is never read.
 *
 * Before the gate, nd500x dispatched on "THA != 0 and the slot is non-zero"
 * alone. MEASURED 05-OCT-2026 on the octobus lane (PLACE-DOMAIN CPU-STAT, RUN):
 * SINTRAN had written OTE = MTE = 0 for the domain process, the program then
 * installed a handler in THA[38], and its next page fault was dispatched to
 * that handler instead of being reported. The handler's ENTT faulted on its own
 * frame and the same park repeated 501 times.
 *
 * The gate applies only when a trap sink is attached. A free-running nd500x
 * (no ND-100 beside it) must keep its existing dispatch exactly, and the first
 * check below pins that.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (4 * 1024 * 1024)
#define START_PC      0x2000u
#define THA_BASE      0x6000u
#define HANDLER_AT    0x7000u
#define FAULT_ADDR    0x12345u
#define ENTT_OPCODE   0xBCu
#define PGF_NUMBER    38u
#define PGF_OTE2_BIT  (1u << (PGF_NUMBER - 32u))

static int sink_calls = 0;
static uint16_t sink_last_trap = 0xFFFFu;

/* Accepts the trap, so raise_trap returns without stopping the machine. */
static int counting_sink(void *ctx, uint16_t trap_number,
                         uint32_t trapping_pc, uint32_t trap_address)
{
    (void)ctx;
    (void)trapping_pc;
    (void)trap_address;
    sink_calls++;
    sink_last_trap = trap_number;
    return 1;
}

/* Put the CPU back to "about to raise a trap at START_PC, nothing pending". */
static void reset_case(Nd500Machine *m, Nd500Cpu *cpu)
{
    nd500_trap_clear();
    nd500_set_in_trap_handler(cpu, false);
    cpu->trap_dispatch_pending = 0;
    cpu->instr_aborted = 0;
    cpu->PC = START_PC;
    cpu->THA = THA_BASE;
    cpu->OTE1 = 0;
    cpu->OTE2 = 0;
    cpu->MTE1 = 0;
    cpu->MTE2 = 0;
    m->run_flag = 1;
    m->stop_reason = STOP_NONE;
    sink_calls = 0;
    sink_last_trap = 0xFFFFu;
}

int main(void)
{
    Nd500Machine m;
    Nd500Cpu cpu;

    printf("=== local trap enable gate (page fault, trap 38) ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* A handler for trap 38 whose first instruction is ENTT. */
    nd500_bus_write32(&m, THA_BASE + (PGF_NUMBER * 4u), HANDLER_AT);
    nd500_bus_write8(&m, HANDLER_AT, ENTT_OPCODE);

    /* --- Control: NO sink. The gate must not apply, and the trap dispatches
     * to the installed handler with OTE = MTE = 0, exactly as before. This is
     * also the check that the fixture can dispatch at all: without it, the
     * "not dispatched" result below would pass on a broken fixture too. --- */
    (void)nd500_cpu_set_trap_sink(&cpu, NULL, NULL);
    reset_case(&m, &cpu);
    raise_trap(&cpu, TRAP_PGF, START_PC, FAULT_ADDR);
    printf("  no sink, OTE=0: PC=0x%X pending=%u\n", cpu.PC, cpu.trap_dispatch_pending);
    CHECK(cpu.PC == HANDLER_AT, "no sink: dispatched to the local handler, as before");
    CHECK(cpu.trap_dispatch_pending == 1u, "no sink: the dispatch is awaiting its ENTT");

    /* --- The gate: sink attached, OTE = MTE = 0. Not locally enabled, so the
     * trap is reported and the handler vector is not used. --- */
    (void)nd500_cpu_set_trap_sink(&cpu, counting_sink, NULL);
    reset_case(&m, &cpu);
    raise_trap(&cpu, TRAP_PGF, START_PC, FAULT_ADDR);
    printf("  sink, OTE=0: PC=0x%X pending=%u sink_calls=%d trap=%u\n",
           cpu.PC, cpu.trap_dispatch_pending, sink_calls, (unsigned)sink_last_trap);
    CHECK(sink_calls == 1, "sink, OTE=MTE=0: the trap is reported to the sink once");
    CHECK(sink_last_trap == PGF_NUMBER, "sink, OTE=MTE=0: reported as trap 38 (46B)");
    CHECK(cpu.PC == START_PC, "sink, OTE=MTE=0: P does not move to the handler");
    CHECK(cpu.trap_dispatch_pending == 0u, "sink, OTE=MTE=0: no dispatch is pending");

    /* --- Own-enabled: OTE2 bit 6 is trap 38. Now it dispatches locally. --- */
    reset_case(&m, &cpu);
    cpu.OTE2 = PGF_OTE2_BIT;
    raise_trap(&cpu, TRAP_PGF, START_PC, FAULT_ADDR);
    printf("  sink, OTE2 bit 6: PC=0x%X sink_calls=%d\n", cpu.PC, sink_calls);
    CHECK(cpu.PC == HANDLER_AT, "sink, OTE enables 38: dispatched to the local handler");
    CHECK(sink_calls == 0, "sink, OTE enables 38: nothing is reported to the sink");

    /* --- Mother-enabled: the reference ORs MTE into the same mask. --- */
    reset_case(&m, &cpu);
    cpu.MTE2 = PGF_OTE2_BIT;
    raise_trap(&cpu, TRAP_PGF, START_PC, FAULT_ADDR);
    printf("  sink, MTE2 bit 6: PC=0x%X sink_calls=%d\n", cpu.PC, sink_calls);
    CHECK(cpu.PC == HANDLER_AT, "sink, MTE enables 38: dispatched to the local handler");
    CHECK(sink_calls == 0, "sink, MTE enables 38: nothing is reported to the sink");

    /* --- A DIFFERENT enabled bit must not open the gate for trap 38. --- */
    reset_case(&m, &cpu);
    cpu.OTE1 = 0xFFFFFFFFu;                      /* every bit below 32 */
    cpu.OTE2 = (uint32_t)~PGF_OTE2_BIT & 0x3FFu;  /* 32..41 except 38 */
    raise_trap(&cpu, TRAP_PGF, START_PC, FAULT_ADDR);
    printf("  sink, every bit but 38: PC=0x%X sink_calls=%d\n", cpu.PC, sink_calls);
    CHECK(sink_calls == 1, "sink, every enable except 38: still reported to the sink");
    CHECK(cpu.PC == START_PC, "sink, every enable except 38: P does not move");

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
