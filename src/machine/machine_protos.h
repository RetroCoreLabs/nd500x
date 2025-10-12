#pragma once
#include <stdint.h>
#include <stddef.h>
#include "machine_types.h"
#include "../cpu/cpu_protos.h"

void nd500_machine_init(Nd500Machine* m, uint32_t mem_size);
void nd500_machine_free(Nd500Machine* m);

uint8_t  nd500_bus_read8 (Nd500Machine* m, uint32_t addr);
void     nd500_bus_write8(Nd500Machine* m, uint32_t addr, uint8_t val);
uint16_t nd500_bus_read16(Nd500Machine* m, uint32_t addr);
void     nd500_bus_write16(Nd500Machine* m, uint32_t addr, uint16_t val);
uint32_t nd500_bus_read32(Nd500Machine* m, uint32_t addr);
void     nd500_bus_write32(Nd500Machine* m, uint32_t addr, uint32_t val);

/* Unified debugger API (initial placeholders) */
size_t nd500_dbg_mem_dump(Nd500Machine* m, uint32_t addr, uint32_t len, uint8_t* out, size_t out_cap);
void nd500_dbg_disasm_print(Nd500Machine* m, uint32_t addr, uint32_t len);
size_t nd500_dbg_disasm  (Nd500Machine* m, uint32_t addr, uint32_t len, char* out, size_t out_cap);
void   nd500_dbg_step    (Nd500Machine* m, uint32_t count);
void   nd500_dbg_run     (Nd500Machine* m);
void   nd500_dbg_stop    (Nd500Machine* m);
int    nd500_dbg_is_running(Nd500Machine* m);
int    nd500_dbg_load_aout_file(Nd500Machine* m, const char* path, uint32_t* out_entry_pc);
int    nd500_dbg_load_aout_buffer(Nd500Machine* m, const uint8_t* data, size_t size, uint32_t* out_entry_pc);
void   nd500_dbg_regs(struct Nd500Cpu* cpu, Nd500Regs* out_regs);


