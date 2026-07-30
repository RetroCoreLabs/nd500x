/*
 * ENTS pending-CALL interlock must survive a trap dispatch + handler + RETT.
 *
 * The CALL/ENT* sequence interlock ("an ENT* must be preceded by CALL/CALLG")
 * is a REAL hardware interlock (microword C,SEQ / INVSEQ; MICRO-5800-B30 CALL
 * @000646, CALLG @000652 set it; ENTD @000660 -> INS_SEQ_ERR @003141 reads
 * IDU,STS). The sequence state lives in the IDU status, which is part of the
 * trap-saved context (ENTT1 @014042 carries C,SEQ).
 *
 * The bug: pending_call_return_address et al. were a naked cpu field with NO
 * save/restore across trap dispatch. A CALL sets it and jumps to a callee whose
 * first instruction is ENTS; if that ENTS page-faults, the trap dispatches into
 * the kernel, whose OWN CALL/ENT* pairs CLEAR the field, and on RETT the resumed
 * ENTS saw the field == 0 and raised a FALSE Instruction-Sequence-Error (ISE).
 *
 * Fix: invoke_trap_handler() saves pending_call_* into trap_saved_pending_call_*
 * (and clears the live fields); the RETT instruction restores them. This test
 * drives exactly that path and asserts the resumed ENTS does NOT false-ISE and
 * builds its frame from the RESTORED return address. Two controls prove the
 * interlock itself is NOT deleted: an ENTS with no preceding CALL still ISEs.
 */

#include <stdio.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

extern void nd500_instr_Ents(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Entt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
extern void nd500_instr_Rett(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

static int tests_passed = 0;
static int tests_failed = 0;
#define CHECK(cond, name) do { \
    if (cond) { printf("  PASS: %s\n", name); tests_passed++; } \
    else      { printf("  FAIL: %s\n", name); tests_failed++; } \
} while (0)

#define MEMORY_SIZE   (8 * 1024 * 1024)

/* Scenario geometry. */
#define OLD_B          0x2000u
#define NEW_B          0x2100u
#define STACK_DEMAND   0x20u
#define SAVED_RETADDR  0x5000u
#define SAVED_ARG0     0xABCD1234u
#define THA_BASE       0x40000u
#define HANDLER_ADDR   0x50000u
#define ENTS_PC        0x1000u
#define PGF_TRAP_NUM   38

/* Build a fetched instruction whose operand(s) are word constants. */
static void set_const_word(Nd500OperandDecoded* op, uint32_t value) {
    memset(op, 0, sizeof(*op));
    op->mode = ND500_ADDR_CONSTANT;
    op->data_len = 4;                 /* word constant, NC (no sign issues) */
    op->data[0] = (uint8_t)(value >> 24);
    op->data[1] = (uint8_t)(value >> 16);
    op->data[2] = (uint8_t)(value >> 8);
    op->data[3] = (uint8_t)(value);
}

static Nd500FetchedInstruction make_ents(uint32_t stack_demand) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = ENTS_PC;
    fi.opcode = 0x00B8;               /* ENTS */
    fi.operand_count = 1;
    fi.data_type = ND500_DTYPE_WORD;
    set_const_word(&fi.operands[0], stack_demand);
    return fi;
}

static Nd500FetchedInstruction make_entt(uint32_t local_data_size, uint32_t stack_demand) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = HANDLER_ADDR;
    fi.opcode = 0x00BC;               /* ENTT */
    fi.operand_count = 2;
    fi.data_type = ND500_DTYPE_WORD;
    set_const_word(&fi.operands[0], local_data_size);
    set_const_word(&fi.operands[1], stack_demand);
    return fi;
}

static Nd500FetchedInstruction make_rett(void) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = HANDLER_ADDR + 16;
    fi.opcode = 0x0083;               /* RETT */
    fi.operand_count = 0;
    fi.data_type = ND500_DTYPE_WORD;
    return fi;
}

/* Pre-trap register/memory state that makes ENTS build a frame successfully. */
static void setup_ents_frame_state(Nd500Machine* m, Nd500Cpu* cpu) {
    nd500_bus_write32(m, OLD_B + 8, NEW_B);   /* B.SP -> address of new frame */
    cpu->B = OLD_B;
    cpu->TOS = NEW_B + STACK_DEMAND + 0x100;  /* headroom -> ENTS succeeds */
    cpu->L = 0xDEADu;
}

static void set_pending_call(Nd500Cpu* cpu) {
    cpu->pending_call_return_address = SAVED_RETADDR;
    cpu->pending_call_arg_count = 1;
    cpu->pending_call_arg_addresses[0] = SAVED_ARG0;
}


