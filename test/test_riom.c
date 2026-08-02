/**
 * RIOM instruction test - "Read I/O processor memory" (ND-100 -> ND-500)
 *
 * Manual: ND-05.009.4 EN, ND-500 Reference Manual, section 16.23
 *   H RIOM <ND-100 addr/r/W>,<buffer/w/H>,<no of halfwords>
 *   - operand 1: READ, data type W (32-bit) - physical ND-100 word address
 *   - operand 2: WRITE, data type H - its EFFECTIVE ADDRESS is the ND-500
 *                destination buffer; it must NOT be dereferenced
 *   - operand 3: number of halfwords
 *   - "Privileged instruction."  Trap: IIC when not privileged.
 *   - "Data status bits: Unaffected" - no flag may change, incl. count == 0.
 *
 * This mirrors the live SINTRAN case: the ND-100 source pointer is held in a
 * memory cell, that cell is the source operand, the ND-100 address is above
 * 0xFFFF (so a 16-bit read would truncate it), and the destination is given as
 * an address-yielding operand.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

#define MEMORY_SIZE (4u << 20)     /* 4MB - must cover nd100 window at 0x40000+ */
#define SRC_PTR_CELL 0x1000u       /* ND-500 cell holding the ND-100 word address */
#define DEST_BUF     0x2000u       /* ND-500 destination buffer */
#define ND100_WORD   0x00050000u   /* ND-100 source word address (> 0xFFFF!) */
#define COUNT        4u

