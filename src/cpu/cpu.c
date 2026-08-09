#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cpu_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include "nd500_domain.h"
#include "nd500_fecall.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../disasm/nd500_disasm.h"
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

/* Global trap state */
Nd500TrapState g_trap_state = {0};

/* Stop-diagnostics ring buffer: the last N instruction PCs, so any run-ending
 * path (trap, breakpoint, invalid opcode, decode failure) can print how
 * execution got there. Populated at the top of every nd500_cpu_step; dumped to
 * stderr when ND500X_STOPDBG is set. Cheap and off the hot path unless enabled. */
#define ND500_PC_RING_LEN 256
static uint32_t g_pc_ring[ND500_PC_RING_LEN];
/* Domain/handler context of each recorded PC. Without it the ring cannot tell a
 * user-domain PC from a kernel one, and the two look identical in the dump -
 * which is exactly the ambiguity that stalled the ENTS/ISE hunt. */
static uint8_t  g_pc_ring_ced[ND500_PC_RING_LEN];
static uint8_t  g_pc_ring_cad[ND500_PC_RING_LEN];
static uint8_t  g_pc_ring_inh[ND500_PC_RING_LEN];
static uint32_t g_pc_ring_pos = 0;
static void nd500_dump_stop_ring(const char* tag) {
	if (!nd500_settings()->stopdbg) return;
	fprintf(stderr, "[STOPDBG] %s - last %d instruction PCs (oldest -> newest):\n",
	        tag, ND500_PC_RING_LEN);
	for (uint32_t k = 0; k < ND500_PC_RING_LEN; k++) {
		uint32_t i = (g_pc_ring_pos + k) % ND500_PC_RING_LEN;
		fprintf(stderr, "  %2u: 0x%08X  CED=%u CAD=%u inH=%u\n", k, g_pc_ring[i],
		        g_pc_ring_ced[i], g_pc_ring_cad[i], g_pc_ring_inh[i]);
	}
}

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
	/* DT and DE have no dedicated StopReason - they are reported through the
	 * generic one, which still names the trap in the log line. Adding enum
	 * values would ripple through every consumer of StopReason for no gain. */
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
	cpu->trap_dispatch_pending = 0;       /* dispatch->ENTT interlock, see cpu_protos.h */

	/* Initialize MMU registers */
	cpu->PSTP = cpu->DITBASE = cpu->CED = cpu->CAD = cpu->PS = 0;

	/* Clear CALL/ENT handshake state */
	cpu->pending_call_return_address = 0;
	cpu->pending_call_arg_count = 0;
	memset(cpu->pending_call_arg_addresses, 0, sizeof(cpu->pending_call_arg_addresses));
	/* ...and the trap-nesting save stack for it (cpu_protos.h trap_seq). */
	cpu->trap_seq_head = 0;
	cpu->trap_seq_count = 0;
	memset(cpu->trap_seq, 0, sizeof(cpu->trap_seq));

	/* Clear variable operand buffer */
	cpu->extra_operand_count = 0;
	memset(cpu->extra_operands, 0, sizeof(cpu->extra_operands));

	/* Cycle counter and the SOLO-region start marker.
	 *
	 * These were the only pieces of CPU state a reset left running, and the
	 * pair is dangerous together: check_solo_timeout() raises DT when
	 * (instruction_count - solo_start_icount) exceeds SOLO_MAX_CYCLES, so a
	 * stale non-zero count against a zero marker looks like a SOLO region that
	 * has already overrun - even though no SOLO was ever executed.
	 *
	 * It bit the conformance harness, which resets the CPU between
	 * tests but shares one process. Noop_Default loads ST1 = 0x12345678, and
	 * that word happens to set PSD (bit 4) with PIA clear - an UNPRIVILEGED
	 * SOLO region. On its own the test passed; after 257 earlier tests had
	 * run, instruction_count was past 256 and the very first step raised DT.
	 * DT has no entry in nd500_trap_set_state's name table, so it surfaced as
	 * the useless "Trap: Unknown ()". The 256-cycle limit is exactly why the
	 * failure appeared at 257 preceding tests and not at 256.
	 *
	 * Zeroing both is also just what a reset should do: a real CPU coming out
	 * of reset is not mid-SOLO with a cycle count carried over. */
	cpu->instruction_count = 0;
	cpu->solo_start_icount = 0;

	/* Clear any pending traps */
	nd500_trap_clear();
}



