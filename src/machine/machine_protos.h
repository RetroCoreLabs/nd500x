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

/* Segment loading helpers */
int    nd500_load_file_to_memory(Nd500Machine* m, const char* path, uint32_t base_addr);
int    nd500_load_pseg_file(Nd500Machine* m, const char* path, uint32_t pseg_base_addr);
int    nd500_load_dseg_file(Nd500Machine* m, const char* path, uint32_t dseg_base_addr);
const char* nd500_load_strerror(int error_code, uint32_t attempted_addr, uint32_t mem_size);

/* Disassembly configuration */
int    nd500_dbg_set_show_ea(int onoff);
int    nd500_dbg_get_show_ea(void);
int    nd500_dbg_set_demangle(int onoff);
int    nd500_dbg_get_demangle(void);

/* Trace configuration */
int    nd500_dbg_set_trace_mode(int onoff);
int    nd500_dbg_get_trace_mode(void);
void   nd500_dbg_trace_instruction(uint32_t pc, const char* mnemonic, uint32_t* registers);

/* Profiling configuration */
int    nd500_dbg_set_profiling(int onoff);
int    nd500_dbg_get_profiling(void);
void   nd500_dbg_profile_instruction(const char* mnemonic);
void   nd500_dbg_show_profile(void);
void   nd500_dbg_reset_profile(void);

/* Call stack tracking */
void   nd500_dbg_call_stack_push(uint32_t pc, uint32_t return_addr);
void   nd500_dbg_call_stack_pop(void);
void   nd500_dbg_show_backtrace(void);
void   nd500_dbg_call_stack_reset(void);

/* Invalid instruction trap functions */
int    nd500_dbg_set_trap_invalid(int onoff);
int    nd500_dbg_get_trap_invalid(void);
void   nd500_dbg_toggle_trap_invalid(void);

/* Trap state management */
void   nd500_dbg_clear_traps(void);
int    nd500_dbg_trap_occurred(void);
const char* nd500_dbg_get_trap_description(void);

/* MMU control */
void nd500_machine_enable_mmu(Nd500Machine* m);
void nd500_machine_disable_mmu(Nd500Machine* m);
int nd500_machine_mmu_is_enabled(Nd500Machine* m);

/* Memory Map Visualization */
const char* nd500_dbg_memory_map_json(Nd500Machine* m);
const char* nd500_dbg_memory_map_for_domain_json(Nd500Machine* m, int domain);