/* LREGBL with an immediate mask and an immediate block address, mirroring NDIX's
 * `lregbl $CNTXMASK,r3` trap-return in machine/locore.c:535. */
static Nd500FetchedInstruction make_lregbl(uint32_t mask, uint32_t block_addr) {
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof(fi));
    fi.address = HANDLER_ADDR + 16;
    fi.opcode = 0x00A4;               /* LREGBL */
    fi.operand_count = 2;
    fi.data_type = ND500_DTYPE_WORD;

    fi.operands[0].mode = ND500_ADDR_CONSTANT;
    fi.operands[0].data_len = 4;
    fi.operands[0].data[0] = (uint8_t)(mask >> 24);
    fi.operands[0].data[1] = (uint8_t)(mask >> 16);
    fi.operands[0].data[2] = (uint8_t)(mask >> 8);
    fi.operands[0].data[3] = (uint8_t)mask;

    fi.operands[1].mode = ND500_ADDR_CONSTANT;
    fi.operands[1].data_len = 4;
    fi.operands[1].data[0] = (uint8_t)(block_addr >> 24);
    fi.operands[1].data[1] = (uint8_t)(block_addr >> 16);
    fi.operands[1].data[2] = (uint8_t)(block_addr >> 8);
    fi.operands[1].data[3] = (uint8_t)block_addr;
    return fi;
}

/* ---- NDIX's REAL trap return: lregbl, not RETT ----
 *
 * NDIX never executes RETT for a kernel trap; machine/locore.c trapex ends the
 * handler with `lregbl $CNTXMASK,r3`. Before this path popped the interlock,
 * ONE NDIX BOOT MEASURED 132 ENTT pushes and 0 pops - the ring saturated and
 * every saved interlock was lost, so a page fault taken on a user program's own
 * ENTS resumed with pending_call_return_address == 0 and the retried ENTS died
 * with a false ISE ("Memory fault - core dumped" running vi at PC=0x0000FC15). */
static void test_lregbl_trap_return_restores(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== NDIX trap return via LREGBL (not RETT) restores the interlock ===\n");

    const uint32_t BLOCK = 0x00030000u;   /* saved context block trapex reloads from */
    const uint32_t RESUME_PC = 0x00002000u;

    cpu->THA = THA_BASE;
    nd500_bus_write32(m, THA_BASE + PGF_TRAP_NUM * 4, HANDLER_ADDR);
    nd500_bus_write8(m, HANDLER_ADDR, 0xBC);

    setup_ents_frame_state(m, cpu);
    set_pending_call(cpu);

    cpu->cur_instr_pc = ENTS_PC;
    invoke_trap_handler(cpu, TRAP_PGF, ENTS_PC);

    Nd500FetchedInstruction entt = make_entt(0x20, 0x100);
    nd500_instr_Entt(cpu, &entt);
    CHECK(cpu->pending_call_return_address == 0, "ENTT cleared the live interlock");
    CHECK(cpu->in_trap_handler, "ENTT marked us inside the handler");

    /* The kernel handler runs its own CALL/ENT* pairs, trampling the live field. */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* trapex: reload P (reg 1) from the saved context block. This IS the return. */
    nd500_bus_write32(m, BLOCK + 1 * 4, RESUME_PC);
    Nd500FetchedInstruction lregbl = make_lregbl(0x1u, BLOCK);
    nd500_instr_Lregbl(cpu, &lregbl);

    CHECK(!cpu->in_trap_handler, "lregbl reloading P ends the handler");
    CHECK(cpu->pending_call_return_address == SAVED_RETADDR,
          "lregbl trap-return restores the pending-call return address");
    CHECK(cpu->pending_call_arg_count == 1,
          "lregbl trap-return restores the pending arg count");
    CHECK(cpu->pending_call_arg_addresses[0] == SAVED_ARG0,
          "lregbl trap-return restores the pending arg EAs");

    /* And the resumed ENTS must NOT raise a false ISE. */
    cpu->THA = 0;
    cpu->B = OLD_B;
    cpu->ST1 = 0;
    Nd500FetchedInstruction ents = make_ents(STACK_DEMAND);
    nd500_instr_Ents(cpu, &ents);
    CHECK((cpu->ST1 & (1u << 35)) == 0 || cpu->B == NEW_B,
          "the resumed ENTS raises no false ISE after an lregbl trap return");
}

