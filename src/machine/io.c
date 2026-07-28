#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "machine_protos.h"
#include "breakpoints.h"
#include "../cpu/nd500_mmu.h"
#include <ndmon/mon_file_table.h>

/* ------------------------------------------------------------------ *
 * PTE write-watch (env ND500X_PTEWATCH). Diagnostic only: logs every
 * physical write that either (a) targets the watched L2 page-table page
 * [ND500X_PTEWATCH_PAGE, default 0x48A000, one 2KB page] or (b) carries the
 * watched page-frame number in its low 30 bits [ND500X_PTEWATCH_PFN, default
 * 0x091d]. Used to find WHERE the NDIX kernel's user-stack PTE write actually
 * lands vs the emulator's PST-derived L2 slot. Physical addresses only.
 * ------------------------------------------------------------------ */
static int      ptewatch_on   = -1;
static uint32_t ptewatch_page = 0x48A000u;
static uint32_t ptewatch_page2 = 0xFFFFFFFFu;   /* second watched page (optional) */
static uint32_t ptewatch_pfn  = 0x091du;
static unsigned long ptewatch_seq = 0;
static void ptewatch_init(void) {
	if (ptewatch_on >= 0) return;
	const char* e = getenv("ND500X_PTEWATCH");
	ptewatch_on = (e && e[0] && e[0] != '0') ? 1 : 0;
	const char* pg = getenv("ND500X_PTEWATCH_PAGE");
	if (pg && pg[0]) ptewatch_page = (uint32_t)strtoul(pg, NULL, 0);
	const char* pg2 = getenv("ND500X_PTEWATCH_PAGE2");
	if (pg2 && pg2[0]) ptewatch_page2 = (uint32_t)strtoul(pg2, NULL, 0);
	const char* pf = getenv("ND500X_PTEWATCH_PFN");
	if (pf && pf[0]) ptewatch_pfn = (uint32_t)strtoul(pf, NULL, 0);
}
static int ptewatch_in_page(uint32_t addr) {
	if (addr >= ptewatch_page && addr < ptewatch_page + 0x800u) return 1;
	if (ptewatch_page2 != 0xFFFFFFFFu && addr >= ptewatch_page2 && addr < ptewatch_page2 + 0x800u) return 2;
	return 0;
}
/* PC-aware write watch: logs any CPU write (any width) to a watched page, with
 * the instruction PC and a global ordering sequence number, so the timeline of
 * writes to two page-table entries can be compared in a single run. */
void nd500_ptewatch_wr(uint32_t pc, uint32_t vaddr, uint32_t paddr, uint32_t value, int size) {
	ptewatch_init();
	if (!ptewatch_on) return;
	int p = ptewatch_in_page(paddr);
	int has_pfn = ((value & 0x3FFFFFFFu) == ptewatch_pfn);
	if (p || has_pfn)
		fprintf(stderr, "[PTEWR #%lu] PC=0x%08X w%d vaddr=0x%08X paddr=0x%08X value=0x%08X%s%s\n",
		        ptewatch_seq++, pc, size, vaddr, paddr, value,
		        p == 1 ? " [PG1]" : p == 2 ? " [PG2]" : "", has_pfn ? " [PFN]" : "");
}
void nd500_ptewatch32(uint32_t addr, uint32_t val) {
	ptewatch_init();
	if (!ptewatch_on) return;
	int in_page = (addr >= ptewatch_page && addr < ptewatch_page + 0x800u);
	int has_pfn = ((val & 0x3FFFFFFFu) == ptewatch_pfn);
	if (in_page || has_pfn)
		fprintf(stderr, "[PTEWATCH] w32 phys=0x%08X val=0x%08X%s%s\n", addr, val,
		        in_page ? " [L2-PAGE]" : "", has_pfn ? " [PFN]" : "");
}
void nd500_ptewatch8(uint32_t addr, uint8_t val) {
	ptewatch_init();
	if (!ptewatch_on) return;
	if (addr >= ptewatch_page && addr < ptewatch_page + 0x800u)
		fprintf(stderr, "[PTEWATCH] w8  phys=0x%08X val=0x%02X [L2-PAGE]\n", addr, val);
}
/* CPU-store-level watch: called from nd500_write_memory_32 with the full
 * virtual addr, translated physical addr, and 32-bit value. */
