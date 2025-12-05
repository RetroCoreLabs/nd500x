#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include "cpu_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include "nd500_domain.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../disasm/nd500_disasm.h"

/* Global jump buffer for trap handling */
jmp_buf cpu_jmp_buf;

/* Global trap state */
Nd500TrapState g_trap_state = {0};

void nd500_cpu_init(Nd500Cpu* cpu, Nd500Machine* machine) {
	if (!cpu) return;
	memset(cpu, 0, sizeof(*cpu));
	cpu->machine = machine;
	if (machine) machine->cpu = cpu;

	/* Initialize ND-100 I/O Processor Bridge */
	cpu->nd100_memory_offset = 0x40000;  /* Default: ND-100 memory at physical offset 0x40000 */

	/* MMU and domain tables are NOT pre-allocated.
	 * User must configure MMU via init script commands before 'mmu enable'.
	 * Tables are allocated on first use (lazy initialization). */
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
	/* Initialize status registers - CPU boots in PRIVILEGED mode (PIA=1)
	 * This allows the OS kernel to execute privileged instructions during boot
	 * (DCTSB, PCTSB, INIT, etc.) before user mode is established.
	 * User programs must explicitly set PIA=0 before returning to user space. */
	cpu->ST1 = (1u << ND500_ST_BIT_PIA);  /* Set PIA bit - privileged mode */
	cpu->ST2 = 0;

	/* Initialize MMU registers */
	cpu->PSTP = cpu->DITBASE = cpu->CED = cpu->CAD = cpu->PS = 0;

	/* Clear CALL/ENT handshake state */
	cpu->pending_call_return_address = 0;
	cpu->pending_call_arg_count = 0;
	memset(cpu->pending_call_arg_addresses, 0, sizeof(cpu->pending_call_arg_addresses));

	/* Clear variable operand buffer */
	cpu->extra_operand_count = 0;
	memset(cpu->extra_operands, 0, sizeof(cpu->extra_operands));

	/* Clear any pending traps */
	nd500_trap_clear();
}

void nd500_cpu_step(Nd500Cpu* cpu) {
	if (!cpu || !cpu->machine) return;

	/* Check for pending traps before executing instruction */
	if (nd500_trap_occurred()) {
		const Nd500TrapState* trap = nd500_trap_get_state();
		/* Trap detected - stop execution and clear trap state */
		cpu->machine->run_flag = 0; /* Stop execution */
		cpu->machine->stop_reason = trap ? trap->trap_description : "Trap occurred";
		nd500_trap_clear(); /* Clear trap so debugger can inspect memory */
		return;
	}

	/* Check breakpoints before executing instruction */
	if (cpu->machine->bp_mgr && bp_should_break_at(cpu->machine->bp_mgr, cpu->PC)) {
		cpu->machine->run_flag = 0; /* Stop execution */
		cpu->machine->stop_reason = "Breakpoint hit";
		return; /* Don't execute this instruction yet */
	}
	
	/* Trap on invalid instruction 0x00 (uninitialized memory) */
	if (nd500_dbg_get_trap_invalid()) {
		/* Use MMU-aware read for instruction fetch */
		uint8_t opcode_byte;
		if (cpu->machine->mmu_enabled) {
			/* Translate virtual → physical address */
			uint32_t paddr = nd500_mmu_translate(cpu, cpu->PC, 0, 1); /* is_write=0, is_instruction=1 */
			opcode_byte = nd500_bus_read8(cpu->machine, paddr);
		} else {
			opcode_byte = nd500_bus_read8(cpu->machine, cpu->PC);
		}
		if (opcode_byte == 0x00) {
			/* Invalid instruction 0x00 detected (uninitialized memory) */
			nd500_trap_set_state(TRAP_IIC, cpu->PC, 0, "Invalid instruction 0x00 (uninitialized memory)");
			cpu->machine->run_flag = 0; /* Stop execution */
			cpu->machine->stop_reason = "Invalid instruction 0x00";
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
        /* Get full disassembly for trace output */
        char disasm_buf[256];
        nd500_disasm_format_range(cpu->machine, old_pc, fi.total_len ? fi.total_len : fi.opcode_len, disasm_buf, sizeof(disasm_buf));
        /* Remove trailing newline if present */
        size_t len = strlen(disasm_buf);
        if (len > 0 && disasm_buf[len-1] == '\n') disasm_buf[len-1] = '\0';
        nd500_dbg_trace_instruction(old_pc, disasm_buf, regs);
    }
    
    /* Profile instruction execution if enabled */
    if (nd500_dbg_get_profiling()) {
        nd500_dbg_profile_instruction(fi.mnemonic);
    }
    
    /* Advance PC BEFORE execution (like C# implementation)
     * Branch/jump instructions will overwrite PC as needed */
    cpu->PC = old_pc + (fi.total_len ? fi.total_len : fi.opcode_len);

    nd500_execute_decoded(cpu, &fi);
    
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
	out->PSTP = cpu->PSTP; out->DITBASE = cpu->DITBASE; out->CED = cpu->CED;
	out->CAD = cpu->CAD; out->PS = cpu->PS;
}

/* ═══════════════════════════════════════════════════════ */
/* ND-500 TRAP SYSTEM IMPLEMENTATION */
/* ═══════════════════════════════════════════════════════ */

/**
 * Raise a trap condition in the ND-500 CPU
 *
 * @param cpu       CPU structure to modify
 * @param trapBit   The trap bit(s) to set (use TRAP_xxx defines)
 * @param trapPC    PC where trap occurred
 * @param dataAddr  Related memory address (if applicable)
 *
 * For non-ignorable/fatal traps: Sets status bit and longjmps back to cpu_run()
 * For ignorable traps: Sets status bit only if enabled in OTE mask
 */
void raise_trap(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trapPC, uint32_t dataAddr) {
	if (!cpu) return;

	/* Set the corresponding bit in ST1/ST2 status registers */
	if (trapBit & 0xFFFFFFFF) {
		cpu->ST1 |= (uint32_t)(trapBit & 0xFFFFFFFF);
	}
	if (trapBit >> 32) {
		cpu->ST2 |= (uint32_t)(trapBit >> 32);
	}

	/* Set trap state for the runner to check */
	nd500_trap_set_state(trapBit, trapPC, dataAddr, "Trap occurred during instruction execution");

	/* Check if this is a non-ignorable trap (bits 0-10) */
	if (trapBit & TRAP_INTERRUPT_MASK) {
		/* Non-ignorable trap - stop execution */
		return;
	}

	/* Ignorable trap (bits 11-29): check if enabled in OTE mask */
	uint64_t ote = ((uint64_t)cpu->OTE2 << 32) | cpu->OTE1;
	/* If enabled in OTE, trap will be checked at end of instruction */
	/* If not enabled, trap is suppressed */
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

void trap_illegal_instruction(Nd500Cpu* cpu, uint32_t pc, uint32_t opcode) {
	raise_trap(cpu, TRAP_IIC, pc, opcode);
}

void trap_illegal_operand(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_IOS, pc, 0);
}

void trap_instruction_sequence_error(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_ISE, pc, 0);
}