/* ---- The fix: pending-call state survives trap -> handler -> RETT ---- */
static void test_survives_trap(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== ENTS pending-CALL survives a page-fault trap + handler + RETT ===\n");

    /* Install a valid PGF handler (must START with ENTT opcode 0xBC). */
    cpu->THA = THA_BASE;
    nd500_bus_write32(m, THA_BASE + PGF_TRAP_NUM * 4, HANDLER_ADDR);
    nd500_bus_write8(m, HANDLER_ADDR, 0xBC);

    setup_ents_frame_state(m, cpu);
    set_pending_call(cpu);

    /* Dispatch the page fault the callee's ENTS would have taken. */
    cpu->cur_instr_pc = ENTS_PC;
    invoke_trap_handler(cpu, TRAP_PGF, ENTS_PC);

    CHECK(cpu->PC == HANDLER_ADDR, "PGF dispatches to the THA[38] handler");
    CHECK(cpu->pending_call_return_address == SAVED_RETADDR,
          "dispatch itself does not touch the interlock (ENTT owns save+clear)");

    /* Handler prologue: ENTT builds the trap frame at THA+256, and saves+clears the
     * interlock under that frame's address. Dispatch verifies ENTT is the handler's
     * first instruction, so nothing runs in between and the guest cannot tell the
     * difference between clearing here and clearing at dispatch. */
    Nd500FetchedInstruction entt = make_entt(0x20, 0x100);
    nd500_instr_Entt(cpu, &entt);

    CHECK(cpu->pending_call_return_address == 0,
          "ENTT clears the naked pending-call field (state moved into the trap ctx)");

    /* Simulate the kernel handler running its OWN CALL/ENT* pair (clears field). */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* RETT restores the interlock from the trap context and resumes. */
    Nd500FetchedInstruction rett = make_rett();
    nd500_instr_Rett(cpu, &rett);

    CHECK(cpu->pending_call_return_address == SAVED_RETADDR,
          "RETT restores the pending-call return address");
    CHECK(cpu->pending_call_arg_count == 1, "RETT restores the pending arg count");
    CHECK(cpu->pending_call_arg_addresses[0] == SAVED_ARG0, "RETT restores the pending arg EAs");
    CHECK(cpu->B == OLD_B, "RETT restored B to the pre-trap base");

    /* The resumed ENTS must now succeed - no false ISE - and use the restored addr. */
    cpu->THA = 0;                 /* isolate: any ISE now records cleanly, no dispatch */
    nd500_trap_clear();
    Nd500FetchedInstruction ents = make_ents(STACK_DEMAND);
    nd500_instr_Ents(cpu, &ents);

    CHECK(!nd500_trap_occurred(),
          "resumed ENTS must NOT raise a (false) ISE once the interlock is restored");
    CHECK(cpu->B == NEW_B, "ENTS switched B to the new frame");
    CHECK(nd500_read_memory_32(cpu, NEW_B + 4) == SAVED_RETADDR,
          "the frame RETA is the RESTORED return address");
}

/* ---- Positive control: ENTS with no preceding CALL still raises ISE ---- */
static void test_no_call_still_ise(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== Control: ENTS with no preceding CALL still raises ISE ===\n");

    setup_ents_frame_state(m, cpu);
    cpu->THA = 0;
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;
    nd500_trap_clear();

    Nd500FetchedInstruction ents = make_ents(STACK_DEMAND);
    nd500_instr_Ents(cpu, &ents);

    CHECK(nd500_trap_occurred(), "ENTS with no preceding CALL raises a trap");
    CHECK(nd500_trap_get_state()->trap_condition == TRAP_ISE,
          "the raised trap is ISE (bit 35)");
}

/* ---- Negative control: a bare mid-stream ENTS (no CALL, no trap) still ISEs ---- */
static void test_bare_midstream_ise(Nd500Machine* m, Nd500Cpu* cpu) {
    printf("\n=== Control: a bare mid-stream ENTS still raises ISE ===\n");

    cpu->THA = 0;
    cpu->B = OLD_B;
    nd500_bus_write32(m, OLD_B + 8, NEW_B);
    cpu->TOS = NEW_B + STACK_DEMAND + 0x100;
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;
    nd500_trap_clear();

    Nd500FetchedInstruction ents = make_ents(STACK_DEMAND);
    nd500_instr_Ents(cpu, &ents);

    CHECK(nd500_trap_occurred(), "a bare mid-stream ENTS raises a trap");
    CHECK(nd500_trap_get_state()->trap_condition == TRAP_ISE,
          "the raised trap is ISE (bit 35)");
}

int main(void) {
    printf("ENTS pending-CALL interlock trap-survival tests\n");
    printf("===============================================\n");

    nd500_quiet = 1;

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    test_survives_trap(&m, &cpu);

    nd500_cpu_init(&cpu, &m);
    test_lregbl_trap_return_restores(&m, &cpu);

    /* Fresh CPU state for the controls. */
    nd500_cpu_init(&cpu, &m);
    test_no_call_still_ise(&m, &cpu);

    nd500_cpu_init(&cpu, &m);
    test_bare_midstream_ise(&m, &cpu);

    nd500_machine_free(&m);

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
