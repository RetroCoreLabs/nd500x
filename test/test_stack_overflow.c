/*
 * STO (Stack Overflow) status-bit tests for the heap/stack instructions.
 *
 * Guards the ND-500 Reference Manual rule (traps section):
 *   "The STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT,
 *    ENTM and GETB instruction."
 *
 * The bug this catches: the emulator used to SET STO on heap exhaustion but
 * never RESET it on success, so the bit went stale after the first overflow.
 * It also used to "auto-initialize" the heap from STAH/ENDH on exhaustion
 * (forbidden by section 3.3), handing out overlapping blocks - the root
 * cause of the NC compiler's "no rewrite" corruption. GETB must instead
 * raise STO so the program's own trap handler extends the heap.
 *
 * GETB is tested directly (self-contained: it only needs TOS -> heap vars).
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"

extern void nd500_instr_Getb(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

/* Heap layout (section 3.3, Figure 4), pointed to by TOS:
 *   +0            MAXL
 *   +4            STAH   (reserved for the trap handler; GETB must not touch)
 *   +8            ENDH   (reserved for the trap handler; GETB must not touch)
 *   +12 + 4*k     FLOG[k]  freelist head for 2^k-word elements (0 = empty)
 * A freelist element's first word is the link to the next element (0 = end).
 */
#define HEAP_VARS   0x1000u
#define MAXL_OFF    0
#define STAH_OFF    4
#define ENDH_OFF    8
#define FLOG(k)     (HEAP_VARS + 12u + 4u * (k))

/* Build a fetched GETB instruction: Wn := GETB <log_size constant>. */
static Nd500FetchedInstruction make_getb(uint8_t target_reg, uint8_t log_size) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = 0x2000;
    fi.opcode = 0xFDA0;              /* GETB (value not used by the handler) */
    fi.operand_count = 1;
    fi.target_register = target_reg; /* 1..4 -> W1..W4 */
    fi.data_type = ND500_DTYPE_BYTE;
    fi.operands[0].mode = ND500_ADDR_CONSTANT_SHORT;  /* short constant 0..31 */
    fi.operands[0].address_code = log_size & 0x3F;
    return fi;
}

static void reset_cpu(Nd500Cpu* cpu) {
    cpu->ST1 = 0;
    cpu->ST2 = 0;
    nd500_trap_clear();
    cpu->TOS = HEAP_VARS;
    cpu->I[0] = cpu->I[1] = cpu->I[2] = cpu->I[3] = 0xEEEEEEEE;  /* poison */
}

int main(void) {
    printf("STO status-bit tests (heap/stack instructions)\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, 0x100000);   /* 1 MB */
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* ---- Test 1: successful GETB clears a pre-set STO bit ---------------- */
    printf("\nTest 1: successful GETB resets STO\n");
    reset_cpu(&cpu);
    /* MAXL=4; put one 2^2 element (address 0x8000) on FLOG[2], link=0 (end) */
    nd500_write_memory_32(&cpu, HEAP_VARS + MAXL_OFF, 4);
    nd500_write_memory_32(&cpu, FLOG(2), 0x8000);
    nd500_write_memory_32(&cpu, 0x8000, 0);           /* element.next = end */
    cpu.ST1 |= (uint32_t)TRAP_STO;                  /* stale bit from an earlier overflow */
    {
        Nd500FetchedInstruction fi = make_getb(1, 2);
        nd500_instr_Getb(&cpu, &fi);
    }
    CHECK(cpu.I[0] == 0x8000, "GETB returned the freelist element in W1");
    CHECK(nd500_read_memory_32(&cpu, FLOG(2)) == 0, "FLOG[2] unlinked (now empty)");
    CHECK((cpu.ST1 & (uint32_t)TRAP_STO) == 0, "STO reset on successful allocation");

    /* ---- Test 2: exhaustion sets STO and leaves state untouched --------- */
    printf("\nTest 2: GETB exhaustion sets STO, no allocation, STAH/ENDH untouched\n");
    reset_cpu(&cpu);
    nd500_write_memory_32(&cpu, HEAP_VARS + MAXL_OFF, 4);
    nd500_write_memory_32(&cpu, HEAP_VARS + STAH_OFF, 0x9000);  /* must NOT be consulted */
    nd500_write_memory_32(&cpu, HEAP_VARS + ENDH_OFF, 0xA000);
    for (int k = 0; k <= 4; k++) nd500_write_memory_32(&cpu, FLOG(k), 0);  /* all empty */
    {
        Nd500FetchedInstruction fi = make_getb(2, 2);
        nd500_instr_Getb(&cpu, &fi);
    }
    CHECK((cpu.ST1 & (uint32_t)TRAP_STO) != 0, "STO set on exhaustion");
    CHECK(cpu.I[1] == 0xEEEEEEEE, "W2 left unchanged on exhaustion (no phantom block)");
    CHECK(nd500_read_memory_32(&cpu, FLOG(2)) == 0,
          "freelist not re-seeded from STAH/ENDH (section 3.3)");

    /* ---- Test 3: requested size > MAXL sets STO ------------------------- */
    printf("\nTest 3: GETB with log_size > MAXL sets STO\n");
    reset_cpu(&cpu);
    nd500_write_memory_32(&cpu, HEAP_VARS + MAXL_OFF, 3);
    {
        Nd500FetchedInstruction fi = make_getb(3, 5);  /* 5 > MAXL 3 */
        nd500_instr_Getb(&cpu, &fi);
    }
    CHECK((cpu.ST1 & (uint32_t)TRAP_STO) != 0, "STO set when log_size exceeds MAXL");

    /* ---- Test 4: split a larger element, upper half returned to freelist  */
    printf("\nTest 4: GETB splits a larger element and resets STO\n");
    reset_cpu(&cpu);
    nd500_write_memory_32(&cpu, HEAP_VARS + MAXL_OFF, 4);
    /* Only a 2^3 element available at 0x8000; request 2^2 -> split once. */
    nd500_write_memory_32(&cpu, FLOG(3), 0x8000);
    nd500_write_memory_32(&cpu, 0x8000, 0);
    cpu.ST1 |= (uint32_t)TRAP_STO;
    {
        Nd500FetchedInstruction fi = make_getb(4, 2);
        nd500_instr_Getb(&cpu, &fi);
    }
    /* Lower half (0x8000) returned; upper half at 0x8000 + 2^2 words*4 = +16 */
    CHECK(cpu.I[3] == 0x8000, "GETB returned lower half of the split element");
    CHECK(nd500_read_memory_32(&cpu, FLOG(2)) == 0x8000 + (1u << 2) * 4u,
          "upper half placed on FLOG[2]");
    CHECK((cpu.ST1 & (uint32_t)TRAP_STO) == 0, "STO reset after successful split");

    nd500_machine_free(&m);
    printf("\nPassed: %d\nFailed: %d\n", tests_passed, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