void trap_protect_violation(Nd500Cpu* cpu, uint32_t pc, uint32_t address) {
	raise_trap(cpu, TRAP_PV, pc, address);
}

void trap_page_fault(Nd500Cpu* cpu, uint32_t pc, uint32_t address) {
	raise_trap(cpu, TRAP_PGF, pc, address);
}

void trap_divide_by_zero(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_DZ, pc, 0);
}

void trap_floating_overflow(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_FO, pc, 0);
}

void trap_floating_underflow(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_FU, pc, 0);
}

void trap_invalid_operation(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_IVO, pc, 0);
}

void trap_stack_overflow(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_STO, pc, 0);
}

void trap_stack_underflow(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_STU, pc, 0);
}

void trap_breakpoint(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_BPT, pc, 0);
}

void trap_single_instruction(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_SIT, pc, 0);
}

void trap_branch(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_BT, pc, 0);
}

void trap_call(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_CT, pc, 0);
}

/* ═══════════════════════════════════════════════════════ */
/* TRAP STATE MANAGEMENT */
/* ═══════════════════════════════════════════════════════ */

void nd500_trap_clear(void) {
    memset(&g_trap_state, 0, sizeof(g_trap_state));
}

int nd500_trap_occurred(void) {
    return g_trap_state.trap_occurred;
}

const Nd500TrapState* nd500_trap_get_state(void) {
    return &g_trap_state;
}

