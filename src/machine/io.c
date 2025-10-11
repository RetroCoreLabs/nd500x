#include <stdlib.h>
#include <string.h>
#include "machine_protos.h"

void nd500_machine_init(Nd500Machine* m, uint32_t mem_size) {
	if (!m) return;
	m->memory_size = mem_size;
	m->memory = (uint8_t*)calloc(1, mem_size);
	m->run_flag = 0;
}

void nd500_machine_free(Nd500Machine* m) {
	if (!m) return;
	free(m->memory);
	m->memory = NULL;
	m->memory_size = 0;
	m->run_flag = 0;
}

static inline int in_range(Nd500Machine* m, uint32_t addr, uint32_t size) {
	return m && m->memory && (addr + size) <= m->memory_size;
}

uint8_t nd500_bus_read8(Nd500Machine* m, uint32_t addr) {
	if (!in_range(m, addr, 1)) return 0;
	return m->memory[addr];
}

void nd500_bus_write8(Nd500Machine* m, uint32_t addr, uint8_t val) {
	if (!in_range(m, addr, 1)) return;
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


