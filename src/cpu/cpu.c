#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cpu_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include "nd500_domain.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../disasm/nd500_disasm.h"

/* Global trap state */
Nd500TrapState g_trap_state = {0};

/* Convert trap condition to StopReason enum */
static StopReason trap_to_stop_reason(uint64_t trap_condition) {
	if (trap_condition & TRAP_PGF)  return STOP_TRAP_PAGE_FAULT;
	if (trap_condition & TRAP_PV)   return STOP_TRAP_PROTECTION_VIOLATION;
	if (trap_condition & TRAP_IIC)  return STOP_TRAP_ILLEGAL_INSTRUCTION;
	if (trap_condition & TRAP_IOS)  return STOP_TRAP_ILLEGAL_OPERAND;
	if (trap_condition & TRAP_DZ)   return STOP_TRAP_DIVIDE_BY_ZERO;
	if (trap_condition & TRAP_FO)   return STOP_TRAP_FLOATING_OVERFLOW;
	if (trap_condition & TRAP_FU)   return STOP_TRAP_FLOATING_UNDERFLOW;
	if (trap_condition & TRAP_IVO)  return STOP_TRAP_INVALID_OPERATION;
	if (trap_condition & TRAP_STO)  return STOP_TRAP_STACK_OVERFLOW;
	if (trap_condition & TRAP_STU)  return STOP_TRAP_STACK_UNDERFLOW;
	if (trap_condition & TRAP_IOV)  return STOP_TRAP_INTEGER_OVERFLOW;
	return STOP_TRAP_OTHER;
}

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
	cpu->MTE1 = cpu->MTE2 = 0;
	/* TEMM (Trap Enable Modification Mask): a real domain loads TEMM from its
	 * Domain Information Table (manual line 8133). At cold boot / in a bare
	 * (root) context no mother has restricted us, so every OTE bit is
	 * modifiable. Default to all-ones so SETE/CLTE/OTE:= work until a domain
	 * installs a restrictive mask; a zero default would trap every OTE write. */
	cpu->TEMM1 = cpu->TEMM2 = 0xFFFFFFFFu;
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

