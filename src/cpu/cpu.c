#include <string.h>
#include <stdio.h>
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"

void nd500_cpu_init(Nd500Cpu* cpu, Nd500Machine* machine) {
	if (!cpu) return;
	memset(cpu, 0, sizeof(*cpu));
	cpu->machine = machine;
	if (machine) machine->cpu = cpu;
}

void nd500_cpu_reset(Nd500Cpu* cpu) {
	if (!cpu) return;
	cpu->PC = 0;
	cpu->FLAGS = 0;
	memset(cpu->I, 0, sizeof(cpu->I));
	memset(cpu->A, 0, sizeof(cpu->A));
	memset(cpu->E, 0, sizeof(cpu->E));
	cpu->L = cpu->B = cpu->R = 0;
	cpu->TOS = cpu->LL = cpu->HL = cpu->THA = 0;
	cpu->OTE1 = cpu->OTE2 = cpu->CTE1 = cpu->CTE2 = 0;
	cpu->MTE1 = cpu->MTE2 = cpu->TEMM1 = cpu->TEMM2 = 0;
}

void nd500_cpu_step(Nd500Cpu* cpu) {
	if (!cpu || !cpu->machine) return;
	
	/* Check breakpoints before executing instruction */
	if (cpu->machine->bp_mgr && bp_should_break_at(cpu->machine->bp_mgr, cpu->PC)) {
		cpu->machine->run_flag = 0; /* Stop execution */
		return; /* Don't execute this instruction yet */
	}
	
	/* Trap on invalid instruction 0x00 (uninitialized memory) */
	if (nd500_dbg_get_trap_invalid()) {
		uint8_t opcode_byte = nd500_bus_read8(cpu->machine, cpu->PC);
		if (opcode_byte == 0x00) {
			printf("\n[TRAP] Invalid instruction 0x00 at PC=0x%08X (uninitialized memory)\n", cpu->PC);
			cpu->machine->run_flag = 0; /* Stop execution */
			return;
		}
	}
	
    /* Decode, execute, then advance PC by decoded length */
    Nd500FetchedInstruction fi;
    uint32_t old_pc = cpu->PC;
    if (nd500_decode_at(cpu->machine, old_pc, &fi) != 0) return;
    
    /* Trace instruction execution if enabled */
    if (nd500_dbg_get_trace_mode()) {
        uint32_t regs[8] = {cpu->PC, cpu->I[0], cpu->I[1], cpu->I[2], cpu->I[3], cpu->L, cpu->B, cpu->R};
        nd500_dbg_trace_instruction(old_pc, fi.mnemonic, regs);
    }
    
    /* Profile instruction execution if enabled */
    if (nd500_dbg_get_profiling()) {
        nd500_dbg_profile_instruction(fi.mnemonic);
    }
    
    nd500_execute_decoded(cpu, &fi);
    if (cpu->PC == old_pc) {
        cpu->PC += fi.total_len ? fi.total_len : fi.opcode_len;
    }
}

void nd500_cpu_get_regs(Nd500Cpu* cpu, Nd500Regs* out) {
	if (!cpu || !out) return;
	out->PC = cpu->PC;
	out->FLAGS = cpu->FLAGS;
	for (int i = 0; i < 4; ++i) { out->I[i] = cpu->I[i]; out->A[i] = cpu->A[i]; out->E[i] = cpu->E[i]; }
	out->L = cpu->L; out->B = cpu->B; out->R = cpu->R;
	out->TOS = cpu->TOS; out->LL = cpu->LL; out->HL = cpu->HL; out->THA = cpu->THA;
	out->OTE1 = cpu->OTE1; out->OTE2 = cpu->OTE2; out->CTE1 = cpu->CTE1; out->CTE2 = cpu->CTE2;
	out->MTE1 = cpu->MTE1; out->MTE2 = cpu->MTE2; out->TEMM1 = cpu->TEMM1; out->TEMM2 = cpu->TEMM2;
}