void nd500_ptewatch_store(uint32_t vaddr, uint32_t paddr, uint32_t value) {
	ptewatch_init();
	if (!ptewatch_on) return;
	int in_page = (paddr >= ptewatch_page && paddr < ptewatch_page + 0x800u);
	int has_pfn = ((value & 0x3FFFFFFFu) == ptewatch_pfn);
	if (in_page || has_pfn)
		fprintf(stderr, "[PTEWATCH] store vaddr=0x%08X paddr=0x%08X value=0x%08X%s%s\n",
		        vaddr, paddr, value, in_page ? " [L2-PAGE]" : "", has_pfn ? " [PFN]" : "");
}
void nd500_ptewatch_read(uint32_t vaddr, uint32_t paddr, uint32_t value) {
	ptewatch_init();
	if (!ptewatch_on) return;
	if ((value & 0x3FFFFFFFu) == ptewatch_pfn)
		fprintf(stderr, "[PTEWATCH] READ  vaddr=0x%08X paddr=0x%08X value=0x%08X [PFN]\n",
		        vaddr, paddr, value);
}

/* Convert stop reason enum to string */
const char* nd500_stop_reason_str(StopReason reason) {
	switch (reason) {
		case STOP_NONE:                     return "none";
		case STOP_USER_REQUESTED:           return "user requested";
		case STOP_BREAKPOINT:               return "breakpoint";
		case STOP_WATCHPOINT_READ:          return "watchpoint (read)";
		case STOP_WATCHPOINT_WRITE:         return "watchpoint (write)";
		case STOP_WATCHPOINT_REGISTER:      return "watchpoint (register)";
		case STOP_TRAP_PAGE_FAULT:          return "page fault";
		case STOP_TRAP_PROTECTION_VIOLATION: return "protection violation";
		case STOP_TRAP_ILLEGAL_INSTRUCTION: return "illegal instruction";
		case STOP_TRAP_ILLEGAL_OPERAND:     return "illegal operand";
		case STOP_TRAP_DIVIDE_BY_ZERO:      return "divide by zero";
		case STOP_TRAP_FLOATING_OVERFLOW:   return "floating overflow";
		case STOP_TRAP_FLOATING_UNDERFLOW:  return "floating underflow";
		case STOP_TRAP_INVALID_OPERATION:   return "invalid floating operation";
		case STOP_TRAP_STACK_OVERFLOW:      return "stack overflow";
		case STOP_TRAP_STACK_UNDERFLOW:     return "stack underflow";
		case STOP_TRAP_INTEGER_OVERFLOW:    return "integer overflow";
		case STOP_TRAP_OTHER:               return "trap";
		case STOP_INVALID_INSTRUCTION_00:   return "invalid instruction 0x00";
		case STOP_MON_HALT:                 return "MON halt";
		case STOP_MON_UNIMPLEMENTED:        return "unimplemented MON";
		case STOP_WAIT_INPUT:               return "waiting for input";
		default:                            return "unknown";
	}
}

void nd500_machine_init(Nd500Machine* m, uint32_t mem_size) {
	if (!m) return;
	memset(m, 0, sizeof(*m));  /* Zero all fields first */
	m->memory_size = mem_size;
	m->memory = (uint8_t*)calloc(1, mem_size);
	m->run_flag = 0;
	m->stop_reason = STOP_NONE;
	m->stop_addr = 0;
	m->stop_data = 0;
	m->mmu_enabled = 0;  /* MMU starts disabled */

	/* Initialize breakpoint manager */
	m->bp_mgr = (BreakpointManager*)calloc(1, sizeof(BreakpointManager));
	if (m->bp_mgr) {
		bp_mgr_init(m->bp_mgr);
	}

	/* Initialize file system tables (for MON 50/43/122/123 etc) */
	mon_file_table_init();
}

void nd500_machine_free(Nd500Machine* m) {
	if (!m) return;
	free(m->memory);
	m->memory = NULL;
	m->memory_size = 0;
	m->run_flag = 0;

	/* Free breakpoint manager */
	if (m->bp_mgr) {
		free(m->bp_mgr);
		m->bp_mgr = NULL;
	}

	/* Reset file system tables */
	mon_file_table_reset();
}

static inline int in_range(Nd500Machine* m, uint32_t addr, uint32_t size) {
	return m && m->memory && (addr + size) <= m->memory_size;
}