void nd500_trap_set_state(uint64_t condition, uint32_t pc, uint32_t data_addr, const char* description) {
    g_trap_state.trap_occurred = 1;
    g_trap_state.trap_condition = condition;
    g_trap_state.trap_pc = pc;
    g_trap_state.trap_data_addr = data_addr;
    if (description) {
        strncpy(g_trap_state.trap_description, description, sizeof(g_trap_state.trap_description) - 1);
        g_trap_state.trap_description[sizeof(g_trap_state.trap_description) - 1] = '\0';
    } else {
        g_trap_state.trap_description[0] = '\0';
    }
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

/* ═══════════════════════════════════════════════════════ */
/* ND-100 I/O PROCESSOR BRIDGE IMPLEMENTATION */
/* ═══════════════════════════════════════════════════════ */

/**
 * Read a word from ND-100 I/O processor memory space via RIOM/DMA
 *
 * @param cpu        CPU structure containing bridge configuration
 * @param nd100_addr ND-100 physical word address (24-bit, 0x000000-0x3FFFFF)
 * @return           Halfword value read from ND-100 memory
 *
 * ND-100 Physical Memory Architecture:
 *
 * ND-100 uses 24-bit word addressing (22-bit physical addresses):
 *   0x000000 - 0x00FFFF (64K words, 128KB)  : Low RAM (boot, kernel, RT programs)
 *   0x010000 - 0x03FFFF (192K words, 384KB) : Extended RAM (programs, buffers)
 *   0x040000 - 0x05FFFF (128K words, 256KB) : 5MPM (shared multiport memory)
 *   0x060000 - 0x3FFFFF (remaining space)   : Additional RAM (system dependent)
 *
 * Address Translation:
 * - ND-100 uses word addressing (address × 2 = byte offset)
 * - ND-500 uses byte addressing (32-bit)
 * - Translation: physical_addr = nd100_memory_offset + (nd100_addr × 2)
 * - Default offset: 0x40000 (maps ND-100 space into ND-500 physical RAM)
 *
 * RIOM Access Scope:
 * - RIOM can access ANY ND-100 physical memory (not limited to 5MPM)
 * - Used to read kernel structures, RT program data, and I/O buffers
 * - Access via DMA through 3022/5015 interface hardware
 * - Does NOT interrupt ND-100 program execution
 *
 * Memory Model:
 * In emulator, ND-100 memory is mapped at nd100_memory_offset:
 *   ND-100 Address    Physical Address    Region
 *   0x000000          0x40000             ND-100 RAM start
 *   0x000001          0x40002             Second word
 *   0x040000          0xC0000             5MPM region start
 *   0x05FFFF          0xFFFFE             5MPM region end
 *
 * Reference: E:\Dev\Ronny\NDInsight\SINTRAN\Emulator\ND100Bridge.md
 *            Lines 130-173 (Memory Map), 484-545 (RIOM Implementation)
 *
 * Note: Both ND-100 and ND-500 use BIG-ENDIAN byte order (no swapping needed)
 */
uint16_t nd500_read_nd100_word(Nd500Cpu* cpu, uint32_t nd100_addr) {
	if (!cpu || !cpu->machine) {
		printf("[ERROR] ND-100 Bridge: Invalid CPU/machine pointer\n");
		return 0;
	}

	/* Validate ND-100 address range (22-bit physical: 0x000000-0x3FFFFF)
	 * This is 4M words = 8MB byte addressing */
	if (nd100_addr > 0x3FFFFF) {
		printf("[ERROR] ND-100 Bridge: Address 0x%08X exceeds ND-100 physical range (max 0x3FFFFF)\n",
		       nd100_addr);
		return 0;
	}

	/* Translate ND-100 word address to ND-500 byte address
	 * Formula: physical_addr = base_offset + (word_addr × 2)
	 *
	 * This maps the entire ND-100 address space into ND-500 physical RAM:
	 * - ND-100 Low RAM (0x000000) → Physical 0x40000
	 * - ND-100 5MPM (0x040000) → Physical 0xC0000
	 */
	uint32_t physical_addr = cpu->nd100_memory_offset + (nd100_addr * 2);

	/* Read halfword from physical memory using existing memory access API
	 * Big-endian byte order (same for both ND-100 and ND-500) */
	uint16_t value = nd500_read_memory_16(cpu, physical_addr);

	return value;
}

/**
 * Write a word to ND-100 I/O processor memory space
 *
 * @param cpu        CPU structure containing bridge configuration
 * @param nd100_addr ND-100 physical word address (24-bit, 0x000000-0x3FFFFF)
 * @param data       Halfword value to write to ND-100 memory
 *
 * Address Translation:
 * - ND-100 uses word addressing (address × 2 = byte offset)
 * - ND-500 uses byte addressing (32-bit)
 * - Translation: physical_addr = nd100_memory_offset + (nd100_addr × 2)
 * - Default offset: 0x40000 (maps ND-100 space into ND-500 physical RAM)
 *
 * Write Operation:
 * - Translates ND-100 word address to physical byte address
 * - Writes 16-bit value to physical memory
 * - Big-endian byte order (same for both ND-100 and ND-500)
 *
 * Important Notes:
 * - **NO WIOM INSTRUCTION EXISTS** in ND-500 architecture
 * - Writing from ND-500 to ND-100 is done via 5MPM shared memory only
 * - This function is for emulator internal use (e.g., test fixtures)
 * - In real hardware, ND-500 → ND-100 communication uses 5MPM at 0x80000000
 *
 * Reference: E:\Dev\Ronny\NDInsight\SINTRAN\Emulator\ND100Bridge.md
 *            Line 570: "NO WIOM instruction documented in ND-500 Reference Manual"
 */
void nd500_write_nd100_word(Nd500Cpu* cpu, uint32_t nd100_addr, uint16_t data) {
	if (!cpu || !cpu->machine) {
		printf("[ERROR] ND-100 Bridge: Invalid CPU/machine pointer\n");
		return;
	}

	/* Validate ND-100 address range (22-bit physical: 0x000000-0x3FFFFF) */
	if (nd100_addr > 0x3FFFFF) {
		printf("[ERROR] ND-100 Bridge: Address 0x%08X exceeds ND-100 physical range (max 0x3FFFFF)\n",
		       nd100_addr);
		return;
	}

	/* Translate ND-100 word address to ND-500 byte address */
	uint32_t physical_addr = cpu->nd100_memory_offset + (nd100_addr * 2);

	/* Write halfword to physical memory using existing memory access API
	 * Big-endian byte order (same for both ND-100 and ND-500) */
	nd500_write_memory_16(cpu, physical_addr, data);
}