bool nd500_cpu_step(Nd500Cpu* cpu) {
	if (!cpu || !cpu->machine) return false;

	/* Check for pending traps before executing instruction */
	if (nd500_trap_occurred()) {
		const Nd500TrapState* trap = nd500_trap_get_state();
		cpu->machine->run_flag = 0;
		cpu->machine->stop_addr = trap ? trap->trap_pc : cpu->PC;
		cpu->machine->stop_data = trap ? trap->trap_data_addr : 0;
		cpu->machine->stop_reason = trap ? trap_to_stop_reason(trap->trap_condition) : STOP_TRAP_OTHER;
		printf("[STOP] %s at PC=0x%08X data=0x%08X\n",
		       nd500_stop_reason_str(cpu->machine->stop_reason),
		       cpu->machine->stop_addr, cpu->machine->stop_data);
		nd500_trap_clear();
		return false;
	}

	/* Check breakpoints before executing instruction.
	 * A resume (step/continue) from a PC that has a breakpoint must
	 * execute that instruction instead of immediately re-breaking. */
	int bp_skip = 0;
	if (cpu->machine->bp_resume_skip) {
		bp_skip = (cpu->machine->bp_resume_pc == cpu->PC);
		cpu->machine->bp_resume_skip = 0;
	}
	if (!bp_skip && cpu->machine->bp_mgr &&
	    bp_should_break_at(cpu->machine->bp_mgr, cpu->PC)) {
		cpu->machine->run_flag = 0;
		cpu->machine->stop_reason = STOP_BREAKPOINT;
		cpu->machine->stop_addr = cpu->PC;
		printf("[STOP] Breakpoint at PC=0x%08X\n", cpu->PC);
		return false;
	}

	/* Trap on invalid instruction 0x00 (uninitialized memory) */
	if (nd500_dbg_get_trap_invalid()) {
		/* Use MMU-aware read for instruction fetch */
		uint8_t opcode_byte;
		if (cpu->machine->mmu_enabled) {
			/* Translate virtual -> physical address */
			uint32_t paddr = nd500_mmu_translate(cpu, cpu->PC, 0, 1); /* is_write=0, is_instruction=1 */
			opcode_byte = nd500_bus_read8(cpu->machine, paddr);
		} else {
			opcode_byte = nd500_bus_read8(cpu->machine, cpu->PC);
		}
		if (opcode_byte == 0x00) {
			cpu->machine->run_flag = 0;
			cpu->machine->stop_reason = STOP_INVALID_INSTRUCTION_00;
			cpu->machine->stop_addr = cpu->PC;
			printf("[STOP] Invalid instruction 0x00 at PC=0x%08X (uninitialized memory)\n", cpu->PC);
			return false;
		}
	}

	/* Decode, execute, then advance PC by decoded length */
	Nd500FetchedInstruction fi;
	uint32_t old_pc = cpu->PC;
	if (nd500_decode_at(cpu->machine, old_pc, &fi) != 0) return false;

	/* Trace instruction execution if enabled - save state before execution */
	static uint32_t saved_regs[10];
	int do_trace = nd500_dbg_get_trace_mode();
	if (do_trace) {
		saved_regs[0] = cpu->PC;
		saved_regs[1] = cpu->I[0]; saved_regs[2] = cpu->I[1];
		saved_regs[3] = cpu->I[2]; saved_regs[4] = cpu->I[3];
		saved_regs[5] = cpu->L;    saved_regs[6] = cpu->B;
		saved_regs[7] = cpu->R;    saved_regs[8] = cpu->ST1;
		saved_regs[9] = cpu->TOS;
		/* Format instruction with operands and output trace */
		char instr_str[256];
		nd500_format_instruction(instr_str, sizeof(instr_str) - 128, &fi);

		/* Build effective address comment for indirect/memory operands */
		size_t pos = strlen(instr_str);
		int has_values = 0;

		/* For CALL/CALLG: show extra operand effective addresses */
		/* Extra operands are the CALL arguments which use indirect addressing */
		/* Note: Use remaining space calculation with overflow protection */
		#define SAFE_SNPRINTF(fmt, ...) do { \
			if (pos < sizeof(instr_str) - 1) { \
				int written = snprintf(instr_str + pos, sizeof(instr_str) - pos, fmt, ##__VA_ARGS__); \
				if (written > 0) pos += (size_t)written; \
				if (pos >= sizeof(instr_str)) pos = sizeof(instr_str) - 1; \
			} \
		} while(0)

		if (cpu->extra_operand_count > 0) {
			for (uint16_t i = 0; i < cpu->extra_operand_count && i < 16; i++) {
				Nd500AddrMode mode = cpu->extra_operands[i].mode;
				/* Include all memory-referencing operands */
				if (mode != ND500_ADDR_CONSTANT && mode != ND500_ADDR_CONSTANT_SHORT &&
				    mode != ND500_ADDR_REGISTER && mode != ND500_ADDR_UNKNOWN) {
					if (!has_values) {
						SAFE_SNPRINTF(" ; ");
						has_values = 1;
					} else {
						SAFE_SNPRINTF(",");
					}
					SAFE_SNPRINTF("0x%X", cpu->extra_operands[i].effective_address);
				}
			}
		} else {
			/* For non-CALL instructions: check regular operands for indirect addressing */
			for (int i = 0; i < fi.operand_count && i < 8; i++) {
				Nd500AddrMode mode = fi.operands[i].mode;
				if (mode == ND500_ADDR_LOCAL_IND || mode == ND500_ADDR_LOCAL_IND_PI) {
					if (!has_values) {
						SAFE_SNPRINTF(" ; ");
						has_values = 1;
					} else {
						SAFE_SNPRINTF(",");
					}
					SAFE_SNPRINTF("0x%X", fi.operands[i].effective_address);
				}
			}
		}
		#undef SAFE_SNPRINTF

		int instr_len = fi.total_len ? fi.total_len : fi.opcode_len;
		nd500_dbg_trace_before(old_pc, instr_str, fi.bytes, instr_len, saved_regs);
	}

	/* Profile instruction execution if enabled */
	if (nd500_dbg_get_profiling()) {
		nd500_dbg_profile_instruction(fi.mnemonic);
	}

	/* Advance PC BEFORE execution (like C# implementation)
	 * Branch/jump instructions will overwrite PC as needed */
	cpu->PC = old_pc + (fi.total_len ? fi.total_len : fi.opcode_len);

	nd500_execute_decoded(cpu, &fi);

	/* -------------------------------------------------------------------
	 * WORKAROUND (opt-in via ND500X_NC_TYPETAG_GUARD): NC type-confusion guard
	 *
	 * Compiling B.C, NC's list-builder helper at 0x08024829 computes a list
	 * size as  count = mem16[recordA] + mem16[recordB]  via:
	 *   0x08024834  h1 := IND(b.20)   ; h1 = mem16[recordA]
	 *   0x08024837  h2 := IND(b.24)   ; h2 = mem16[recordB]
	 *   0x0802483A  w1 + W2           ; count = h1 + h2
	 * At instr ~1,196,914 recordA (0x1802A1B0) is a LIVE type-3 object
	 * (tag byte 0x03 at offset 0), so mem16[recordA] = 0x0300 (the tag) is
	 * misread as a count -> 0x0300 + 2 = 770 -> an over-sized list whose walk
	 * later dereferences a wild pointer -> PV at 0x080241FC.
	 *
	 * The type-confusion is upstream and NOT resolvable by tracing without a
	 * hardware/manual oracle (see docs/NC_CRASH_0x08023EA4_ROOTCAUSE.md
	 * UPDATE 39-43). This guard does not FIX it - it validates the record's
	 * type tag at the count read and, when the source is a type-3 object,
	 * substitutes a 0 count contribution so execution proceeds past this
	 * crash and exposes the NEXT blocker. Disabled unless the env var is set.
	 * ------------------------------------------------------------------- */
	if (old_pc == 0x08024834u) {
		static int guard_mode = -1; /* -1 unread, 0 off, 1 on */
		if (guard_mode < 0) {
			const char* e = getenv("ND500X_NC_TYPETAG_GUARD");
			guard_mode = (e && e[0] && e[0] != '0') ? 1 : 0;
		}
		/* h1 (cpu->I[0]) now holds mem16[recordA], big-endian, so its high
		 * byte is the record's tag byte at offset 0. A type-3 object has
		 * tag 0x03; a genuine count-record here holds a tiny value (1,2).
		 * Only the high byte is inspected - no memory access, so no host
		 * fault risk on non-type-3 invocations. */
		if (guard_mode && ((cpu->I[0] >> 8) & 0xFF) == 0x03) {
			uint32_t rec_ptr = (fi.operand_count >= 1)
			                   ? fi.operands[0].effective_address : 0;
			printf("[NC-GUARD] type-3 tag (mem16=0x%04X) at record 0x%08X read as "
			       "list count at PC=0x%08X instr=%llu; substituting count 0\n",
			       (unsigned)(cpu->I[0] & 0xFFFF), rec_ptr, old_pc,
			       (unsigned long long)cpu->instruction_count);
			fflush(stdout);
			cpu->I[0] = 0;
		}
	}

	/* Trace register changes after execution */
	if (do_trace) {
		uint32_t after_regs[10] = {cpu->PC, cpu->I[0], cpu->I[1], cpu->I[2], cpu->I[3],
		                           cpu->L, cpu->B, cpu->R, cpu->ST1, cpu->TOS};
		nd500_dbg_trace_after(saved_regs, after_regs);
	}

	/* Increment instruction counter (used by MON 11B TIME) */
	cpu->instruction_count++;

	/* Check for pending ignorable traps at end of instruction.
	 * Pass old_pc (the faulting/just-executed instruction's address, before
	 * PC was advanced) so RETT retries the correct instruction - not the
	 * already-advanced cpu->PC. */
	check_pending_traps(cpu, old_pc);

	/* Check if a non-ignorable trap occurred during execution */
	if (nd500_trap_occurred()) {
		const Nd500TrapState* trap = nd500_trap_get_state();
		if (trap && (trap->trap_condition & TRAP_INTERRUPT_MASK)) {
			cpu->machine->run_flag = 0;
			cpu->machine->stop_reason = trap_to_stop_reason(trap->trap_condition);
			cpu->machine->stop_addr = trap->trap_pc;
			cpu->machine->stop_data = trap->trap_data_addr;
			printf("[STOP] %s at PC=0x%08X data=0x%08X\n",
			       nd500_stop_reason_str(cpu->machine->stop_reason),
			       trap->trap_pc, trap->trap_data_addr);
			return false;
		}
	}

	/* Check register watchpoints. Guarded by wp_count so the cost is a
	 * single compare per instruction when no watchpoints exist. */
	if (cpu->machine->bp_mgr && cpu->machine->bp_mgr->wp_count > 0) {
		uint32_t wp_regs[WP_REG_INDEX_COUNT] = {
			cpu->PC, cpu->I[0], cpu->I[1], cpu->I[2], cpu->I[3],
			cpu->L, cpu->B, cpu->R
		};
		int hit = wp_check_registers(cpu->machine->bp_mgr, wp_regs);
		if (hit >= 0) {
			cpu->machine->run_flag = 0;
			cpu->machine->stop_reason = STOP_WATCHPOINT_REGISTER;
			cpu->machine->stop_addr = cpu->PC;
			cpu->machine->stop_data =
				cpu->machine->bp_mgr->watchpoints[hit].last_value;
			printf("[STOP] Register watchpoint (%s) at PC=0x%08X\n",
			       cpu->machine->bp_mgr->watchpoints[hit].register_name, cpu->PC);
			return false;
		}
	}

	return true;
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
 * @param trapPC    PC where trap occurred (the trapping instruction, NOT the next one)
 * @param dataAddr  Related memory address (if applicable)
 *
 * For non-ignorable/fatal traps: Sets status bit and stops execution
 * For ignorable traps:
 *   - If enabled in OTE and handler exists: invoke handler immediately
 *   - If enabled in OTE but no handler: stop execution
 *   - If not enabled in OTE: just set status bit, continue execution
 */
void raise_trap(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trapPC, uint32_t dataAddr) {
	if (!cpu) return;

	/* DEBUG: Print when trap is raised with more context */
	fprintf(stderr, "[DEBUG] raise_trap: trapBit=0x%llX trapPC=0x%08X dataAddr=0x%08X INTERRUPT=%d instr_count=%llu\n",
	        (unsigned long long)trapBit, trapPC, dataAddr,
	        (trapBit & TRAP_INTERRUPT_MASK) ? 1 : 0,
	        (unsigned long long)cpu->instruction_count);

	/* Set the corresponding bit in ST1/ST2 status registers */
	if (trapBit & 0xFFFFFFFF) {
		cpu->ST1 |= (uint32_t)(trapBit & 0xFFFFFFFF);
	}
	if (trapBit >> 32) {
		cpu->ST2 |= (uint32_t)(trapBit >> 32);
	}

	/* Check if this is a non-ignorable/fatal trap (bits 32+) */
	if (trapBit & TRAP_INTERRUPT_MASK) {
		/* Non-ignorable traps (bits 32-41: PV, ISE, THM, PGF, ...) are still delivered to the
		 * PROGRAM via its THA vector on the ND-500 - a program installs handlers precisely to
		 * receive them (NC sets THA[36]=PV handler 0x0802D817, and handlers for 32-41). The
		 * machine only halts when there is NO handler (THA==0 or slot==0 -> Trap Handler Missing)
		 * or when a fault occurs while ALREADY inside a handler (a double fault - our single-level
		 * saved-trap state cannot nest, and a nested non-ignorable trap is a genuine hard error).
		 * Opt out with ND500X_NO_TRAP_DISPATCH=1 to restore the old always-halt behavior. */
		static int td = -1;
		if (td < 0) { const char* e = getenv("ND500X_NO_TRAP_DISPATCH"); td = (e && e[0] && e[0] != '0') ? 0 : 1; }
		if (td && cpu->THA != 0 && !cpu->in_trap_handler) {
			int tn = 0; for (int i = 0; i < 64; i++) { if ((trapBit >> i) & 1) { tn = i; break; } }
			uint32_t hp = nd500_mmu_translate(cpu, cpu->THA + tn * 4, 0, 0);
			uint32_t haddr = nd500_trap_occurred() ? 0 : nd500_bus_read32(cpu->machine, hp);
			if (haddr != 0) {
				nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
				invoke_trap_handler(cpu, trapBit, trapPC);
				return;
			}
			/* no handler installed for this trap -> fall through to halt (Trap Handler Missing) */
		}
		/* Set trap state - this WILL stop execution */
		nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
		TRACE("[TRAP] %s at PC=0x%08X data=0x%08X\n",
		      nd500_stop_reason_str(trap_to_stop_reason(trapBit)), trapPC, dataAddr);
		if (cpu->machine) {
			cpu->machine->run_flag = 0;
			if (cpu->machine->stop_reason == STOP_NONE) {
				cpu->machine->stop_reason = trap_to_stop_reason(trapBit);
				cpu->machine->stop_addr = trapPC;
				cpu->machine->stop_data = dataAddr;
			}
		}
		return;
	}

	/* Ignorable trap (bits 11-29): check if enabled in OTE mask */
	uint64_t ote = ((uint64_t)cpu->OTE2 << 32) | cpu->OTE1;
	if (trapBit & ote & TRAP_IGNORABLE_MASK) {
		/* Trap is enabled - set trap state and invoke handler */
		nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
		/* Pass trapPC (the trapping instruction's address) so RETT can retry it */
		invoke_trap_handler(cpu, trapBit, trapPC);
		/* If invoke_trap_handler succeeded, execution continues in handler */
		/* If no handler was found, it will have stopped execution */
	}
	/* If trap is not enabled in OTE, just continue (status bit is set, no stop) */
}

/**
 * Check for pending ignorable traps at end of instruction
 * Called after each instruction completes
 *
 * trappingPC is the address of the instruction that just completed
 * (the PC BEFORE it was advanced by fetch/decode), not the current
 * (already-advanced) cpu->PC. This is the address RETT must retry,
 * matching the C# reference (CpuND500.Execute.cs: CheckPendingTraps(instructionPC)).
 */
void check_pending_traps(Nd500Cpu* cpu, uint32_t trappingPC) {
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
				invoke_trap_handler(cpu, trapBit, trappingPC);
				break;  /* Only handle one trap at a time */
			}
		}
	}
}