bool nd500_cpu_step(Nd500Cpu* cpu) {
	if (!cpu || !cpu->machine) return false;

	/* Name the instruction about to be fetched BEFORE anything can fault -
	 * including the invalid-00 heuristic's own translate below. A page fault
	 * raised while fetching an instruction must restart at ITS START, and for
	 * an instruction straddling a page boundary the fault address is the NEXT
	 * page, not the instruction. Setting this later left it naming the
	 * PREVIOUS instruction during those faults. */
	cpu->cur_instr_pc = cpu->PC;

	/* Clear the previous instruction's abort flag at the TOP of the step -
	 * BEFORE fetch/decode. The guarded mmu_read/write helpers refuse access
	 * while instr_aborted is set; clearing it only just before execute (as
	 * before) made the fetch of the instruction FOLLOWING an aborted one read
	 * all-zero bytes -> bogus illegal-instruction stop. */
	cpu->instr_aborted = 0;

	/* Post-trap step trace (ND500X_PTDBG=<hexPC>): once a trap is raised at
	 * that PC, log PC/CED/B for the following instructions - shows exactly
	 * where the kernel handler goes and where it resumes. */
	{
		extern unsigned long g_ptdbg_target, g_ptdbg_count;
		if (g_ptdbg_count) {
			g_ptdbg_count--;
			fprintf(stderr, "[PTDBG] PC=0x%08X CED=%u CAD=%u B=0x%08X inTrap=%d\n",
			        cpu->PC, cpu->CED, cpu->CAD, cpu->B, cpu->in_trap_handler);
		}
	}

	/* Record this PC in the stop-diagnostics ring before any stop check below. */
	g_pc_ring[g_pc_ring_pos] = cpu->PC;
	g_pc_ring_ced[g_pc_ring_pos] = (uint8_t)cpu->CED;
	g_pc_ring_cad[g_pc_ring_pos] = (uint8_t)cpu->CAD;
	g_pc_ring_inh[g_pc_ring_pos] = (uint8_t)(cpu->in_trap_handler ? 1 : 0);
	g_pc_ring_pos = (g_pc_ring_pos + 1u) % ND500_PC_RING_LEN;


	/* SLPDBG: env-gated probe at the panic("sleep") call site in _sleep
	 * (kern_synch.c:121). PC 0x11F75 is the `call _panic` for "sleep"; at that
	 * point u.u_procp and its fields still hold the values that failed the guard
	 * `chan==0 || rp->p_stat!=SRUN || rp->p_rlink`. Dump the real bytes so we can
	 * tell WHICH of the three conditions fired, without guessing frame layout. */
	{
		static int slpdbg = -1;
		if (slpdbg < 0) slpdbg = nd500_settings()->slpdbg;
		if (slpdbg && cpu->PC == 0x00011F75u) {
			uint32_t pu = nd500_mmu_peek(cpu, 0xE8000008u); /* &u.u_procp (seg 29) */
			uint32_t procp = (pu != 0xFFFFFFFFu) ? nd500_bus_read32(cpu->machine, pu) : 0xDEADBEEF;
			fprintf(stderr, "[SLPDBG] panic(sleep) site: B=0x%08X L=0x%08X R=0x%08X u.u_procp=0x%08X\n",
			        cpu->B, cpu->L, cpu->R, procp);
			/* chan candidate at B+20 (b.20 tested in guard); read via data MMU. */
			uint32_t pch = nd500_mmu_peek(cpu, cpu->B + 20u);
			uint32_t chan = (pch != 0xFFFFFFFFu) ? nd500_bus_read32(cpu->machine, pch) : 0xDEADBEEF;
			fprintf(stderr, "[SLPDBG]   chan(b.20)=0x%08X\n", chan);
			if (procp != 0 && procp != 0xDEADBEEF) {
				uint32_t pp = nd500_mmu_peek(cpu, procp);          /* p_link @0 */
				uint32_t prl = nd500_mmu_peek(cpu, procp + 4u);    /* p_rlink @4 */
				uint32_t pst = nd500_mmu_peek(cpu, procp + 31u);   /* p_stat  @31 */
				uint32_t v_link  = (pp  != 0xFFFFFFFFu) ? nd500_bus_read32(cpu->machine, pp) : 0xDEADBEEF;
				uint32_t v_rlink = (prl != 0xFFFFFFFFu) ? nd500_bus_read32(cpu->machine, prl) : 0xDEADBEEF;
				uint8_t  v_stat  = (pst != 0xFFFFFFFFu) ? nd500_bus_read8(cpu->machine, pst) : 0xFF;
				fprintf(stderr, "[SLPDBG]   proc: p_link=0x%08X p_rlink=0x%08X p_stat=%u (SRUN=3)\n",
				        v_link, v_rlink, v_stat);
			}
		}
	}

	/* PAGEINDBG: dump pagein(space,vaddr,...) args at _pagein entry (0x2FD24).
	 * The kernel reads the faulting address from cx_vaddr (ENTT-built trap frame);
	 * memory's ENTT fault-address fix must deliver the REAL vaddr (0x08000014 /
	 * 0xF0000000), not the bogus 0x32 (=arg-count N). Args are on the stack at B+. */
	{
		static int pidbg = -1;
		if (pidbg < 0) pidbg = nd500_settings()->pageindbg;
		if (pidbg && cpu->PC == 0x0002FD24u) {
			for (uint32_t off = 0x14; off <= 0x28; off += 4) {
				uint32_t pa = nd500_mmu_peek(cpu, cpu->B + off);
				uint32_t v = (pa != 0xFFFFFFFFu) ? nd500_bus_read32(cpu->machine, pa) : 0xDEADBEEF;
				fprintf(stderr, "[PAGEINDBG] B+0x%02X = 0x%08X  (B=0x%08X CED=%u CAD=%u)\n", off, v, cpu->B, cpu->CED, cpu->CAD);
			}
		}
	}

	/* Check for pending traps before executing instruction */
	if (nd500_trap_occurred()) {
		const Nd500TrapState* trap = nd500_trap_get_state();
		cpu->machine->run_flag = 0;
		cpu->machine->stop_addr = trap ? trap->trap_pc : cpu->PC;
		cpu->machine->stop_data = trap ? trap->trap_data_addr : 0;
		cpu->machine->stop_reason = trap ? trap_to_stop_reason(trap->trap_condition) : STOP_TRAP_OTHER;
		/* P1 = the TRAPPING P: the instruction that actually failed. PC/stop_addr is
		 * the RESTART P and normally runs AHEAD of it, so this line printed alone
		 * sends a reader to the wrong instruction. ND-05.017.01 ch.6 STEP 2 has the
		 * engineer read BOTH registers for exactly that reason. Disassemble P1. */
		printf("[STOP] %s at PC=0x%08X data=0x%08X P1=0x%08X <- failing instruction\n",
		       nd500_stop_reason_str(cpu->machine->stop_reason),
		       cpu->machine->stop_addr, cpu->machine->stop_data, cpu->P1);
		nd500_dump_stop_ring("trap");
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
		uint32_t paddr = cpu->PC;
		uint32_t fetch_pc = cpu->PC;
		if (cpu->machine->mmu_enabled) {
			/* Translate virtual -> physical address */
			paddr = nd500_mmu_translate(cpu, cpu->PC, 0, 1); /* is_write=0, is_instruction=1 */
			if (cpu->PC != fetch_pc) {
				/* The translate FAULTED and the trap already vectored
				 * (PC now points at the handler; on a demand fetch fault
				 * this is the normal pagein path). The byte we would
				 * read is meaningless - skip the invalid-00 heuristic
				 * and let the next step fetch the handler. */
				goto invalid00_done;
			}
			opcode_byte = nd500_bus_read8(cpu->machine, paddr);
			if (nd500_settings()->icodedbg && cpu->CED != 0) {
				uint32_t cap_addr = cpu->DITBASE + (uint32_t)cpu->CED * 256u + 0u
				                  + ((uint32_t)((cpu->PC >> SGSHIFT) & 0x1F)) * 2u;
				uint16_t gcap = (uint16_t)(((uint32_t)nd500_bus_read8(cpu->machine, cap_addr) << 8)
				                         |  (uint32_t)nd500_bus_read8(cpu->machine, cap_addr + 1));
				printf("[ICODEDBG] fetch PC=0x%08X CED=%u seg=%u paddr=0x%08X byte=0x%02X guest_pcap=0x%04X\n",
				       cpu->PC, cpu->CED, (cpu->PC >> SGSHIFT) & 0x1F, paddr, opcode_byte, gcap);
			}
		} else {
			opcode_byte = nd500_bus_read8(cpu->machine, cpu->PC);
		}
		if (opcode_byte == 0x00) {
			/* ND500X_NO_INVALID00=1: report but keep running. The heuristic
			 * assumes a zero opcode means uninitialized memory, but a
			 * demand-paged text page can legitimately read as zero for a
			 * moment while its disk read is in flight - halting there hides
			 * whether the page arrives. */
			static int nz = -1;
			if (nz < 0) nz = nd500_settings()->no_invalid00;
			if (nz) {
				static unsigned zc = 0;
				if (zc++ < 40)
					fprintf(stderr, "[ZEROFETCH] PC=0x%08X CED=%u paddr=0x%08X (continuing)\n",
					        cpu->PC, cpu->CED, paddr);
				goto invalid00_done;
			}
			cpu->machine->run_flag = 0;
			cpu->machine->stop_reason = STOP_INVALID_INSTRUCTION_00;
			cpu->machine->stop_addr = cpu->PC;
			{
				uint32_t pa = cpu->machine->mmu_enabled ? nd500_mmu_translate(cpu, cpu->PC, 0, 1) : cpu->PC;
				printf("[STOP] Invalid instruction 0x00 at PC=0x%08X (uninitialized memory) CED=%u CAD=%u B=0x%08X paddr=0x%08X "
				       "fetch_paddr=0x%08X byte_at_repaddr=0x%02X in_trap=%d\n",
				       cpu->PC, cpu->CED, cpu->CAD, cpu->B, pa,
				       paddr, nd500_bus_read8(cpu->machine, pa),
				       (int)cpu->in_trap_handler);
			}
			nd500_dump_stop_ring("invalid-00");
			return false;
		}
invalid00_done: ;
	}

	/* ND-100 front-end interrupt tick: at the kernel base level, deliver a
	 * pending disk/dctl completion or a periodic clock tick (vectoring PC to
	 * _intvec). The kernel's diintr()->iodone() wakes biowait() sleepers and
	 * hardclock() drives the scheduler. Has its own fast path (no-op most
	 * instructions). */
	{
		extern void nd500_fecall_tick(Nd500Cpu* cpu);
		nd500_fecall_tick(cpu);
	}
	if (nd500_settings()->piadbg) {
		static int prev_pia = -1;
		static uint32_t prev_pc = 0;
		int pia = (cpu->ST1 >> 1) & 1;   /* PIA = ST1 bit 1 */
		if (prev_pia == 1 && pia == 0)
			fprintf(stderr, "[PIADBG] PIA 1->0 cleared BY instruction @PC=0x%08X (now PC=0x%08X ST1=0x%08X CED=%u CAD=%u)\n",
			        prev_pc, cpu->PC, cpu->ST1, cpu->CED, cpu->CAD);
		prev_pia = pia; prev_pc = cpu->PC;
	}
	if (nd500_settings()->pcsample) {
		static uint64_t pcn = 0;
		if ((pcn++ % 500000) == 0) {
			uint32_t iplp = nd500_bus_read32(cpu->machine, 0x1cb20u); /* *_iplp = iplrec ptr */
			uint32_t ipw  = nd500_bus_read32(cpu->machine, iplp + 4u); /* ip_current(short@4)|ip_mask(short@6) */
			uint32_t ipnx = nd500_bus_read32(cpu->machine, iplp + 0u); /* ip_next */
			uint32_t lock = nd500_bus_read32(cpu->machine, iplp + 8u); /* ip_lock */
			fprintf(stderr, "[PCSAMPLE] PC=0x%08X CED=0x%X iplrec=0x%08X ip_curr=0x%04X ip_mask=0x%04X ip_next=0x%08X lock=0x%08X fe_pend=%d gen=%d (n=%llu)\n",
				cpu->PC, cpu->CED, iplp, (ipw >> 16) & 0xFFFF, ipw & 0xFFFF, ipnx, lock,
				(int)cpu->fe_int_count,
				(int)(cpu->fe_int_count ? cpu->fe_int_q[cpu->fe_int_head].gen : 0),
				(unsigned long long)pcn);
		}
	}

	/* A clean shutdown requested earlier fires HERE - before the PC is latched,
	 * so that when it redirects us to _boot this very step decodes and executes
	 * _boot with the CALL/ENTS interlock it just set up.
	 *
	 * Doing this AFTER the decode does not work: the instruction at the safe
	 * point still executes, and at the syscall dispatcher that instruction is
	 * an ENTS, which clears the interlock again ("[TRAP] ENTS: Must be preceded
	 * by CALL/CALLG", then a kernel stack underflow panic). */
	if (nd500_ndix_halt_pending())
		nd500_ndix_halt_check(cpu, cpu->PC);

	/* Decode, execute, then advance PC by decoded length */
	Nd500FetchedInstruction fi;
	uint32_t old_pc = cpu->PC;
	if (nd500_decode_at(cpu->machine, old_pc, &fi) != 0) {
		/* Instruction decode failed. This was previously a SILENT run-ending
		 * path (return false with no message) - the reason `run` appeared to
		 * "just stop" with no [STOP] line. Report it loudly, and on ND500X_STOPDBG
		 * dump the last PCs that led here so the real stopping point is visible
		 * (works around the debug prompt's buffered/reset register view). */
		cpu->machine->run_flag = 0;
		cpu->machine->stop_reason = STOP_TRAP_OTHER;
		cpu->machine->stop_addr = old_pc;
		cpu->machine->stop_data = 0;
		printf("[STOP] decode failed at PC=0x%08X (bad opcode or unmapped instruction fetch)\n", old_pc);
		nd500_dump_stop_ring("decode-failed");
		return false;
	}
	if (nd500_settings()->d1dbg && cpu->CED == 1 && old_pc <= 0x40) {
		static uint64_t d1n = 0;
		if (d1n++ < 8) {
			uint32_t pa = nd500_mmu_translate(cpu, old_pc, 0, 1);
			uint8_t b[8]; for (int i=0;i<8;i++) b[i]=nd500_bus_read8(cpu->machine, pa+i);
			fprintf(stderr, "[D1DBG] CED=1 PC=0x%08X pa=0x%08X op=0x%04X '%s' bytes=%02x %02x %02x %02x %02x %02x %02x %02x len=%u nops=%u B=0x%08X L=0x%08X R=0x%08X\n",
			        old_pc, pa, fi.opcode, fi.mnemonic ? fi.mnemonic : "?",
			        b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
			        fi.total_len, fi.operand_count, cpu->B, cpu->L, cpu->R);
		}
	}

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

	/* Bounded trap-handler control-flow trace (env ND500X_HDLRTRACE): prints the
	 * PC sequence executed while inside a trap handler, to see where the NDIX
	 * kernel PGF stub branches (the PC=0x93 mid-instruction blocker). */
	if (nd500_settings()->hdlrtrace && cpu->in_trap_handler) {
		/* Log only taken control transfers: whenever this PC is not the sequential
		 * successor of the previous instruction, print from->to with the SOURCE
		 * instruction bytes. Directly exposes the branch that reaches PC=0x04. */
		static uint32_t exp_next = 0xFFFFFFFFu, prev_pc = 0, prev_len = 0;
		if (old_pc != exp_next && exp_next != 0xFFFFFFFFu) {
			uint32_t ppa = nd500_mmu_translate(cpu, prev_pc, 0, 1);
			printf("[HBR] 0x%08X (len %u, bytes %02X %02X %02X %02X %02X %02X) --> 0x%08X  PiA=%u ST1=0x%08X B=0x%08X L=0x%08X\n",
			       prev_pc, prev_len,
			       nd500_bus_read8(cpu->machine, ppa), nd500_bus_read8(cpu->machine, ppa+1),
			       nd500_bus_read8(cpu->machine, ppa+2), nd500_bus_read8(cpu->machine, ppa+3),
			       nd500_bus_read8(cpu->machine, ppa+4), nd500_bus_read8(cpu->machine, ppa+5),
			       old_pc, (unsigned)((cpu->ST1 >> ND500_ST_BIT_PIA) & 1u), cpu->ST1, cpu->B, cpu->L);
		}
		if (old_pc == 0x165u) {
			uint32_t base = cpu->I[2];  /* W3 = I3 = lregbl base operand */
			printf("[LREGBL] base(I3)=0x%08X  THA+256=0x%08X\n", base, cpu->THA + 256u);
			for (int rn = 0; rn <= 6; rn++) {
				uint32_t va = base + (uint32_t)rn * 4u;
				printf("[LREGBL]   [base+%d]=0x%08X\n", rn * 4,
				       nd500_read_memory_32(cpu, va));
			}
		}
		prev_pc = old_pc;
		prev_len = (fi.total_len ? fi.total_len : fi.opcode_len);
		exp_next = old_pc + prev_len;
	}

	/* Record the faulting-instruction PC for restartable traps (PGF): the
	 * PC advance below moves cpu->PC past this instruction before execute,
	 * but a page fault must restart THIS instruction (see raise_trap). */
	cpu->cur_instr_pc = old_pc;
	/* (instr_aborted is cleared at the top of the step, before fetch/decode) */

	/* A fault raised during FETCH/DECODE has already vectored cpu->PC at the
	 * trap handler. Advancing the PC below (and executing the half-decoded
	 * instruction) would DESTROY that vector. This bites whenever an
	 * instruction straddles a page boundary and only its tail bytes are
	 * missing: the decode completes from garbage, PC becomes old_pc+len, the
	 * handler never runs, and the program silently continues in the middle of
	 * a page it does not have - the ls -l failure (instruction at 0x0FFC
	 * spanning into the unmapped page at 0x1000 resumed at 0x1003, in kernel
	 * text, with the user's B). Faults at the FIRST byte were unaffected,
	 * which is why most demand paging worked. */
	if (cpu->instr_aborted) {
		return true;
	}

	/* Advance PC BEFORE execution (like C# implementation)
	 * Branch/jump instructions will overwrite PC as needed */
	cpu->PC = old_pc + (fi.total_len ? fi.total_len : fi.opcode_len);

	/* Boot-flow trace (env ND500X_BOOTDBG): log kernel (CED==0) entry to key
	 * scheduling / mount / I/O functions to map where the boot stalls and what
	 * proc[0] sleeps on. sleep()'s wchan is arg1 (register I1 at entry). */
	if (nd500_settings()->bootdbg && cpu->CED == 0) {
		const char* nm =
		    old_pc == 0x2319du ? "mountfs" :
		    old_pc == 0x0f578u ? "newproc" :
		    old_pc == 0x11f38u ? "sleep"   :
		    old_pc == 0x37d1fu ? "swtch"   :
		    old_pc == 0x32ac4u ? "sched"   :
		    old_pc == 0x20712u ? "bread"   :
		    old_pc == 0x212c7u ? "biowait" :
		    old_pc == 0x1213bu ? "wakeup"  :
		    old_pc == 0x1228fu ? "setrun"  :
		    old_pc == 0x3ee79u ? "xgintr"  :
		    old_pc == 0x35a3bu ? "swapconf": NULL;
		if (nm) {
			static uint32_t last = 0; static int rep = 0;
			static int dumped = 0;
			if (!dumped && old_pc == 0x11f38u) {   /* first sleep: dump drvtab */
				dumped = 1;
				for (int g = 0; g < 8; g++) {
					uint32_t intr = nd500_read_memory_32(cpu, 0x1c034u + (uint32_t)g*16u + 4u);
					fprintf(stderr, "[BOOTDBG] drvtab[%d].fr_intr=0x%08X (gen %d)\n", g, intr, g+1);
				}
			}
			if (old_pc == last) { rep++; }
			else {
				if (rep > 0) fprintf(stderr, "[BOOTDBG]   (x%d)\n", rep+1);
				rep = 0; last = old_pc;
				uint32_t a0 = cpu->pending_call_arg_count > 0 ? cpu->pending_call_arg_addresses[0] : 0;
				uint32_t chan = a0 ? nd500_read_memory_32(cpu, a0) : 0;
				fprintf(stderr, "[BOOTDBG] %-8s L=0x%08X argc=%u arg0@0x%08X=0x%08X\n",
				        nm, cpu->L, cpu->pending_call_arg_count, a0, chan);
			}
		}
	}

	/* One-shot frame-chain dump at the swtch() idle loop (env ND500X_SWTCHDBG):
	 * walk B -> PREVB(@0)/RETA(@4) to reveal who called swtch() and thus what
	 * proc[0] is waiting on when the scheduler goes idle with no runnable proc. */
	if (nd500_settings()->swtchdbg && cpu->CED == 0 && old_pc == 0x844u) {
		static int done = 0;
		if (!done) {
			done = 1;
			uint32_t b = cpu->B;
			fprintf(stderr, "[SWTCHDBG] swtch idle. frame chain (B=0x%08X):\n", b);
			for (int lvl = 0; lvl < 12 && b >= 0xE8000000u && b < 0xE8100000u; lvl++) {
				uint32_t prevb = nd500_read_memory_32(cpu, b + 0);
				uint32_t reta  = nd500_read_memory_32(cpu, b + 4);
				fprintf(stderr, "[SWTCHDBG]   L%d B=0x%08X RETA=0x%08X\n", lvl, b, reta);
				if (prevb == b || prevb == 0) break;
				b = prevb;
			}
		}
	}

	/* init-domain instruction trace (env ND500X_INITDBG): logs init (CED==1)
	 * execution of its low pcode + the current value of its syscall-code slot
	 * b.0x14, to see whether the page-faulting instruction (PC=4, sets code
	 * 0x3B) re-executes after the pagein or is skipped by the restart P. */
	if (cpu->CED == 1 && old_pc < 0x40 && nd500_settings()->initdbg) {
		printf("[INITDBG] CED=1 old_pc=0x%08X newPC=0x%08X B=0x%08X\n",
		       old_pc, cpu->PC, cpu->B);
	}

	/* ioctl-path trace (env ND500X_IOCDBG): which kernel routines a user ioctl
	 * actually reaches. Addresses from the parsed vmunix symbol table (only
	 * locore + properly relocated symbols; verified against _splx@0x83A). */
	{
		static int iod = -1;
		if (iod < 0) iod = nd500_settings()->iocdbg;
		if (iod && cpu->CED == 0) {
			const char* n =
			    old_pc == 0x173B4u ? "ioctl(syscall)" :
			    old_pc == 0x18C0Au ? "ino_ioctl"      :
			    old_pc == 0x3DDA6u ? "mxioctl"        :
			    old_pc == 0x1A336u ? "ttioctl"        :
			    old_pc == 0x19C55u ? "soo_ioctl"      :
			    old_pc == 0x00F75u ? "ifioctl"        :
			    old_pc == 0x1CE36u ? "nullioctl"      :
			    old_pc == 0x0DA10u ? "getf"           : NULL;
			if (n) {
				static unsigned k = 0;
				if (k++ < 200)
					fprintf(stderr, "[IOCDBG] %-14s B=0x%08X I1=0x%08X I2=0x%08X I3=0x%08X\n",
					        n, cpu->B, cpu->I[0], cpu->I[1], cpu->I[2]);
			}
		}
	}

	/* ttioctl compare-chain window (env ND500X_TTYDBG): the kernel's ttioctl
	 * dispatch on the ioctl command, to see which arm it takes / where it
	 * falls through to the ENOTTY default. Window covers the embedded
	 * TIOCGETD/TIOCGETP constants at 0x1AB61/0x1AB90. */
	{
		static int td2 = -1;
		if (td2 < 0) td2 = nd500_settings()->ttydbg;
		if (td2 && cpu->CED == 0 && old_pc >= 0x1AB30u && old_pc <= 0x1ABE0u) {
			static unsigned q = 0;
			if (q++ < 300)
				fprintf(stderr, "[TTYDBG] PC=0x%05X I1=0x%08X B=0x%08X\n", old_pc, cpu->I[0], cpu->B);
		}
	}

	/* Syscall-path trace (env ND500X_SYSDBG): log kernel (CED==0) entry to the
	 * syscall dispatcher / fuword / execve / nosys to see how init's execve
	 * syscall is decoded and where it errors. */
	if (cpu->CED == 0 && nd500_settings()->sysdbg) {
		const char* nm = (old_pc == 0x38f7a) ? "_syscall"
		               : (old_pc == 0x655)   ? "_fuword"
		               : (old_pc == 0xdba6)  ? "_execve"
		               : (old_pc == 0x38874) ? "_nosys"
		               : (old_pc == 0x6a2)   ? "_fuerror" : NULL;
		if (nm)
			printf("[SYSDBG] enter %s  B=0x%08X L=0x%08X R=0x%08X I1=0x%08X I2=0x%08X\n",
			       nm, cpu->B, cpu->L, cpu->R, cpu->I[0], cpu->I[1]);
		/* ND500X_SYSCNO: at the syscall dispatcher, print the SYSCALL NUMBER the
		 * kernel decoded (locore's _domain_call stores the user's code in the
		 * kernel frame; syscall() reads it) plus the frame, so a failing call can
		 * be named instead of guessed. Bounded. */
		{
			static int scn = -1;
			if (scn < 0) scn = nd500_settings()->syscno;
			if (scn && old_pc == 0x38f7a) {
				static unsigned n = 0;
				if (n++ < 4000) {
					uint32_t code = nd500_read_memory_32(cpu, cpu->B + 20);
					fprintf(stderr, "[SYSCNO] syscall code=%u (0x%X) B=0x%08X CAD=%u\n",
					        code, code, cpu->B, cpu->CAD);
				}
			}
		}
		/* Execution-window trace (env ND500X_EXEDBG): every CED=0 instruction
		 * in the fu*-routine window [0x640,0x6C0] - shows whether the faulting
		 * fubyte at ~0x678 is re-executed after the _Udata pagein. */
		{
			static int exed = -1;
			if (exed < 0) exed = nd500_settings()->exedbg;
			if (exed && old_pc >= 0x640 && old_pc < 0x6E0) {
				static uint64_t xn = 0;
				if (xn++ < 400)
					fprintf(stderr, "[EXEDBG] CED=0 PC=0x%04X B=0x%08X I1=0x%08X I2=0x%08X\n",
					        old_pc, cpu->B, cpu->I[0], cpu->I[1]);
			}
		}
		/* At fuword's add3 (0x657, after ENTS), b.24 holds the uaddr arg. */
		if (old_pc == 0x657) {
			/* Trap-free reads only. fuword's whole job is to probe a user
			 * address that MAY be bad, so uaddr is 0xFFFFFFFF often enough:
			 * translating _Udata+0xFFFFFFFF = 0xEFFFFFFF through the faulting
			 * path made the probe itself raise a kernel page fault, and the
			 * boot then died in a runaway trap loop at PC=0x657 - with
			 * ND500X_SYSDBG=1 the machine never reached login: at all.
			 * Also stderr, not stdout: stdout is the guest console. */
			uint32_t pa24 = nd500_mmu_peek(cpu, cpu->B + 24);
			uint32_t pa20 = nd500_mmu_peek(cpu, cpu->B + 20);
			uint32_t a24 = (pa24 == 0xFFFFFFFFu) ? 0xFFFFFFFFu : nd500_bus_read32(cpu->machine, pa24);
			uint32_t a20 = (pa20 == 0xFFFFFFFFu) ? 0xFFFFFFFFu : nd500_bus_read32(cpu->machine, pa20);
			uint32_t uva = 0xF0000000u + a24;
			uint32_t pau = nd500_mmu_peek(cpu, uva);
			if (pau == 0xFFFFFFFFu)
				fprintf(stderr, "[SYSDBG] fuword add3: B=0x%08X [B+20]=0x%08X [B+24]=0x%08X (uaddr) -> _Udata+uaddr=0x%08X UNMAPPED\n",
				        cpu->B, a20, a24, uva);
			else
				fprintf(stderr, "[SYSDBG] fuword add3: B=0x%08X [B+20]=0x%08X [B+24]=0x%08X (uaddr) -> _Udata+uaddr=0x%08X *uaddr=0x%08X\n",
				        cpu->B, a20, a24, uva, nd500_bus_read32(cpu->machine, pau));
		}
	}

	cpu->in_execute = 1;
	nd500_execute_decoded(cpu, &fi);
	cpu->in_execute = 0;

	/* -------------------------------------------------------------------
	 * CAD-change trace (opt-in via ND500X_CADDBG). Prints every time the
	 * live CAD register changes value, with the PC of the instruction that
	 * caused it and the current CED. Used to determine how (or whether) CAD
	 * ever becomes non-zero during the NDIX boot before the /etc/init launch
	 * RET at PC=0x29 (manual 4.2.5.2: domain-return gate is CAD!=CED && CAD!=0).
	 * ------------------------------------------------------------------- */
	{
		static int caddbg = -1;
		static uint32_t last_cad = 0xFFFFFFFFu;
		if (caddbg < 0) caddbg = nd500_settings()->caddbg;
		if (caddbg && cpu->CAD != last_cad) {
			printf("[CADDBG] CAD %u -> %u at PC=0x%08X (instr@0x%08X) CED=%u\n",
			       last_cad == 0xFFFFFFFFu ? 0 : last_cad,
			       cpu->CAD, old_pc, old_pc, cpu->CED);
			last_cad = cpu->CAD;
		}
	}

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
		if (guard_mode < 0) guard_mode = nd500_settings()->nc_typetag_guard;
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

	/* DT: has this SOLO region outstayed its welcome? Checked before the
	 * ignorable traps because a timeout is non-ignorable and outranks them. */
	check_solo_timeout(cpu, old_pc);

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
			printf("[STOP] %s at PC=0x%08X data=0x%08X (B=0x%08X R=0x%08X L=0x%08X)\n",
			       nd500_stop_reason_str(cpu->machine->stop_reason),
			       trap->trap_pc, trap->trap_data_addr, cpu->B, cpu->R, cpu->L);
			nd500_dump_stop_ring("trap-exec");
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
/* When set (by the shell for a clean prompt), suppress informational CPU-side
 * printf output (domain allocation, MMU-enable, MON-halt notices). Default 0
 * keeps existing behaviour for the debugger, --run and tests. */
int nd500_quiet = 0;

/* --- Real NDIX DIT accessors ---------------------------------------------
 * The live kernel DIT is 256 bytes/domain (PCBSIZ) at physical DITBASE; field
 * offsets verified against kernel/MASTER/machine/pcb.h (struct pcb, 8-bit
 * packed) AND ND-500 Reference Manual ND-05.009.4 Table 6 (octal offsets):
 *   OTE @226B=150  MTE @246B=166  THA @266B=182  Mother @272B=186
 *   Inside-trap-handler flag @273B=187  TOS/LL/HL = struct order 188/192/196
 *   Trap save area: Trapped @213B=139  Alt @214B=140  Status @216B=142/146
 * DITBASE is PHYSICAL, so use the bus accessors (not the MMU). These differ
 * from the stale toy layout in nd500_domain.c (16B/domain) - do not use that. */
#define NDIX_DIT_STRIDE   256u
#define DIT_OFF_TRAPPED   139u
#define DIT_OFF_TRAP_ALT  140u
#define DIT_OFF_TRAP_ST1  142u
#define DIT_OFF_TRAP_ST2  146u
#define DIT_OFF_OTE1      150u
#define DIT_OFF_OTE2      154u
#define DIT_OFF_MTE1      166u
#define DIT_OFF_MTE2      170u
#define DIT_OFF_THA       182u
#define DIT_OFF_MD        186u
#define DIT_OFF_ITH       187u
#define DIT_OFF_TOS       188u
#define DIT_OFF_LL        192u
#define DIT_OFF_HL        196u
#define DIT_OFF_PIA       200u   /* pcb_pia / domain status (PiA = bit 0), manual 310B=200 */

static inline uint32_t ndix_dit_r32(Nd500Cpu* cpu, uint32_t dom, uint32_t off) {
	return nd500_bus_read32(cpu->machine, cpu->DITBASE + dom * NDIX_DIT_STRIDE + off);
}
static inline uint8_t ndix_dit_r8(Nd500Cpu* cpu, uint32_t dom, uint32_t off) {
	return nd500_bus_read8(cpu->machine, cpu->DITBASE + dom * NDIX_DIT_STRIDE + off);
}
static inline void ndix_dit_w32(Nd500Cpu* cpu, uint32_t dom, uint32_t off, uint32_t v) {
	nd500_bus_write32(cpu->machine, cpu->DITBASE + dom * NDIX_DIT_STRIDE + off, v);
}
static inline void ndix_dit_w8(Nd500Cpu* cpu, uint32_t dom, uint32_t off, uint8_t v) {
	nd500_bus_write8(cpu->machine, cpu->DITBASE + dom * NDIX_DIT_STRIDE + off, (uint8_t)v);
}

/* Privilege (PIA, ST1 bit 1) is a DOMAIN attribute on the ND-500 (domain status
 * PiA @ DIT 310B=200), so it must follow the executing domain across every domain
 * transition. Apply the given domain's PiA to the live ST1. Called on trap
 * dispatch (-> handler domain), RETT (-> restored domain) and domain return
 * (-> launched domain). No-op when no real DIT is present (single-domain SINTRAN). */
void nd500_apply_domain_pia(Nd500Cpu* cpu, uint32_t domain) {
	if (!cpu || !cpu->machine || !cpu->DITBASE) return;
	uint8_t pia = ndix_dit_r8(cpu, domain, DIT_OFF_PIA);
	if (pia & 1u) cpu->ST1 |=  (1u << ND500_ST_BIT_PIA);
	else          cpu->ST1 &= ~(1u << ND500_ST_BIT_PIA);
}

unsigned long g_ptdbg_target = 0, g_ptdbg_count = 0;
/*
 * Mother-domain trap dispatch, factored out of raise_trap so BOTH the
 * non-ignorable path and the ignorable path can use it.
 *
 * MTE (Mother Trap Enable) means "this trap is handled by the mother domain",
 * so an MTE-enabled trap MUST switch domains before the handler vector is
 * read - THA points into the kernel's u-area (segment 29, 0xE8000000), which a
 * user domain has no data capability for. Dispatching PRT without this switch
 * produced, from user domain 3:
 *     [MMU] TRAP: No data capability! domain=3 segment=29 vaddr=0xE80007B0
 *     [TRAP] No trap handler at THA[29] (THA=0xE800073C, ptr=0xE80007B0)
 */
static void nd500_trap_maybe_cross_domain(Nd500Cpu* cpu, uint64_t trapBit) {
	cpu->trap_cross_domain = 0;
	/* Env-gated PRT dispatch probe (ND500X_PRTDBG). */
	{	static int prtdbg = -1; static unsigned n = 0;
		if (prtdbg < 0) prtdbg = nd500_settings()->prtdbg;
		if (prtdbg && (trapBit & TRAP_PRT) && n++ < 12)
			fprintf(stderr, "[PRTDBG] xdom entry: DITBASE=0x%08X inH=%d CED=%u MD(CED)=%u\n",
			        cpu->DITBASE, cpu->in_trap_handler, cpu->CED,
			        cpu->DITBASE ? ndix_dit_r8(cpu, cpu->CED, DIT_OFF_MD) : 0);
	}
	if (cpu->DITBASE && !cpu->trap_dispatch_pending) {
		int tn2 = 0; for (int i = 0; i < 64; i++) { if ((trapBit >> i) & 1) { tn2 = i; break; } }
		uint32_t d = cpu->CED, handler = 0xFFFFFFFFu;
		for (int hops = 0; hops < 64; hops++) {
			uint64_t ote = (uint64_t)ndix_dit_r32(cpu, d, DIT_OFF_OTE1)
			             | ((uint64_t)ndix_dit_r32(cpu, d, DIT_OFF_OTE2) << 32);
			uint8_t ith = ndix_dit_r8(cpu, d, DIT_OFF_ITH);
			if (((ote >> tn2) & 1) && !ith) { handler = d; break; }
			uint64_t mte = (uint64_t)ndix_dit_r32(cpu, d, DIT_OFF_MTE1)
			             | ((uint64_t)ndix_dit_r32(cpu, d, DIT_OFF_MTE2) << 32);
			if ((mte >> tn2) & 1) {
				uint32_t mother = ndix_dit_r8(cpu, d, DIT_OFF_MD);
				if (mother == d) break;      /* reached the top of the tree */
				/* MTE means "MY MOTHER HANDLES THIS" - the mother does NOT have
				 * to own-enable it as well. Requiring an OTE match somewhere up
				 * the chain made three traps undispatchable under NDIX, because
				 * user PCBs own-enable NOTHING (machine/vm_machdep.c:53 sets
				 * pcb_ote1 = pcb_ote2 = 0) and the kernel's own mask
				 * T_KOTE1 = 0xD601D800 lacks exactly SIT, BPT and PRT while the
				 * child's T_CMTE1 = 0xF413D800 delegates all three.
				 *
				 * PRT is the load-bearing one: it is how NDIX delivers every
				 * pending signal, profiling tick and reschedule, so with it
				 * undispatchable a process that should die from SIGSEGV instead
				 * refaulted forever and halted the machine.
				 *
				 * Traps the kernel DOES own-enable (page fault 38 via
				 * T_KOTE2 = 0x5F, etc.) still match on OTE at the top of this
				 * loop and are completely unaffected. */
				d = mother;
				handler = d;
				continue;
			}
			break;                            /* not enabled anywhere up the chain */
		}
		if (handler != 0xFFFFFFFFu && handler != cpu->CED) {
			/* Save the trapping context into the handler domain's DIT trap area */
			ndix_dit_w8 (cpu, handler, DIT_OFF_TRAPPED,  (uint8_t)cpu->CED);
			ndix_dit_w8 (cpu, handler, DIT_OFF_TRAP_ALT, (uint8_t)cpu->CAD);
			ndix_dit_w32(cpu, handler, DIT_OFF_TRAP_ST1, cpu->ST1);
			ndix_dit_w32(cpu, handler, DIT_OFF_TRAP_ST2, cpu->ST2);
			/* Stash trapping CED/CAD so ENTT records them (arg25/26) for RETT */
			cpu->trap_saved_CED = cpu->CED;
			cpu->trap_saved_CAD = cpu->CAD;
			/* Switch: CAD <- trapping domain (manual: "CAD is loaded with CED of
			 * the trapping domain"), CED <- handling mother domain. */
			cpu->CAD = cpu->CED;
			cpu->CED = handler;
			/* Load ONLY the handler domain's THA from its DIT (needed to locate
			 * the handler vector). Deliberately leave TOS/LL/HL as the trapping
			 * program's live values so the register block that ENTT saves - and
			 * RETT restores - carries the trapping domain's stack registers, not
			 * the handler's. (If the kernel handler proves to need its own TOS,
			 * add DIT save/restore of TOS/LL/HL across the switch here + in RETT.) */
			/* Do NOT load THA from the mother's DIT pcb_tha here. In NDIX the
			 * trap-handler vector is per-process/per-nesting-level and is set up
			 * LIVE by the kernel's __resume (THA = _u + U_CXB0 + traplev*496),
			 * pointing into the current process's u-area; the static pcb_tha in
			 * the DIT (kpcbinit's &Ktrap) is 6 bytes off (U_CXB0 vs _Ktrap) and
			 * yields a misaligned garbage vector. Only the kernel ever sets THA,
			 * and a user domain never overwrites it, so the live THA is already
			 * the correct kernel handler vector at trap time - keep it. */
			uint32_t old_tha = cpu->THA;  /* == live/kept THA (for debug below) */
			/* Privilege follows the handler domain (kernel pcb_pia=1) so its
			 * handler can run privileged instructions (e.g. entrap's dcc/pctsb). */
			nd500_apply_domain_pia(cpu, handler);
			cpu->trap_cross_domain = 1;
			if (nd500_settings()->domdbg) {
				printf("[DOMTRAP] trap bit %d in domain %u -> mother domain %u  DIT.THA=0x%08X  live.THA(pre)=0x%08X\n",
				       tn2, cpu->CAD, cpu->CED, cpu->THA, old_tha);
				for (int s = tn2 - 1; s <= tn2 + 1; s++) {
					uint32_t va = old_tha + (uint32_t)s * 4u;
					uint32_t pa = nd500_mmu_translate(cpu, va, 0, 0);
					printf("[DOMTRAP]   live.THA[%d] @va=0x%08X pa=0x%08X = 0x%08X\n",
					       s, va, pa, nd500_bus_read32(cpu->machine, pa));
				}
				/* Dump the handler-vector slots around this trap number so we
				 * can see whether THA points at a valid start-address vector. */
				for (int s = tn2 - 2; s <= tn2 + 1; s++) {
					if (s < 0) continue;
					uint32_t va = cpu->THA + (uint32_t)s * 4u;
					uint32_t pa = nd500_mmu_translate(cpu, va, 0, 0);
					printf("[DOMTRAP]   THA[%d] @va=0x%08X pa=0x%08X = 0x%08X\n",
					       s, va, pa, nd500_bus_read32(cpu->machine, pa));
				}
			}
		}
	}
}

/* CALL/ENT* sequence-interlock save stack. See the long note in cpu_protos.h for
 * why this is a keyed ring and not a single slot or a plain depth counter.
 * RetroCore CpuND500.Trap.cs must match this exactly. */
void nd500_trap_seq_push(Nd500Cpu* cpu, uint32_t frame_base) {
	uint32_t slot = cpu->trap_seq_head;
	cpu->trap_seq[slot].frame_base     = frame_base;
	cpu->trap_seq[slot].return_address = cpu->pending_call_return_address;
	uint32_t n = cpu->pending_call_arg_count;
	if (n > TRAP_SEQ_MAXARG) n = TRAP_SEQ_MAXARG;
	cpu->trap_seq[slot].arg_count = n;
	for (uint32_t i = 0; i < n; i++)
		cpu->trap_seq[slot].arg_addresses[i] = cpu->pending_call_arg_addresses[i];

	cpu->trap_seq_head = (slot + 1u) % TRAP_SEQ_RING;
	if (cpu->trap_seq_count < TRAP_SEQ_RING)
		cpu->trap_seq_count++;
	/* else: the oldest entry has just been overwritten. Nesting that deep means
	 * the guest is not unwinding, so the oldest is the least likely to be wanted. */
}

/* Restore the interlock belonging to the trap frame at frame_base. Searches
 * newest-first, so properly nested traps match their own dispatch. On a hit,
 * everything pushed after it is discarded - those are frames the guest abandoned
 * (a process killed by SIGSEGV never runs its RETT), and keeping them would let
 * the ring fill with corpses. No match restores a cleared interlock, which is the
 * old single-slot behaviour and the safe direction. */
void nd500_trap_seq_pop(Nd500Cpu* cpu, uint32_t frame_base) {
	for (uint32_t back = 1; back <= cpu->trap_seq_count; back++) {
		uint32_t slot = (cpu->trap_seq_head + TRAP_SEQ_RING - back) % TRAP_SEQ_RING;
		if (cpu->trap_seq[slot].frame_base != frame_base)
			continue;

		cpu->pending_call_return_address = cpu->trap_seq[slot].return_address;
		cpu->pending_call_arg_count      = cpu->trap_seq[slot].arg_count;
		for (uint32_t i = 0; i < cpu->trap_seq[slot].arg_count; i++)
			cpu->pending_call_arg_addresses[i] = cpu->trap_seq[slot].arg_addresses[i];

		cpu->trap_seq_head   = slot;            /* drop this entry and any above it */
		cpu->trap_seq_count -= back;
		return;
	}
	cpu->pending_call_return_address = 0;
	cpu->pending_call_arg_count = 0;
}

/* Restore the interlock from the NEWEST entry, whatever frame it belongs to.
 *
 * NDIX does not return from a kernel trap with RETT: machine/locore.c trapex ends
 * the handler with `lregbl $CNTXMASK,r3`, reloading P/ST1/CED/CAD straight out of
 * the saved context block (see the note in SYSTEM/Lregbl.c). So the frame-keyed
 * pop above never runs under NDIX - measured 132 pushes and 0 pops across one boot,
 * which left the ring saturated and every interlock lost. An lregbl trap-return
 * carries no frame address to key on, but handler entry/exit is strictly nested,
 * so the newest entry is by definition the one this return belongs to. */
void nd500_trap_seq_pop_top(Nd500Cpu* cpu) {
	if (cpu->trap_seq_count == 0) {
		cpu->pending_call_return_address = 0;
		cpu->pending_call_arg_count = 0;
		return;
	}
	uint32_t slot = (cpu->trap_seq_head + TRAP_SEQ_RING - 1u) % TRAP_SEQ_RING;
	nd500_trap_seq_pop(cpu, cpu->trap_seq[slot].frame_base);
}

void raise_trap(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trapPC, uint32_t dataAddr) {
	if (!cpu) return;

	/* Latch P1 = the TRAPPING P (see cpu_protos.h for the register's provenance).
	 * Done for EVERY trap, and before anything below can return early, so a
	 * handler or a post-mortem always sees the instruction the trap was raised
	 * for rather than a stale address from an earlier fault. The PGF/PV block
	 * further down adjusts the RESTART P; this is the separate trapping P and the
	 * two must not be conflated. */
	if (cpu->cur_instr_pc != 0) cpu->P1 = cpu->cur_instr_pc;

	{ static int init = 0;
	  if (!init) { init = 1; g_ptdbg_target = nd500_settings()->ptdbg_target; }
	  if (g_ptdbg_target && dataAddr == (uint32_t)g_ptdbg_target && !g_ptdbg_count)
	      g_ptdbg_count = 60; }

	/* A PAGE FAULT is restartable: after the missing page is mapped, the
	 * faulting instruction must RE-EXECUTE (so its memory access completes).
	 * The emulator advances cpu->PC to the next instruction BEFORE execute,
	 * so trap_page_fault passes that advanced PC (e.g. init's PC=4 store to
	 * its stack faults with cpu->PC already=8). Restarting at 8 would SKIP
	 * the faulting store -- init's `move $0x3B,b.0x14` (the execve syscall
	 * code) never lands, so execve gets code 0 and fails (K set). Use the
	 * faulting instruction's own start PC as the trapping/restart P. */
	/* The same correction applies to a PROTECT VIOLATION, and for the same
	 * reason: the trapping P must name the instruction that made the access,
	 * not the one after it. The architecture keeps the two apart deliberately -
	 * the context block has BOTH a "Trapping P register" and a "Restart P
	 * register" (ND-05.017.01 Appendix A.1, registers 0 and 1).
	 * Mirrored from RetroCore 2026-07-27, where a protect violation in the
	 * SINTRAN swapper was reported at 0x0800913B while the instruction
	 * responsible was at 0x08009137 - four bytes earlier - which sent the
	 * investigation to the wrong instruction entirely. */
	if ((trapBit & (TRAP_PGF | TRAP_PV)) && cpu->cur_instr_pc != 0) {
		/* cur_instr_pc is now set BEFORE decode, so it always names the
		 * instruction that faulted - including the first fetch after a
		 * domain return, and a fetch that straddles a page boundary (where
		 * dataAddr is the NEXT page, not the instruction start). Use the
		 * fault address only when it IS the instruction start. */
		if ((trapBit & TRAP_PGF) && (cpu->mmu_pgf_where & 0x40u /*MMINST*/)
		    && !cpu->in_execute && dataAddr == cpu->cur_instr_pc) {
			/* INSTRUCTION-FETCH fault: the faulting fetch address IS the
			 * restart point. cur_instr_pc still names the PREVIOUS
			 * instruction - across a domain boundary (RET domain-return
			 * to a user entry whose text page is not yet in) that is the
			 * OLD domain's RET, and restoring it as the restart P resumed
			 * init at the kernel RET address inside domain 1 (PV).
			 * in_execute excludes program-space reads made WHILE an
			 * instruction executes (CALL/CALLG entry-point check): those
			 * must restart the instruction, not jump to its target. */
			trapPC = dataAddr;
		} else {
			trapPC = cpu->cur_instr_pc;
		}
	}

	/* NOTE on cpu->instr_aborted: it signals the currently-executing
	 * instruction to ABORT (no destination commit) - invoke_trap_handler
	 * clears the global trap state synchronously, so the flag is the only
	 * reliable mid-instruction fault indicator left when control returns
	 * into the instruction implementation. It is set ONLY on the paths
	 * below that actually dispatch or halt: an IGNORABLE trap that is not
	 * OTE-enabled merely records a status bit and the instruction MUST
	 * complete normally (setting the flag there made the guarded
	 * mmu_write helpers drop legitimate stores). Cleared at the top of
	 * cpu_step. */

	/* Remember the faulting logical address for ENTT to place in the trap frame's
	 * N field (B+16), which the NDIX kernel (locore entrap) copies to cx_vaddr and
	 * pagein() faults in. Without this ENTT wrote the literal arg count 50 (=0x32)
	 * there and the kernel paged in address 0x32 instead of the real fault address,
	 * so no user stack/data page was ever mapped (panic: pagein valid page). */
	cpu->trap_saved_fault_addr = dataAddr;
	/* For a page fault, hand the kernel the fault-location code (MMWHERE nibble)
	 * in cx_info so its PGF handler recognises a demand-paging fault and calls
	 * pagein() rather than panicking. The MMU walk records where the walk found a
	 * zero PTE (PFZ2 for the common 2nd-level miss); default to PFZ2 when unset. */
	if (trapBit & TRAP_PGF)
		cpu->trap_saved_info = cpu->mmu_pgf_where ? cpu->mmu_pgf_where : 0xFu /*PFZ2*/;
	else if (trapBit & TRAP_PV)
		/* A PROTECT VIOLATION carries a fault-location code too, and the kernel
		 * needs it: machine/trap.c gates BOTH T_PV (:314) and T_PV+USER (:360)
		 * on (info&MMWHERE)==PVWVIOL && (info&MMINST)==0 before calling
		 * pagein(). With cx_info left at 0 that gate never fired, so a write to
		 * a write-protected page - the one protect violation NDIX recovers from
		 * - went straight to panic("Kernel Protect Violation") in kernel mode
		 * and SIGSEGV in user mode, and fuword/suword could never fault a user
		 * page in. The MMU walk records which check rejected the access
		 * (MMW_PVWVIOL / MMW_ZEROCAP / MMW_INDEXERR / MMW_IND_*); 0 when the
		 * violation came from somewhere other than the walk. */
		cpu->trap_saved_info = cpu->mmu_pgf_where;
	else
		cpu->trap_saved_info = 0;
	cpu->mmu_pgf_where = 0;

	/* Diagnostic (env ND500X_FUWDBG): a kernel-domain (CED==0) trap on a
	 * _Udata/_Ustack window address (>=0xF0000000) is the fuword() read of a
	 * user syscall arg faulting because domain-0 seg 30/31 DATA capability is
	 * not mapped to the current user's data/stack. */
	if ((trapBit & TRAP_INTERRUPT_MASK) && cpu->CED == 0 && dataAddr >= 0xF0000000u && nd500_settings()->fuwdbg)
		printf("[FUWDBG] kernel trap bit=%d trapPC=0x%08X faultaddr=0x%08X B=0x%08X CAD=%u\n",
		       __builtin_ctzll(trapBit), trapPC, dataAddr, cpu->B, cpu->CAD);

	/* Diagnostic (env ND500X_KSUDBG): a kernel-domain (CED==0) trap taken while B is
	 * below _Ktrap (0xE8000736) is what makes locore trapex panic "kernel stack
	 * underflow detected during trap" on return. Log the trap that causes it. */
	if ((trapBit & TRAP_INTERRUPT_MASK) && cpu->CED == 0 && cpu->B < 0xE8000736u) {
		static int ksudbg = -1;
		if (ksudbg < 0) ksudbg = nd500_settings()->ksudbg;
		if (ksudbg)
			fprintf(stderr, "[KSUDBG] kernel trap bit=%d trapPC=0x%08X B=0x%08X L=0x%08X R=0x%08X TOS=0x%08X CAD=%u data=0x%08X\n",
			        __builtin_ctzll(trapBit), trapPC, cpu->B, cpu->L, cpu->R, cpu->TOS, cpu->CAD, dataAddr);
	}

	/* Trap tracing, off by default - it was firing on every ignorable trap (AZ,
	 * stack-overflow, ...) and polluting normal program output. Enable with
	 * ND500X_TRAPLOG=1 when debugging traps. */
	static int traplog = -1;
	if (traplog < 0) traplog = nd500_settings()->traplog;
	if (traplog) {
		fprintf(stderr, "[DEBUG] raise_trap: trapBit=0x%llX trapPC=0x%08X dataAddr=0x%08X INTERRUPT=%d instr_count=%llu\n",
		        (unsigned long long)trapBit, trapPC, dataAddr,
		        (trapBit & TRAP_INTERRUPT_MASK) ? 1 : 0,
		        (unsigned long long)cpu->instruction_count);
	}

	/* Runaway-trap-loop guard. A program whose data/stack segment is not mapped
	 * (e.g. a DOM whose runtime expects DSEG at segment 30 / 0xF0000000 that the
	 * loader never mapped) re-faults on the IDENTICAL (trapPC, dataAddr) forever:
	 * the trap dispatches to the program's handler, the handler retries the
	 * faulting instruction, and it faults again - an infinite loop that also
	 * floods the MMU error log. Legitimate page-fault handling makes progress
	 * (the retry succeeds, so the identical trap does not repeat), so a long run
	 * of identical consecutive traps only happens in a genuine dead loop. Detect
	 * it and HALT instead of spinning.
	 *
	 * DT and DE are covered too: a handler that returns without clearing PSD
	 * would otherwise re-time-out immediately, for ever. */
	if (trapBit & TRAP_NONIGNORABLE_MASK) {
		static uint32_t last_pc = 0xFFFFFFFFu, last_data = 0xFFFFFFFFu;
		static uint64_t last_bit = 0;
		static uint32_t rep = 0;
		if (trapPC == last_pc && dataAddr == last_data && trapBit == last_bit) {
			if (++rep > 500) {
				fprintf(stderr, "[TRAP] Runaway trap loop: %s at PC=0x%08X data=0x%08X "
				        "repeated - HALTING (likely an unmapped data/stack segment)\n",
				        nd500_stop_reason_str(trap_to_stop_reason(trapBit)), trapPC, dataAddr);
				nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
				if (cpu->machine) {
					cpu->machine->run_flag = 0;
					cpu->machine->stop_reason = trap_to_stop_reason(trapBit);
					cpu->machine->stop_addr = trapPC;
					cpu->machine->stop_data = dataAddr;
				}
				rep = 0; last_pc = 0xFFFFFFFFu;
				return;
			}
		} else {
			rep = 0; last_pc = trapPC; last_data = dataAddr; last_bit = trapBit;
		}
	}

	/* Set the corresponding bit in ST1/ST2 status registers */
	if (trapBit & 0xFFFFFFFF) {
		cpu->ST1 |= (uint32_t)(trapBit & 0xFFFFFFFF);
	}
	if (trapBit >> 32) {
		cpu->ST2 |= (uint32_t)(trapBit >> 32);
	}

	/* ---- DE: a non-ignorable trap taken inside a SOLO region -------------
	 *
	 * Manual ch.6.5.4: "When executing with the process switch disable set,
	 * non-ignorable traps (such as page fault) that require process switching
	 * must not occur. If they do occur, they cause a disable process switch
	 * error trap condition." Ch.16.1 says the same of SOLO: "Non-ignorable and
	 * fatal traps cause a disable process switch error trap."
	 *
	 * So DE is raised ALONGSIDE the trap that provoked it, not instead of it:
	 * the status bit records that the fault happened at a point where the
	 * machine could not afford to switch. NDIX enables DE and vectors it
	 * (machine/locore.c:690), which is how a kernel finds out that a solo
	 * region - a context switch, an interrupt entry - touched an unmapped page.
	 *
	 * Guarded against re-entry: DE itself is non-ignorable, so raising it from
	 * here without the check would recurse. */
	if ((trapBit & TRAP_NONIGNORABLE_MASK) && !(trapBit & TRAP_DE) &&
	    (cpu->ST1 & ND500_FLAG_PSD)) {
		cpu->ST1 |= (uint32_t)TRAP_DE;
		{
			static int dbg = -1;
			if (dbg < 0) dbg = nd500_settings()->solodbg;
			if (dbg)
				fprintf(stderr, "[SOLO] DE: non-ignorable trap 0x%llX at PC=0x%08X "
				                "while process switch disabled\n",
				        (unsigned long long)trapBit, trapPC);
		}
	}

	/* Check if this is a non-ignorable/fatal trap (bits 30-31, 32+) */
	if (trapBit & TRAP_NONIGNORABLE_MASK) {
		cpu->instr_aborted = 1;   /* dispatch or halt - the instruction aborts */
		/* ---- Mother-domain trap dispatch (manual 4.2.5.3 + ch.6 Fig.18) ----
		 * When a trap in the current (child) domain is NOT own-handled there but a
		 * mother domain has a handler, switch to the handling mother domain so its
		 * trap handler runs with its own capabilities and THA. Only engaged when a
		 * real kernel DIT is present (NDIX multi-domain); single-domain SINTRAN
		 * keeps its existing live-THA dispatch untouched (trap_cross_domain stays 0).
		 * Search follows the pcb_md chain: own-enabled (OTE & !inside-handler) wins
		 * locally; else mother-enabled (MTE) propagates up; else stop. */
		nd500_trap_maybe_cross_domain(cpu, trapBit);
		/* Non-ignorable traps (bits 32-41: PV, ISE, THM, PGF, ...) are still delivered to the
		 * PROGRAM via its THA vector on the ND-500 - a program installs handlers precisely to
		 * receive them (NC sets THA[36]=PV handler 0x0802D817, and handlers for 32-41). The
		 * machine only halts when there is NO handler (THA==0 or slot==0 -> Trap Handler Missing)
		 * or when a fault occurs while ALREADY inside a handler (a double fault - our single-level
		 * saved-trap state cannot nest, and a nested non-ignorable trap is a genuine hard error).
		 * Opt out with ND500X_NO_TRAP_DISPATCH=1 to restore the old always-halt behavior. */
		static int td = -1;
		if (td < 0) td = nd500_settings()->trap_dispatch;
		if (td && cpu->THA != 0 && !cpu->trap_dispatch_pending) {
			int tn = 0; for (int i = 0; i < 64; i++) { if ((trapBit >> i) & 1) { tn = i; break; } }
			uint32_t hp = nd500_mmu_translate(cpu, cpu->THA + tn * 4, 0, 0);
			uint32_t haddr = nd500_trap_occurred() ? 0 : nd500_bus_read32(cpu->machine, hp);
			{ static int thd = -1;
			  if (thd < 0) thd = nd500_settings()->thadbg;
			  if (thd) fprintf(stderr, "[THADBG] trap %d trapPC=0x%08X data=0x%08X CED=%u CAD=%u THA=0x%08X slot@0x%08X haddr=0x%08X xdom=%d\n",
			                   tn, trapPC, dataAddr, cpu->CED, cpu->CAD, cpu->THA, hp, haddr, cpu->trap_cross_domain); }
			if (haddr != 0) {
				nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
				invoke_trap_handler(cpu, trapBit, trapPC);
				return;
			}
			/* no handler installed for this trap -> fall through to halt (Trap Handler Missing) */
		}
		/* Set trap state - this WILL stop execution */
		nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
		/* Both program registers - same reason as the [STOP] line above. */
		TRACE("[TRAP] %s at PC=0x%08X data=0x%08X P1=0x%08X <- failing instruction\n",
		      nd500_stop_reason_str(trap_to_stop_reason(trapBit)), trapPC, dataAddr, cpu->P1);
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

	/* Ignorable trap (bits 11-29): enabled by EITHER the own-domain mask (OTE)
	 * or the mother-domain mask (MTE) - see the same combination and its
	 * rationale in check_pending_traps(). */
	uint64_t ote = ((uint64_t)cpu->OTE2 << 32) | cpu->OTE1;
	uint64_t mte = ((uint64_t)cpu->MTE2 << 32) | cpu->MTE1;
	if ((trapBit & TRAP_IGNORABLE_MASK) && cpu->DITBASE) {
		mte |= (uint64_t)ndix_dit_r32(cpu, cpu->CED, DIT_OFF_MTE1)
		     | ((uint64_t)ndix_dit_r32(cpu, cpu->CED, DIT_OFF_MTE2) << 32);
	}
	if (trapBit & (ote | mte) & TRAP_IGNORABLE_MASK) {
		/* Trap is enabled - set trap state and invoke handler. Do NOT set
		 * instr_aborted here: ignorable (arithmetic-class) traps on the
		 * ND-500 are post-completion - the instruction finishes its stores
		 * and THEN the handler runs (setting the flag suppressed those
		 * stores via the guarded mmu_write helpers and shifted NC's
		 * instruction count). Only the non-ignorable/MMU class aborts. */
		nd500_trap_set_state(trapBit, trapPC, dataAddr, NULL);
		/* An MTE-delegated trap is handled by the MOTHER domain, so switch there
		 * before invoke_trap_handler reads the vector: THA points into the kernel
		 * u-area (segment 29), which a user domain has no capability for. */
		nd500_trap_maybe_cross_domain(cpu, trapBit);
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
/* ---- DT: process-switch-disable timeout ---------------------------------
 *
 * Manual ND-05.009.4 ch.6.5.4: "Synchronization procedures can execute with the
 * process switch disable status bit set. If this bit is set for more than 256
 * microcycles (including the 2 spent in the SOLO instruction), a process switch
 * timeout trap condition occurs."
 *
 * Two details from ch.16.1 decide how that is counted and to whom it applies:
 *
 *   "In the 500/2 implementation, these are microcycles. In the ND-5000
 *    implementation they are macroinstruction cycles."
 * This emulates the ND-5000, so the unit is executed instructions - which the
 * CPU already counts. Counting real microcycles would mean modelling per-operand
 * timing that nothing else here needs.
 *
 *   "In privilege mode there is no limitation to the duration of a SOLO
 *    operation. Unprivileged users are not allowed to run in SOLO for more than
 *    256 cycles."
 * So the timeout is armed for unprivileged code only. That is not a convenience:
 * NDIX's kernel sits in SOLO across context switches and interrupt entry
 * (machine/locore.c:508, :737) for far longer than 256 instructions, and would
 * trap on every switch if privilege were ignored.
 *
 * The +2 the manual mentions for the SOLO instruction itself is inside the
 * 256 and is not modelled separately - at instruction granularity it rounds
 * away, and erring long cannot produce a false timeout. */
#define SOLO_MAX_CYCLES 256

void check_solo_timeout(Nd500Cpu* cpu, uint32_t trappingPC) {
	if (!cpu) return;
	if (!(cpu->ST1 & ND500_FLAG_PSD)) return;      /* not in a SOLO region */
	if (nd500_is_privileged(cpu)) return;          /* privileged: no limit */

	if (cpu->instruction_count - cpu->solo_start_icount <= SOLO_MAX_CYCLES)
		return;

	{
		static int dbg = -1;
		if (dbg < 0) dbg = nd500_settings()->solodbg;
		if (dbg)
			fprintf(stderr, "[SOLO] DT: process switch disabled for %llu cycles "
			                "(limit %d) at PC=0x%08X\n",
			        (unsigned long long)(cpu->instruction_count - cpu->solo_start_icount),
			        SOLO_MAX_CYCLES, trappingPC);
	}

	/* Clear PSD before raising. The region is over as far as the machine is
	 * concerned, and leaving the bit set would make the handler's own first
	 * instruction time out again immediately. */
	cpu->ST1 &= ~ND500_FLAG_PSD;
	cpu->solo_start_icount = 0;

	raise_trap(cpu, TRAP_DT, trappingPC, 0);
}

/* Is <trapBit> enabled for the current domain?
 *
 * "Enabled" means EITHER the own-domain mask (OTE, "handle it here") or the
 * mother-domain mask (MTE, "the mother domain handles it"), with MTE also read
 * from the kernel's DIT - the live MTE registers are never loaded under NDIX,
 * so the DIT is where the real per-domain enables live. That is the same
 * combination raise_trap and check_pending_traps use; this exists so callers
 * outside cpu.c can ask the question without duplicating it, and without
 * needing the DIT accessors, which are private to this file.
 *
 * The BP instruction is the caller that needs it: manual ND-05.009.4 p.2086,
 * "BreakPoint instruction Trap condition occurs when a breakpoint instruction
 * (BP) is executed. If BPT is not enabled, a BP instruction will cause an IIC
 * trap condition." - so BP has to know before deciding which trap to raise. */
int nd500_trap_is_enabled(Nd500Cpu* cpu, uint64_t trapBit) {
	if (!cpu) return 0;
	{
		uint64_t ote = ((uint64_t)cpu->OTE2 << 32) | cpu->OTE1;
		uint64_t mte = ((uint64_t)cpu->MTE2 << 32) | cpu->MTE1;
		if (cpu->DITBASE) {
			mte |= (uint64_t)ndix_dit_r32(cpu, cpu->CED, DIT_OFF_MTE1)
			     | ((uint64_t)ndix_dit_r32(cpu, cpu->CED, DIT_OFF_MTE2) << 32);
		}
		return (trapBit & (ote | mte)) != 0;
	}
}

void check_pending_traps(Nd500Cpu* cpu, uint32_t trappingPC) {
	if (!cpu) return;

	/* Ignorable traps are suppressed inside a SOLO region.
	 *
	 * Manual ch.6.5.4, immediately after the timeout paragraph: "Ignorable trap
	 * conditions are ignored in SOLO-TUTTI sequences regardless of enabling of
	 * these traps." The status bits still accumulate - they are set by the
	 * instructions themselves - they simply do not dispatch until TUTTI. That
	 * is the whole point of the sequence being indivisible: a handler call is
	 * exactly the process switch SOLO exists to prevent. */
	if (cpu->ST1 & ND500_FLAG_PSD) return;

	/* Combine ST1 and ST2 into 64-bit status */
	uint64_t st = ((uint64_t)cpu->ST2 << 32) | cpu->ST1;

	/* A trap is enabled if EITHER the own-domain mask (OTE) or the mother-domain
	 * mask (MTE) has its bit set - OTE means "handle it here", MTE means "the
	 * mother domain handles it". Consulting OTE alone made every MTE-only trap
	 * invisible, and NDIX arms exactly this way: pcb_mte1 = T_CMTE1 (0xF413D800)
	 * covers IVO DZ FO BO IOV SIT BPT IX STU, and PRT is armed on demand at
	 * machine/machdep.c:1273 with  u.u_pcb->pcb_mte1 |= (1 << T_PRT).
	 *
	 * PRT (Programmed Trap, bit 29) is how NDIX delivers ALL pending signals,
	 * profiling ticks and rescheduling: machine/trap.c:718-731 sets the PRT bit
	 * in the SAVED context status so that returning to the user domain traps
	 * immediately, and its handler (trap.c:246) falls through to psig(). With
	 * PRT invisible, a page fault that should kill a process only QUEUED the
	 * signal (trap.c:544-550 returns early for T_PGF, deliberately skipping
	 * psig()), so the instruction retried, refaulted, and the runaway guard
	 * halted the whole machine instead of the process dying. */
	/* Fast path: nothing pending in the ignorable range, so do not touch the DIT.
	 * This runs after EVERY instruction and the DIT read below is guest memory. */
	uint64_t st_ign = st & TRAP_IGNORABLE_MASK;
	if (st_ign == 0) return;

	/* A trap is enabled by EITHER the own-domain mask (OTE) or the mother-domain
	 * mask (MTE). The LIVE OTE/MTE registers are never loaded under NDIX -
	 * measured with ND500X_PRTDBG, both read 0 for an entire boot - so nothing in
	 * the ignorable range could ever fire. Only the kernel's DIT carries the real
	 * per-domain enables, and raise_trap's cross-domain dispatch already sources
	 * MTE from there; do the same here so both paths agree. */
	uint64_t ote = ((uint64_t)cpu->OTE2 << 32) | cpu->OTE1;
	uint64_t mte = ((uint64_t)cpu->MTE2 << 32) | cpu->MTE1;
	if (cpu->DITBASE) {
		mte |= (uint64_t)ndix_dit_r32(cpu, cpu->CED, DIT_OFF_MTE1)
		     | ((uint64_t)ndix_dit_r32(cpu, cpu->CED, DIT_OFF_MTE2) << 32);
	}

	/* Find pending ignorable traps that are enabled */
	uint64_t pending = st_ign & (ote | mte);

	if (pending != 0) {
		/* Find highest priority trap (highest bit number).
		 *
		 * Down to 9, not 11: OVERFLOW is bit 9 and is an ignorable trap
		 * condition (see TRAP_IGNORABLE_MASK in cpu_protos.h for the manual
		 * references). Stopping at 11 meant an enabled overflow trap was
		 * masked in, matched as pending, and then never dispatched. Bit 10 is
		 * undefined and is absent from the mask, so it can never be pending. */
		for (int bit = 29; bit >= 9; bit--) {
			uint64_t trapBit = 1ULL << bit;
			if (pending & trapBit) {
				/* MTE-delegated traps are handled by the MOTHER domain, so switch
				 * there BEFORE the handler vector is read - THA points into the
				 * kernel u-area (segment 29), unreadable from a user domain.
				 * This path reaches invoke_trap_handler directly, so it needs the
				 * same switch raise_trap's non-ignorable path performs. */
				nd500_trap_maybe_cross_domain(cpu, trapBit);
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

	if (nd500_settings()->privdbg)
		fprintf(stderr, "[PRIVDBG] dispatch trap %d -> handler=0x%08X priv=%d ST1=0x%08X CED=%u CAD=%u THA=0x%08X inH=%d\n",
		        trapNumber, handlerAddr, nd500_is_privileged(cpu) ? 1 : 0, cpu->ST1,
		        cpu->CED, cpu->CAD, cpu->THA, cpu->in_trap_handler);

	if (handlerAddr == 0) {
		fprintf(stderr, "[TRAP] No trap handler at THA[%d] (THA=0x%08X, ptr=0x%08X)\n",
		       trapNumber, cpu->THA, handlerPointer);
		if (nd500_settings()->stodbg)
			fprintf(stderr, "[STODBG] trap %d trapPC=0x%08X B=0x%08X TOS=0x%08X LL=0x%08X HL=0x%08X ST1=0x%08X OTE1=0x%08X CED=%u inH=%d\n",
			        trapNumber, trappingP, cpu->B, cpu->TOS, cpu->LL, cpu->HL, cpu->ST1, cpu->OTE1, cpu->CED, cpu->in_trap_handler);
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
		fprintf(stderr, "[TRAP] No ENTT instruction at trap handler 0x%08X (found 0x%02X, expected 0xBC)\n",
		       handlerAddr, byte0);
		/* Clear trap bit since handler is invalid */
		if (trapBit & 0xFFFFFFFF)
			cpu->ST1 &= ~(uint32_t)(trapBit & 0xFFFFFFFF);
		if (trapBit >> 32)
			cpu->ST2 &= ~(uint32_t)(trapBit >> 32);
		return;
	}

	/* Save state for RETT to restore. Trapping P (frame arg1) is always the
	 * trapping instruction. The RESUME P (frame arg2, what RETT returns to)
	 * depends on the trap's timing class (manual ND-05.009.4 Table 10):
	 * After-class traps (O, IVO, DZ, FU, FO, BO, IOV, SIT, BT, CT, ATF, ATR,
	 * ATW, AZ, DT, DE, PWF) complete the instruction first, so the handler
	 * returns to the NEXT instruction - cpu->PC is already advanced past the
	 * trapping instruction both when raised mid-execute and when dispatched
	 * from check_pending_traps. Before/During-class traps retry the trapping
	 * instruction. (LED's runtime looped forever on a CHAIN zero-link IOV
	 * because the handler RETTed back into the same CHAIN.) */
	cpu->trap_saved_PC = trappingP;
	/* PRT is ASYNCHRONOUS - not caused by the instruction it interrupts. It fires
	 * because the status bit was set, normally by RETT restoring a context whose
	 * saved ST1 carries it (machine/trap.c:729). The instruction that just
	 * completed did so successfully and must NOT be re-executed: that would
	 * repeat its side effects, and in the common case it IS the RETT, which would
	 * re-restore the same context and re-fire PRT forever. Treat it as
	 * after-class regardless of TRAP_AFTER_MASK, which lists only
	 * instruction-caused traps (manual ND-05.009.4 Table 10). */
	cpu->trap_resume_PC = (trapBit & (TRAP_AFTER_MASK | TRAP_PRT)) ? cpu->PC : trappingP;
	cpu->trap_saved_OTE1 = cpu->OTE1;    /* Save trap enable state */
	cpu->trap_saved_OTE2 = cpu->OTE2;
	cpu->trap_number = trapNumber;
	cpu->in_trap_handler = true;
	cpu->trap_dispatch_pending = 1;   /* cleared by the handler's ENTT - see cpu_protos.h */

	/* The pending CALL/ENT* sequence interlock is saved and cleared by the handler's
	 * ENTT, not here: ENTT is verified above to be the handler's first instruction
	 * (byte0 == 0xBC), so nothing executes in between, and ENTT is where the trap
	 * frame address that keys the save is computed (THA+256). Keying it there also
	 * survives a domain switch reloading THA between this dispatch and the ENTT. */

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
	if (nd500_settings()->pgfdbg)
		printf("[PVDBG] protect violation CED=%u CAD=%u PC=0x%08X addr=0x%08X B=0x%08X\n",
		       cpu->CED, cpu->CAD, pc, address, cpu->B);
	raise_trap(cpu, TRAP_PV, pc, address);
}

void trap_page_fault(Nd500Cpu* cpu, uint32_t pc, uint32_t address) {
	if (nd500_settings()->pgfdbg)
		printf("[PGFDBG] page fault CED=%u CAD=%u PC=0x%08X faultaddr=0x%08X B=0x%08X ret=%p\n",
		       cpu->CED, cpu->CAD, pc, address, cpu->B,
		       __builtin_return_address(0));
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

/* Backing check for the ND500X_TRAPLOG macro in cpu_protos.h. Calling getenv()
 * on a per-instruction path would be absurd, so cache the answer on first use. */
int nd500x_traplog(void) {
	static int state = -1;
	if (state < 0) state = nd500_settings()->traplog;
	return state;
}

void trap_invalid_operation(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_IVO, pc, 0);
}

/* Integer overflow condition (O, bit 9) - see TRAP_O in cpu_protos.h. The
 * status flag is set by the instruction regardless; this only offers the trap,
 * which is taken solely if the guest armed bit 9. NDIX does not, which is the
 * correct outcome for ordinary signed overflow in C code. */
void trap_integer_overflow(Nd500Cpu* cpu, uint32_t pc) {
	raise_trap(cpu, TRAP_O, pc, 0);
}

void trap_stack_overflow(Nd500Cpu* cpu, uint32_t pc) {
	/* ND500X_STODBG: log the full dispatch-relevant state at every STO raise
	 * (heap vars at TOS, THA[27], OTE bit 27, handler nesting) - discriminating
	 * experiment for the NC exit crash, see
	 * docs/HANDOFF-NC-HEAP-CRASH-2026-07-27.md section 2b. */
	if (nd500_settings()->stodbg) {
		uint32_t hv[3] = { 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu };
		for (int i = 0; i < 3; i++) {
			uint32_t pa = nd500_mmu_peek(cpu, cpu->TOS + (uint32_t)i * 4u);
			if (pa != 0xFFFFFFFFu) hv[i] = nd500_bus_read32(cpu->machine, pa);
		}
		uint32_t pth = nd500_mmu_peek(cpu, cpu->THA + 27u * 4u);
		uint32_t h27 = (pth != 0xFFFFFFFFu) ? nd500_bus_read32(cpu->machine, pth)
		                                    : 0xDEADBEEFu;
		fprintf(stderr, "[STODBG] raise PC=0x%08X TOS=0x%08X MAXL=0x%08X "
		        "STAH=0x%08X ENDH=0x%08X THA=0x%08X THA[27]=0x%08X OTE27=%d "
		        "inH=%d CED=%u\n",
		        pc, cpu->TOS, hv[0], hv[1], hv[2], cpu->THA, h27,
		        (int)((cpu->OTE1 >> 27) & 1u), cpu->in_trap_handler ? 1 : 0,
		        cpu->CED);
	}
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
    /* The bits below have no C# TrapType counterpart, so they used to fall
     * through to "Unknown". That is a bad trade: a test failure reporting
     * "Trap: Unknown ()" says nothing, and chasing one such report - a DT from
     * a stale instruction_count, see nd500_cpu_reset - took far longer than it
     * should have. Name them after the architecture instead. A validator
     * comparing against a C# TrapType will not match these, which is correct:
     * they are conditions that enum cannot express. */
    else if (condition & TRAP_O)   name = "IntegerOverflow";     /* bit 9  */
    else if (condition & TRAP_SIT) name = "SingleInstructionTrap"; /* bit 17 */
    else if (condition & TRAP_BT)  name = "BranchTrap";          /* bit 18 */
    else if (condition & TRAP_CT)  name = "CallTrap";            /* bit 19 */
    else if (condition & TRAP_BPT) name = "BreakpointTrap";      /* bit 20 */
    else if (condition & TRAP_PRT) name = "ProgrammedTrap";      /* bit 29 */
    else if (condition & TRAP_DT)  name = "SoloTimeout";         /* bit 30 */
    else if (condition & TRAP_DE)  name = "SoloError";           /* bit 31 */
    else if (condition & TRAP_XSE) name = "IndexScalingError";   /* bit 32 */
    else if (condition & TRAP_THM) name = "TrapHandlerMissing";  /* bit 37 */
    else if (condition & TRAP_PWF) name = "PowerFailure";        /* bit 39 */
    else if (condition & TRAP_PRF) name = "ProcessorFault";      /* bit 40 */
    else if (condition & TRAP_HF)  name = "HardwareFault";       /* bit 41 */

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
 * @param nd100_addr ND-100 physical word address (24-bit, 0x000000-0xFFFFFF)
 * @return           Halfword value read from ND-100 memory
 *
 * ND-100 Physical Memory Architecture:
 *
 * ND-100 uses 24-bit word addressing (24-bit physical addresses):
 *   0x000000 - 0x00FFFF (64K words, 128KB)  : Low RAM (boot, kernel, RT programs)
 *   0x010000 - 0x03FFFF (192K words, 384KB) : Extended RAM (programs, buffers)
 *   0x040000 - 0x05FFFF (128K words, 256KB) : 5MPM (shared multiport memory)
 *   0x060000 - 0xFFFFFF (remaining space)   : Additional RAM (system dependent)
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

	/* Validate ND-100 address range (24-bit physical: 0x000000-0xFFFFFF)
	 * This is 4M words = 8MB byte addressing */
	if (nd100_addr > 0xFFFFFF) {
		printf("[ERROR] ND-100 Bridge: Address 0x%08X exceeds ND-100 physical range (max 0xFFFFFF)\n",
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
 * @param nd100_addr ND-100 physical word address (24-bit, 0x000000-0xFFFFFF)
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

	/* Validate ND-100 address range (24-bit physical: 0x000000-0xFFFFFF) */
	if (nd100_addr > 0xFFFFFF) {
		printf("[ERROR] ND-100 Bridge: Address 0x%08X exceeds ND-100 physical range (max 0xFFFFFF)\n",
		       nd100_addr);
		return;
	}

	/* Translate ND-100 word address to ND-500 byte address */
	uint32_t physical_addr = cpu->nd100_memory_offset + (nd100_addr * 2);

	/* Write halfword to physical memory using existing memory access API
	 * Big-endian byte order (same for both ND-100 and ND-500) */
	nd500_write_memory_16(cpu, physical_addr, data);
}



/* Public wrapper so other translation units (e.g. Ret.c's bogus-domain-return
 * diagnostic) can dump the recent-PC ring. */
void nd500_dump_pc_ring(const char* tag) { nd500_dump_stop_ring(tag); }
