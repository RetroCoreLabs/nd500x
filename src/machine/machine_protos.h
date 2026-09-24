/*
 * machine_protos.h - machine module prototypes: memory bus, loaders, debug API
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef MACHINE_PROTOS_H
#define MACHINE_PROTOS_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "machine_types.h"
#include "../cpu/cpu_protos.h"

void nd500_machine_init(Nd500Machine* m, uint32_t mem_size);
void nd500_machine_init_shared(Nd500Machine* m, uint8_t* memory, uint32_t mem_size);
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

/* CPU lock: held by whichever thread is advancing the CPU (the background
 * run_thread, or the SINTRAN shell's own run loops), and taken by the DAP
 * adapter around every read of CPU/memory state. Recursive.
 * nd500_cpu_lock_yield() drops it, yields, and retakes it - what a run loop
 * calls periodically so a waiting reader gets a turn. No-ops under WASM. */
void   nd500_cpu_lock(void);
void   nd500_cpu_unlock(void);
void   nd500_cpu_lock_yield(void);
/* Release every level this thread holds (returns the count) and put them
 * back. For a run loop about to block for an unbounded time. */
int    nd500_cpu_lock_suspend(void);
void   nd500_cpu_lock_resume(int levels);

/* "A loop outside machine.c is driving the CPU" (the SINTRAN shell's
 * run_domain). While set, nd500_dbg_run raises run_flag but does not spawn
 * its own run thread, so a DAP continue resumes THAT loop. */
void   nd500_cpu_set_external_driver(int on);
int    nd500_cpu_has_external_driver(void);
int    nd500_dbg_is_running(Nd500Machine* m);
int    nd500_dbg_load_aout_file(Nd500Machine* m, const char* path, uint32_t* out_entry_pc);
int    nd500_dbg_load_aout_buffer(Nd500Machine* m, const uint8_t* data, size_t size, uint32_t* out_entry_pc);
void   nd500_dbg_regs(struct Nd500Cpu* cpu, Nd500Regs* out_regs);

/* Register access by name (shared by CLI 'set' command and DAP adapter) */
uint32_t* nd500_dbg_reg_ptr(struct Nd500Cpu* cpu, const char* name);
int    nd500_dbg_reg_get_by_name(struct Nd500Cpu* cpu, const char* name, uint32_t* out);
int    nd500_dbg_reg_set_by_name(struct Nd500Cpu* cpu, const char* name, uint32_t value);
int    nd500_dbg_reg_count(void);
const char* nd500_dbg_reg_name(int index);

/* Side-effect-free memory access for debugger use (no watchpoint triggers) */
size_t nd500_dbg_mem_read_raw(Nd500Machine* m, uint32_t addr, uint32_t len, uint8_t* out, size_t out_cap);
size_t nd500_dbg_mem_write_raw(Nd500Machine* m, uint32_t addr, const uint8_t* data, uint32_t len);

/* Segment loading helpers */
int    nd500_load_file_to_memory(Nd500Machine* m, const char* path, uint32_t base_addr);
int    nd500_load_pseg_file(Nd500Machine* m, const char* path, uint32_t pseg_base_addr);
int    nd500_load_dseg_file(Nd500Machine* m, const char* path, uint32_t dseg_base_addr);
const char* nd500_load_strerror(int error_code, uint32_t attempted_addr, uint32_t mem_size);

/* Disassembly configuration */
int    nd500_dbg_set_show_ea(int onoff);
int    nd500_dbg_get_show_ea(void);
int    nd500_dbg_set_show_hex(int onoff);
int    nd500_dbg_get_show_hex(void);
int    nd500_dbg_set_radix(int mode);   /* 0=decimal, 1=hex, 2=octal */
int    nd500_dbg_get_radix(void);
int    nd500_dbg_get_radix_base(void);  /* returns 10, 16, or 8 */
int    nd500_dbg_set_demangle(int onoff);
int    nd500_dbg_get_demangle(void);
int    nd500_dbg_set_show_source(int mode);  /* 0=off, 1=asm, 2=c, 3=both */
int    nd500_dbg_get_show_source(void);

/* Trace configuration */
int    nd500_dbg_set_trace_mode(int onoff);
int    nd500_dbg_get_trace_mode(void);

