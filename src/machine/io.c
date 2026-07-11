#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "machine_protos.h"
#include "breakpoints.h"
#include "../cpu/nd500_mmu.h"
#include "../libmon/mon_file_table.h"

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

	if (!in_range(m, addr, 1)) return;

	/* Check watchpoints on write */
	if (m->bp_mgr && wp_should_break_on_write(m->bp_mgr, addr, val)) {
		m->run_flag = 0;
		m->stop_reason = STOP_WATCHPOINT_WRITE;
		m->stop_addr = addr;
		m->stop_data = val;
		printf("[STOP] Watchpoint write at 0x%08X (val=0x%02X)\n", addr, val);
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
	m->memory[addr] = (uint8_t)((val >> 8) & 0xFF);
	m->memory[addr + 1] = (uint8_t)(val & 0xFF);
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
	m->memory[addr] = (uint8_t)((val >> 24) & 0xFF);
	m->memory[addr + 1] = (uint8_t)((val >> 16) & 0xFF);
	m->memory[addr + 2] = (uint8_t)((val >> 8) & 0xFF);
	m->memory[addr + 3] = (uint8_t)(val & 0xFF);
}


