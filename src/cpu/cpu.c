#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"

/* Global jump buffer for trap handling */
jmp_buf cpu_jmp_buf;

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
	cpu->ST1 = cpu->ST2 = 0;  /* Initialize status registers */
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
    
    /* Check for pending ignorable traps at end of instruction */
    check_pending_traps(cpu);
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
	out->ST1 = cpu->ST1; out->ST2 = cpu->ST2;
}

/* ═══════════════════════════════════════════════════════ */
/* ND-500 TRAP SYSTEM IMPLEMENTATION */
/* ═══════════════════════════════════════════════════════ */

/**
 * Raise a trap condition in the ND-500 CPU
 *
 * @param trapBit   The trap bit(s) to set (use TRAP_xxx defines)
 * @param trapPC    PC where trap occurred
 * @param dataAddr  Related memory address (if applicable)
 *
 * For non-ignorable/fatal traps: Sets status bit and longjmps back to cpu_run()
 * For ignorable traps: Only sets status bit
 */
void raise_trap(uint64_t trapBit, uint32_t trapPC, uint32_t dataAddr) {
	/* This function needs access to the CPU structure, but we'll implement it
	 * as a global function that works with the current CPU context */
	printf("\n[TRAP] Trap 0x%016llx at PC=0x%08X Data=0x%08X\n", 
	       (unsigned long long)trapBit, trapPC, dataAddr);
	
	/* Check if this trap interrupts instruction execution */
	if (trapBit & TRAP_INTERRUPT_MASK) {
		/* Check if we're in a setjmp context */
		/* For now, just stop execution gracefully instead of longjmp */
		printf("[TRAP] Interrupting instruction execution\n");
		
		/* Instead of longjmp, we'll set a flag to stop execution */
		/* This prevents segfaults when called from debugger step */
		printf("[TRAP] Stopping execution due to non-ignorable trap\n");
		
		/* TODO: In full implementation, this would:
		 * 1. Set status bits in CPU structure
		 * 2. Check if trap is enabled
		 * 3. longjmp back to cpu_run() if in proper context
		 * 4. Or set a flag to stop execution gracefully */
		return;
	}
	
	/* Ignorable trap: just set bit, will be checked at end of instruction */
	printf("[TRAP] Ignorable trap - setting status bit\n");
}

/**
 * Check for pending ignorable traps at end of instruction
 * Called after each instruction completes
 */
void check_pending_traps(Nd500Cpu* cpu) {
	if (!cpu) return;
	
	/* Combine ST1 and ST2 into 64-bit status */
	uint64_t st = ((uint64_t)cpu->ST2 << 32) | cpu->ST1;
	uint64_t ote = ((uint64_t)cpu->OTE2 << 32) | cpu->OTE1;
	
	/* Find pending ignorable traps that are enabled */
	uint64_t pending = st & ote & TRAP_IGNORABLE_MASK;
	
	if (pending != 0) {
		/* Find highest priority trap (highest bit number) */
		for (int bit = 29; bit >= 11; bit--) {
			uint64_t trapBit = 1ULL << bit;
			if (pending & trapBit) {
				invoke_trap_handler(cpu, trapBit, cpu->PC);
				break;  /* Only handle one trap at a time */
			}
		}
	}
}

/**
 * Invoke trap handler for a specific trap
 */
void invoke_trap_handler(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trappingP) {
	if (!cpu) return;
	
	/* Calculate trap number (bit position) */
	int trapNumber = 0;
	for (int i = 0; i < 64; i++) {
		if ((trapBit >> i) & 1) {
			trapNumber = i;
			break;
		}
	}
	
	printf("[TRAP] Invoking trap handler for trap %d @PC=0x%08X\n",
	       trapNumber, trappingP);
	
	/* THA points to start address vector (64 words = 256 bytes) */
	/* Handler address = THA + (trapNumber * 4) in byte-addressed memory */
	uint32_t handlerPointer = cpu->THA + (trapNumber * 4);
	
	/* TODO: Full implementation requires:
	 * 1. Save context (registers, status) to trap handler data field
	 * 2. Clear OTE to prevent recursive traps
	 * 3. Read handler address from memory[handlerPointer]
	 * 4. Jump to handler (set PC)
	 * 5. On RETT instruction, restore context
	 */
	
	/* For now: just clear the trap bit */
	if (trapBit & 0xFFFFFFFF)
		cpu->ST1 &= ~(uint32_t)(trapBit & 0xFFFFFFFF);
	if (trapBit >> 32)
		cpu->ST2 &= ~(uint32_t)(trapBit >> 32);
}