/**
 * Invoke trap handler for a specific trap
 *
 * This function implements the ND-500 trap dispatch mechanism:
 * 1. Calculate trap number from bit position
 * 2. Read handler address from THA vector
 * 3. Verify ENTT instruction at handler address
 * 4. Save state for RETT to restore
 * 5. Clear OTE to prevent recursive traps
 * 6. Jump to handler by setting PC
 *
 * The trap handler must start with ENTT and end with RETT.
 * RETT will return to the trapping instruction (trappingP) to retry it.
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

	/* Read handler address from DATA space (use MMU translation) */
	uint32_t paddr_tha = nd500_mmu_translate(cpu, handlerPointer, 0, 0); /* read, data */
	uint32_t handlerAddr = nd500_bus_read32(cpu->machine, paddr_tha);

	if (handlerAddr == 0) {
		printf("[TRAP] No trap handler at THA[%d] (THA=0x%08X, ptr=0x%08X)\n",
		       trapNumber, cpu->THA, handlerPointer);
		/* Clear trap bit since we can't handle it */
		if (trapBit & 0xFFFFFFFF)
			cpu->ST1 &= ~(uint32_t)(trapBit & 0xFFFFFFFF);
		if (trapBit >> 32)
			cpu->ST2 &= ~(uint32_t)(trapBit >> 32);
		return;
	}

	/* Verify ENTT instruction at handler address (PROG space) */
	/* ENTT opcode is 0xBC (single-byte opcode in the 0x00BC table entry) */
	uint32_t paddr_handler = nd500_mmu_translate(cpu, handlerAddr, 0, 1); /* read, instruction */
	uint8_t byte0 = nd500_bus_read8(cpu->machine, paddr_handler);
	if (byte0 != 0xBC) {
		printf("[TRAP] No ENTT instruction at trap handler 0x%08X (found 0x%02X, expected 0xBC)\n",
		       handlerAddr, byte0);
		/* Clear trap bit since handler is invalid */
		if (trapBit & 0xFFFFFFFF)
			cpu->ST1 &= ~(uint32_t)(trapBit & 0xFFFFFFFF);
		if (trapBit >> 32)
			cpu->ST2 &= ~(uint32_t)(trapBit >> 32);
		return;
	}

	/* Save state for RETT to restore */
	cpu->trap_saved_PC = trappingP;      /* Return to trapping instruction */
	cpu->trap_saved_OTE1 = cpu->OTE1;    /* Save trap enable state */
	cpu->trap_saved_OTE2 = cpu->OTE2;
	cpu->trap_number = trapNumber;
	cpu->in_trap_handler = true;

	/* Clear OTE to prevent recursive traps during handler execution */
	cpu->OTE1 = 0;
	cpu->OTE2 = 0;

	/* Jump to handler - set PC to handler address */
	cpu->PC = handlerAddr;

	/* Clear global trap state since we're handling it */
	nd500_trap_clear();
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

    /* Set trap_name based on condition for test validation */
    /* Names match C# TrapType enum values */
    const char* name = "Unknown";
    if (condition & TRAP_DZ) name = "DivisionByZero";
    else if (condition & TRAP_IOV) name = "IllegalOperandValue";
    else if (condition & TRAP_IOS) name = "IllegalOperandValue";
    else if (condition & TRAP_IIC) name = "IllegalInstruction";
    else if (condition & TRAP_PV) name = "PrivilegeViolation";
    else if (condition & TRAP_STO) name = "StackOverflow";
    else if (condition & TRAP_STU) name = "StackUnderflow";
    else if (condition & TRAP_ISE) name = "InstructionSequenceError";
    else if (condition & (TRAP_IVO | TRAP_FU | TRAP_FO)) name = "FloatException";
    else if (condition & TRAP_BO) name = "Overflow";  /* BCD overflow */
    else if (condition & (TRAP_ATF | TRAP_ATR | TRAP_ATW | TRAP_AZ |
                          TRAP_DR | TRAP_IX | TRAP_PGF)) {
        name = "AddressingError";
    }

    strncpy(g_trap_state.trap_name, name, sizeof(g_trap_state.trap_name) - 1);
    g_trap_state.trap_name[sizeof(g_trap_state.trap_name) - 1] = '\0';

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

	/* Main execution loop */
	while (steps != 0 && cpu->machine->run_flag) {
		/* Execute one instruction - returns false if trap occurred */
		if (!nd500_cpu_step(cpu)) {
			/* Trap occurred - stop execution */
			break;
		}

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