uint8_t nd500_bus_read8(Nd500Machine* m, uint32_t addr) {
	/*
	 * NOTE: MMU translation for CPU-initiated accesses should happen BEFORE
	 * calling this function (in the CPU instruction implementations).
	 * This function provides raw physical memory access used by:
	 * - CPU after address translation
	 * - MMU for reading PTEs (nd500_mmu_read_pte uses physical addresses)
	 * - Debugger for direct memory inspection
	 */

	if (!in_range(m, addr, 1)) return 0;

	/* Check watchpoints on read */
	if (m->bp_mgr && wp_should_break_on_read(m->bp_mgr, addr)) {
		m->run_flag = 0;
		m->stop_reason = STOP_WATCHPOINT_READ;
		m->stop_addr = addr;
		printf("[STOP] Watchpoint read at 0x%08X\n", addr);
	}

	return m->memory[addr];
}

void nd500_bus_write8(Nd500Machine* m, uint32_t addr, uint8_t val) {
	/*
	 * NOTE: MMU translation for CPU-initiated accesses should happen BEFORE
	 * calling this function (in the CPU instruction implementations).
	 * This function provides raw physical memory access used by:
	 * - CPU after address translation
	 * - MMU for writing PTEs (if needed)
	 * - Debugger for direct memory modification
	 */

	if (!in_range(m, addr, 1)) {
		/* Out-of-range physical writes were dropped SILENTLY, which hid a
		 * class of translation bugs (a bogus PTE sends a kernel store past
		 * memory_size and the data just vanishes - reads then return 0).
		 * Log the first few drops (env ND500X_DROPDBG). */
		static int dropd = -1;
		if (dropd < 0) { const char* e = getenv("ND500X_DROPDBG"); dropd = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dropd) {
			static unsigned n = 0;
			if (n++ < 40)
				fprintf(stderr, "[DROP] w8 phys=0x%08X val=0x%02X beyond memory_size=0x%X\n",
				        addr, val, m ? m->memory_size : 0);
		}
		return;
	}

	/* DIT1DBG: watch writes to the domain-1 DIT (DITBASE=0x90000, +256..+512 =
	 * domain-1 program+data capabilities). newproc must populate these when it
	 * creates proc1/init; if NOTHING writes here, proc1 was never created (the
	 * NDIX-boot 0x844 idle regression: proc1 never scheduled). Env-gated. */
	{
		static int dit1 = -1;
		if (dit1 < 0) { const char* e = getenv("ND500X_DIT1DBG"); dit1 = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dit1 && addr >= 0x90100u && addr < 0x90200u && val != 0) {
			static unsigned n = 0;
			if (n++ < 128)
				fprintf(stderr, "[DIT1] NONZERO write DITBASE+dom1 off=0x%X (addr=0x%08X) = 0x%02X\n",
				        addr - 0x90000u, addr, val);
		}
	}

	/* L2WATCH: watch writes to the init process's data/stack L2 page-table pages
	 * (PST[47] data L2 @0x489000, seg-1 stack L2 @0x48A000) - to see whether/where
	 * the guest kernel installs the data-page-0 ("/etc/init") and stack PTEs that
	 * the seg-30/_Udata (0xF0000000) and seg-1 (0x08000014) walks read as empty. */
	{
		static int l2w = -1;
		if (l2w < 0) { const char* e = getenv("ND500X_L2WATCH"); l2w = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (l2w && ((addr >= 0x489000u && addr < 0x489020u) || (addr >= 0x48A000u && addr < 0x48A020u))) {
			/* Zero-clears are noise (the 2KB page wipe): sample the first few.
			 * NONZERO writes are the PTE installs we hunt: log them all. */
			static unsigned nz = 0, z = 0;
			if (val != 0) {
				if (nz++ < 200)
					fprintf(stderr, "[L2WATCH] NONZERO write phys=0x%08X = 0x%02X\n", addr, val);
			} else if (z++ < 8) {
				fprintf(stderr, "[L2WATCH] zero write phys=0x%08X\n", addr);
			}
		}
	}

	/* Check watchpoints on write */
	if (m->bp_mgr && wp_should_break_on_write(m->bp_mgr, addr, val)) {
		m->run_flag = 0;
		m->stop_reason = STOP_WATCHPOINT_WRITE;
		m->stop_addr = addr;
		m->stop_data = val;
		printf("[STOP] Watchpoint write at 0x%08X (val=0x%02X)\n", addr, val);
	}

	nd500_ptewatch8(addr, val);
	/* DCODEDBG (env ND500X_DCODEDBG): detect the "/etc" byte sequence being
	 * written to CONSECUTIVE physical addresses through ANY path (CPU store,
	 * word/halfword funnel, fecall DMA) - finds where init's dcode
	 * ("/etc/init") physically lands during the boot. */
	{
		static int dcd = -1;
		if (dcd < 0) { const char* e = getenv("ND500X_DCODEDBG"); dcd = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dcd) {
			static uint32_t match_addr = 0; static int match_len = 0;
			static const uint8_t pat[4] = { 0x2F, 0x65, 0x74, 0x63 }; /* "/etc" */
			if (val == pat[match_len] && (match_len == 0 || addr == match_addr + 1)) {
				match_addr = addr; match_len++;
				if (match_len == 4) {
					static unsigned hits = 0;
					if (hits++ < 40)
						fprintf(stderr, "[DCODEDBG] \"/etc\" written ending at phys=0x%08X\n", addr);
					match_len = 0;
				}
			} else {
				match_len = (val == pat[0]) ? 1 : 0;
				match_addr = addr;
			}
		}
	}
	/* Caller-capture for the specific Usrptmap[7] corruption byte (env
	 * ND500X_PTECATCH=<hexaddr>): print the C return address of whoever writes
	 * this exact physical byte, so a non-CPU (emulator-internal) writer can be
	 * identified and resolved with addr2line. */
	{
		static long catch_addr = -2;
		if (catch_addr == -2) { const char* e = getenv("ND500X_PTECATCH");
			catch_addr = (e && e[0]) ? (long)strtoul(e, NULL, 0) : -1; }
		if (catch_addr >= 0 && addr == (uint32_t)catch_addr) {
			long delta = (char*)__builtin_return_address(0) - (char*)&nd500_bus_write8;
			fprintf(stderr, "[PTECATCH] write phys=0x%08X val=0x%02X ret=%p delta_from_bus_write8=0x%lx\n",
			        addr, val, __builtin_return_address(0), delta);
		}
	}
	m->memory[addr] = val;
}

uint16_t nd500_bus_read16(Nd500Machine* m, uint32_t addr) {
	uint16_t v = 0;
	if (!in_range(m, addr, 2)) return 0;
	v = ((uint16_t)m->memory[addr] << 8) | (uint16_t)m->memory[addr + 1];
	return v;
}

void nd500_bus_write16(Nd500Machine* m, uint32_t addr, uint16_t val) {
	if (!in_range(m, addr, 2)) return;
	/* Funnel through bus_write8 so the physical write-watches (L2WATCH,
	 * ptewatch, PTECATCH, debugger watchpoints) see EVERY physical store,
	 * including the cpu_instr.c mmu_write16 path and fecall DMA which
	 * previously bypassed them. Byte order identical (big-endian). */
	nd500_bus_write8(m, addr,     (uint8_t)((val >> 8) & 0xFF));
	nd500_bus_write8(m, addr + 1, (uint8_t)(val & 0xFF));
}

uint32_t nd500_bus_read32(Nd500Machine* m, uint32_t addr) {
	uint32_t v = 0;
	if (!in_range(m, addr, 4)) return 0;
	v = ((uint32_t)m->memory[addr] << 24) |
		((uint32_t)m->memory[addr + 1] << 16) |
		((uint32_t)m->memory[addr + 2] << 8) |
		(uint32_t)m->memory[addr + 3];
	return v;
}

void nd500_bus_write32(Nd500Machine* m, uint32_t addr, uint32_t val) {
	if (!in_range(m, addr, 4)) return;
	nd500_ptewatch32(addr, val);
	/* Funnel through bus_write8 (see bus_write16) so all physical watches
	 * cover this path too (mmu_write32 + DMA previously bypassed them). */
	nd500_bus_write8(m, addr,     (uint8_t)((val >> 24) & 0xFF));
	nd500_bus_write8(m, addr + 1, (uint8_t)((val >> 16) & 0xFF));
	nd500_bus_write8(m, addr + 2, (uint8_t)((val >> 8) & 0xFF));
	nd500_bus_write8(m, addr + 3, (uint8_t)(val & 0xFF));
}