/* ═══════════════════════════════════════════════════════ */
/* TRAP HELPER FUNCTIONS */
/* ═══════════════════════════════════════════════════════ */

void trap_illegal_instruction(uint32_t pc, uint32_t opcode) {
	printf("[TRAP] Illegal instruction 0x%04X at PC=0x%08X\n", opcode, pc);
	raise_trap(TRAP_IIC, pc, opcode);
}

void trap_illegal_operand(uint32_t pc) {
	printf("[TRAP] Illegal operand at PC=0x%08X\n", pc);
	raise_trap(TRAP_IOS, pc, 0);
}

void trap_instruction_sequence_error(uint32_t pc) {
	printf("[TRAP] Instruction sequence error at PC=0x%08X\n", pc);
	raise_trap(TRAP_ISE, pc, 0);
}

void trap_protect_violation(uint32_t pc, uint32_t address) {
	printf("[TRAP] Protect violation at PC=0x%08X address=0x%08X\n", pc, address);
	raise_trap(TRAP_PV, pc, address);
}

void trap_page_fault(uint32_t pc, uint32_t address) {
	printf("[TRAP] Page fault at PC=0x%08X address=0x%08X\n", pc, address);
	raise_trap(TRAP_PGF, pc, address);
}

void trap_divide_by_zero(uint32_t pc) {
	printf("[TRAP] Divide by zero at PC=0x%08X\n", pc);
	raise_trap(TRAP_DZ, pc, 0);
}

void trap_floating_overflow(uint32_t pc) {
	printf("[TRAP] Floating overflow at PC=0x%08X\n", pc);
	raise_trap(TRAP_FO, pc, 0);
}

void trap_floating_underflow(uint32_t pc) {
	printf("[TRAP] Floating underflow at PC=0x%08X\n", pc);
	raise_trap(TRAP_FU, pc, 0);
}

void trap_invalid_operation(uint32_t pc) {
	printf("[TRAP] Invalid operation at PC=0x%08X\n", pc);
	raise_trap(TRAP_IVO, pc, 0);
}

void trap_stack_overflow(uint32_t pc) {
	printf("[TRAP] Stack overflow at PC=0x%08X\n", pc);
	raise_trap(TRAP_STO, pc, 0);
}

void trap_stack_underflow(uint32_t pc) {
	printf("[TRAP] Stack underflow at PC=0x%08X\n", pc);
	raise_trap(TRAP_STU, pc, 0);
}

void trap_breakpoint(uint32_t pc) {
	printf("[TRAP] Breakpoint at PC=0x%08X\n", pc);
	raise_trap(TRAP_BPT, pc, 0);
}

void trap_single_instruction(uint32_t pc) {
	printf("[TRAP] Single instruction trap at PC=0x%08X\n", pc);
	raise_trap(TRAP_SIT, pc, 0);
}

void trap_branch(uint32_t pc) {
	printf("[TRAP] Branch trap at PC=0x%08X\n", pc);
	raise_trap(TRAP_BT, pc, 0);
}

void trap_call(uint32_t pc) {
	printf("[TRAP] Call trap at PC=0x%08X\n", pc);
	raise_trap(TRAP_CT, pc, 0);
}

/**
 * Run the CPU for specified number of steps with trap handling
 * 
 * @param cpu    CPU to run
 * @param steps  Number of steps to run (-1 = infinite)
 * @return       Remaining steps
 */
int nd500_cpu_run(Nd500Cpu* cpu, int steps) {
	if (!cpu || !cpu->machine) return steps;
	
	/* Set up longjmp target for trap handling */
	/* Returns 0 on initial call, non-zero when longjmp is called */
	int trap_occurred = setjmp(cpu_jmp_buf);
	
	if (trap_occurred != 0) {
		/* We arrived here via longjmp from a trap! */
		/* The instruction was interrupted mid-execution */
		printf("[CPU] Trap handler returned, PC=0x%08X\n", cpu->PC);
		
		/* Stop execution when trap occurs */
		cpu->machine->run_flag = 0;
		return steps;
	}
	
	/* Main execution loop */
	while (steps != 0 && cpu->machine->run_flag) {
		/* Execute one instruction */
		nd500_cpu_step(cpu);
		
		/* Decrement step counter */
		if (steps > 0)
			steps--;
	}
	
	return steps;
}


