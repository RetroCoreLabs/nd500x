/*
 * An instruction whose operand access faulted must commit NOTHING.
 *
 * This pins the ADD3 abort guard (commit a351296). Without it, running cc
 * inside the NDIX guest panicked the kernel every time, at any memory size:
 *
 *   [PGFDBG] page fault CED=6 CAD=0 PC=0x00003B2F faultaddr=0x0000123C
 *   [TRAP] ADD3 at PC=0x00003B26: Integer overflow
 *   pageno 000009e4
 *   panic: pagein valid page
 *
 * 0x3B26 + 9 bytes = 0x3B2F, so both lines came from ONE instruction: the ADD3
 * page-faulted on an operand and carried on regardless. It computed on
 * undefined values, wrote a garbage result, set flags, and raised "Integer
 * overflow" on top of the page fault the kernel was already about to service.
 * NDIX then entered pagein() for a page whose pte already held a frame and
 * panicked (kernel/MASTER/sys/vm_page.c:106-118).
 *
 * The instruction under test is a real one lifted from the NDIX kernel
 * (machine/locore.c, disassembled from locore.o):
 *
 *      0000050E: FC 69 42 CE 00 80 D1    w add3  b.0x8,$0x80,r2
 *
 * The abort is simulated by setting cpu->instr_aborted, which is exactly the
 * state a faulting operand access leaves behind. That keeps the test
 * deterministic - it does not depend on arranging a real page fault, which
 * needs MMU setup and would test the MMU rather than the guard.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

extern void nd500_instr_Add3(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE (4 * 1024 * 1024)
#define CODE_AT     0x2000u
#define FRAME_B     0x9000u
#define SENTINEL    0xFEEDFACEu
#define ADDEND      0x00000080u          /* the $0x80 in the encoding */
#define LHS         0x00000011u          /* value placed at b.0x8 */

/* w add3 b.0x8,$0x80,r2  -> FC 69 42 CE 00 80 D1 */
static const uint8_t ADD3_BYTES[] = { 0xFC, 0x69, 0x42, 0xCE, 0x00, 0x80, 0xD1 };

static int decode_add3(Nd500Machine* m, Nd500Cpu* cpu, Nd500FetchedInstruction* fi) {
    for (size_t i = 0; i < sizeof ADD3_BYTES; i++)
        nd500_bus_write8(m, CODE_AT + (uint32_t)i, ADD3_BYTES[i]);

    nd500_bus_write32(m, FRAME_B + 0x8, LHS);
    cpu->B = FRAME_B;

    memset(fi, 0, sizeof(*fi));
    nd500_decode_at(m, CODE_AT, fi);
    return fi->operand_count;
}

int main(void) {
    Nd500Machine m;
    Nd500Cpu cpu;
    Nd500FetchedInstruction fi;

    printf("=== instruction abort guard (ADD3) ===\n");

    memset(&m, 0, sizeof(m));
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    int ops = decode_add3(&m, &cpu, &fi);
    printf("  decoded: opcode=0x%04X operands=%d len=%u\n",
           fi.opcode, ops, fi.total_len);
    CHECK(ops == 3, "the ADD3 encoding decodes with three operands");

    /* --- Control: with no abort pending, the instruction MUST still work.
     * A guard that simply refused to execute would also make the panic go
     * away, and would be a far worse bug. --- */
    cpu.instr_aborted = 0;
    nd500_write_integer_register(&cpu, 2, SENTINEL);
    nd500_instr_Add3(&cpu, &fi);
    uint32_t normal = nd500_read_integer_register(&cpu, 2);
    printf("  normal run: r2=0x%X (want 0x%X)\n", normal, LHS + ADDEND);
    CHECK(normal == LHS + ADDEND, "ADD3 still computes normally when not aborted");

    /* --- The guard: an aborted operand access must leave the destination
     * untouched. --- */
    cpu.instr_aborted = 1;
    nd500_write_integer_register(&cpu, 2, SENTINEL);
    cpu.ST1 &= ~(uint32_t)ND500_FLAG_Z;
    nd500_instr_Add3(&cpu, &fi);
    uint32_t aborted = nd500_read_integer_register(&cpu, 2);
    printf("  aborted run: r2=0x%X (want the sentinel 0x%X)\n", aborted, SENTINEL);
    CHECK(aborted == SENTINEL, "aborted ADD3 does not write its destination");

    cpu.instr_aborted = 0;

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    nd500_machine_free(&m);
    return tests_failed == 0 ? 0 : 1;
}
