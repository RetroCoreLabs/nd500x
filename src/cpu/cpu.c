#include <string.h>
#include "cpu_protos.h"
#include "../machine/machine_protos.h"

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
    /* Fetch opcode (up to 2 bytes) and advance PC by opcode length */
    uint8_t b0 = nd500_bus_read8(cpu->machine, cpu->PC);
    uint8_t b1 = nd500_bus_read8(cpu->machine, cpu->PC + 1);
    uint16_t opcode = (uint16_t)b0 | ((uint16_t)b1 << 8);
    int oplen = nd500_instr_opcode_length(opcode);
    if (oplen < 1) oplen = 1;
    cpu->PC += (uint32_t)oplen;
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