/* Trace file output (NULL = stdout) */
int    nd500_dbg_set_trace_file(const char* path);
int    nd500_dbg_set_trace_file_ex(const char* path, int append);
void   nd500_dbg_close_trace_file(void);
FILE*  nd500_dbg_get_trace_file(void);
void   nd500_dbg_flush_console_output(void);

/* Two-phase trace API for register change tracking
 * mnemonic: instruction mnemonic (e.g., "call", "entd")
 * instr_bytes: raw instruction bytes from fi.bytes[]
 * instr_len: total instruction length */
void   nd500_dbg_trace_before(uint32_t pc, const char* mnemonic,
                              const uint8_t* instr_bytes, int instr_len,
                              uint32_t* before_regs);
void   nd500_dbg_trace_after(uint32_t* before_regs, uint32_t* after_regs);

/* TRACE macro: only outputs if trace mode is enabled */
#define TRACE(...) do { if (nd500_dbg_get_trace_mode()) printf(__VA_ARGS__); } while(0)

/* Memory trace configuration */
#define MEMTRACE_OFF   0
#define MEMTRACE_READ  1
#define MEMTRACE_WRITE 2
#define MEMTRACE_ALL   (MEMTRACE_READ | MEMTRACE_WRITE)

int    nd500_dbg_set_memtrace(int flags);
int    nd500_dbg_get_memtrace(void);

/* MEMTRACE macros: only output if respective memtrace flag is set */
#define MEMTRACE_RD(...) do { if (nd500_dbg_get_memtrace() & MEMTRACE_READ) printf(__VA_ARGS__); } while(0)
#define MEMTRACE_WR(...) do { if (nd500_dbg_get_memtrace() & MEMTRACE_WRITE) printf(__VA_ARGS__); } while(0)

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

/* Breakpoint helper functions */
#include <stdbool.h>
bool   nd500_dbg_has_breakpoint_at(Nd500Machine* m, uint32_t addr);

/* MMU control */
void nd500_machine_enable_mmu(Nd500Machine* m);
void nd500_machine_disable_mmu(Nd500Machine* m);
int nd500_machine_mmu_is_enabled(Nd500Machine* m);

/* MMU logging levels */
#define MMU_LOG_OFF    0
#define MMU_LOG_ERRORS 1
#define MMU_LOG_TRACE  2
#define MMU_LOG_ALL    3

int nd500_dbg_set_mmu_log_level(int level);
int nd500_dbg_get_mmu_log_level(void);

/* Memory Map Visualization */
const char* nd500_dbg_memory_map_json(Nd500Machine* m);
const char* nd500_dbg_memory_map_for_domain_json(Nd500Machine* m, int domain);


/**
 * @brief Page-table write watch: log a CPU write of any width that lands in a
 *        watched physical page, or whose value carries the watched page-frame
 *        number, with the instruction PC and a global sequence number.
 *
 * Diagnostic only; does nothing unless the ptewatch setting is on.
 *
 * @param pc     PC of the writing instruction.
 * @param vaddr  Virtual address written.
 * @param paddr  Translated physical address.
 * @param value  Value written.
 * @param size   Width in bits (8, 16 or 32).
 */
void nd500_ptewatch_wr(uint32_t pc, uint32_t vaddr, uint32_t paddr, uint32_t value, int size);

/**
 * @brief Page-table watch for 32-bit CPU stores, called from
 *        nd500_write_memory_32. Diagnostic only; off unless ptewatch is on.
 *
 * @param vaddr  Virtual address written.
 * @param paddr  Translated physical address.
 * @param value  Value written.
 */
void nd500_ptewatch_store(uint32_t vaddr, uint32_t paddr, uint32_t value);

/**
 * @brief Page-table watch for reads: log a read whose value carries the
 *        watched page-frame number. Diagnostic only; off unless ptewatch is on.
 *
 * @param vaddr  Virtual address read.
 * @param paddr  Translated physical address.
 * @param value  Value read.
 */
void nd500_ptewatch_read(uint32_t vaddr, uint32_t paddr, uint32_t value);

#endif /* MACHINE_PROTOS_H */