/* Instruction implementations have no public header in this repo; declare it. */
void nd500_instr_Riom(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0, tests_failed = 0;

static void test_result(const char* name, bool passed, const char* details) {
    if (passed) { tests_passed++; printf("  [PASS] %s\n", name); }
    else        { tests_failed++; printf("  [FAIL] %s: %s\n", name, details); }
}

/* Build the fetched instruction used by every case below. */
static void build_riom(Nd500FetchedInstruction* fi) {
    memset(fi, 0, sizeof(*fi));
    fi->address = 0x8000;
    fi->mnemonic = "riom";
    fi->operand_count = 3;
    fi->data_type = ND500_DTYPE_HALFWORD;   /* H prefix */

    /* Operand 1: memory cell holding the ND-100 address (read as W) */
    fi->operands[0].mode = ND500_ADDR_ABSOLUTE;
    fi->operands[0].effective_address = SRC_PTR_CELL;

    /* Operand 2: destination buffer - only its EFFECTIVE ADDRESS matters */
    fi->operands[1].mode = ND500_ADDR_ABSOLUTE;
    fi->operands[1].effective_address = DEST_BUF;

    /* Operand 3: halfword count, immediate constant */
    fi->operands[2].mode = ND500_ADDR_CONSTANT;
    fi->operands[2].data_len = 2;
    fi->operands[2].data[0] = (uint8_t)(COUNT >> 8);
    fi->operands[2].data[1] = (uint8_t)(COUNT & 0xFF);
}

static void seed_memory(Nd500Machine* m, Nd500Cpu* cpu) {
    /* Source pointer cell: full 32-bit ND-100 word address */
    nd500_write_memory_32(cpu, SRC_PTR_CELL, ND100_WORD);

    /* Poison the destination buffer content so that if RIOM wrongly used the
     * operand VALUE instead of its effective address we would write elsewhere. */
    for (uint32_t i = 0; i < COUNT; i++)
        nd500_write_memory_16(cpu, DEST_BUF + i * 2, 0xDEAD);

    /* Payload in the ND-100 window (offset + word*2) */
    for (uint32_t i = 0; i < COUNT; i++)
        nd500_write_memory_16(cpu, cpu->nd100_memory_offset + (ND100_WORD + i) * 2,
                              (uint16_t)(0x1100 + i));
    (void)m;
}

/* Put the CPU into the state nd500_cpu_step guarantees at the top of every
 * instruction. This test drives nd500_instr_Riom directly, so nothing else
 * restores that invariant between cases:
 *
 *   - cpu->instr_aborted is cleared at the top of each step (src/cpu/cpu.c).
 *   - A trap left pending from the previous instruction HALTS the machine
 *     before another instruction executes (src/cpu/cpu.c, the
 *     nd500_trap_occurred() check ahead of execute), so the global trap state
 *     is likewise clear whenever an instruction begins.
 *
 * Case 1 raises IIC on purpose. Without this reset that abort state leaked
 * into every later case, and RIOM's operand-abort guard correctly refused to
 * run - the instruction was fine, the harness was carrying stale state.
 */
static void begin_instruction(Nd500Cpu* cpu) {
    nd500_trap_clear();
    cpu->instr_aborted = 0;
}

int main(void) {
    printf("ND-500 RIOM Tests\n=================\n");

    Nd500Machine m; Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    Nd500FetchedInstruction fi;
    build_riom(&fi);
    seed_memory(&m, &cpu);

    /* ---- Case 1: not privileged -> IIC trap, no transfer -------------- */
    begin_instruction(&cpu);
    cpu.ST1 = 0;                       /* PIA clear */
    nd500_instr_Riom(&cpu, &fi);
    bool untouched = true;
    for (uint32_t i = 0; i < COUNT; i++)
        if (nd500_read_memory_16(&cpu, DEST_BUF + i * 2) != 0xDEAD) untouched = false;
    test_result("non-privileged RIOM performs no transfer", untouched,
                "destination buffer was modified without PIA");

    /* ---- Case 2: privileged transfer, EA destination, W source -------- */
    begin_instruction(&cpu);
    cpu.ST1 = (1U << ND500_ST_BIT_PIA);
    uint32_t st1_before = cpu.ST1, st2_before = cpu.ST2;
    nd500_instr_Riom(&cpu, &fi);
    bool ok = true; char det[128] = "";
    for (uint32_t i = 0; i < COUNT; i++) {
        uint16_t got = nd500_read_memory_16(&cpu, DEST_BUF + i * 2);
        if (got != (uint16_t)(0x1100 + i)) {
            ok = false;
            snprintf(det, sizeof(det), "halfword %u: expected 0x%04X got 0x%04X",
                     i, 0x1100 + i, got);
            break;
        }
    }
    test_result("transfer copies ND-100 words to buffer effective address", ok, det);
    test_result("status bits unaffected by transfer",
                cpu.ST1 == st1_before && cpu.ST2 == st2_before,
                "ST1/ST2 changed - manual says data status bits are unaffected");

    /* ---- Case 3: count == 0 must NOT set Z ---------------------------- */
    begin_instruction(&cpu);
    fi.operands[2].data[0] = 0; fi.operands[2].data[1] = 0;
    cpu.ST1 = (1U << ND500_ST_BIT_PIA);   /* Z deliberately clear */
    cpu.ST2 = 0;
    nd500_instr_Riom(&cpu, &fi);
    test_result("count==0 leaves Z untouched",
                (cpu.ST1 & ND500_FLAG_Z) == 0,
                "Z was set on zero count - status bits must be unaffected");

    /* ---- Case 4: REGISTER buffer operand -> IOS trap ----------------- */
    /* A register has no address in memory (manual section 15.4), so a register
     * buffer operand must raise IOS (trap 34), not perform a transfer. Mirrors
     * RetroCore Test_RIOM_RegisterBuffer_RaisesIos. */
    begin_instruction(&cpu);
    build_riom(&fi);                              /* clean instruction */
    fi.operands[1].mode = ND500_ADDR_REGISTER;    /* illegal buffer mode */
    cpu.ST1 = (1U << ND500_ST_BIT_PIA);           /* privileged: reach operand decode */
    nd500_instr_Riom(&cpu, &fi);
    {
        const Nd500TrapState* ts = nd500_trap_get_state();
        bool ios = ts->trap_occurred && (ts->trap_condition & TRAP_IOS);
        test_result("REGISTER buffer operand raises IOS", ios,
                    ts->trap_occurred ? "a trap fired but not IOS" : "no trap raised");
    }

    /* ---- Case 5: CONSTANT buffer operand -> IOS trap ----------------- */
    /* A constant has no address in memory either; same rule. Mirrors
     * RetroCore Test_RIOM_ConstantBuffer_RaisesIos. */
    begin_instruction(&cpu);
    build_riom(&fi);
    fi.operands[1].mode = ND500_ADDR_CONSTANT;    /* illegal buffer mode */
    cpu.ST1 = (1U << ND500_ST_BIT_PIA);
    nd500_instr_Riom(&cpu, &fi);
    {
        const Nd500TrapState* ts = nd500_trap_get_state();
        bool ios = ts->trap_occurred && (ts->trap_condition & TRAP_IOS);
        test_result("CONSTANT buffer operand raises IOS", ios,
                    ts->trap_occurred ? "a trap fired but not IOS" : "no trap raised");
    }

    nd500_machine_free(&m);
    printf("\n=================\nResults: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
