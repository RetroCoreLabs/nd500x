/*
 * breakpoints.h - breakpoint and watchpoint tables
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef BREAKPOINTS_H
#define BREAKPOINTS_H
#include <stdint.h>
#include <stdbool.h>

/* Maximum number of breakpoints and watchpoints */
#define MAX_BREAKPOINTS 64
#define MAX_WATCHPOINTS 32

/* Breakpoint types */
typedef enum {
    BP_TYPE_EXECUTE = 1,      /* Break on instruction execution at PC */
    BP_TYPE_CONDITIONAL = 2   /* Break when condition is true */
} BreakpointType;

/* Expression evaluation result */
typedef struct {
    bool valid;
    uint32_t value;
    char error_msg[64];
} ExprResult;

/* Watchpoint types */
typedef enum {
    WP_TYPE_READ  = 1,  /* Break on memory read */
    WP_TYPE_WRITE = 2,  /* Break on memory write */
    WP_TYPE_CHANGE = 3, /* Break on memory value change */
    WP_TYPE_REGISTER = 4 /* Break on register value change */
} WatchpointType;

/* Breakpoint structure */
typedef struct {
    uint32_t address;         /* PC address for execution breakpoint */
    BreakpointType type;
    bool enabled;
    bool one_shot;            /* Delete after first hit */
    uint32_t hit_count;       /* Number of times hit */
    char condition[128];      /* Optional condition expression (for future) */
} Breakpoint;

/* Watchpoint structure */
typedef struct {
    uint32_t address;         /* Memory address to watch (or register index for REGISTER type) */
    uint32_t length;          /* Number of bytes to watch */
    WatchpointType type;
    bool enabled;
    uint32_t last_value;      /* For CHANGE and REGISTER types */
    uint32_t hit_count;
    char register_name[8];     /* Register name for REGISTER type */
} Watchpoint;

/* Breakpoint manager */
typedef struct BreakpointManager {
    Breakpoint breakpoints[MAX_BREAKPOINTS];
    Watchpoint watchpoints[MAX_WATCHPOINTS];
    int bp_count;
    int wp_count;
    bool break_on_next_instruction;  /* For step command */
} BreakpointManager;

/* Initialize breakpoint manager */
void bp_mgr_init(BreakpointManager* mgr);

/* Breakpoint management */
int bp_add(BreakpointManager* mgr, uint32_t address, bool one_shot);
int bp_add_conditional(BreakpointManager* mgr, uint32_t address, const char* condition, bool one_shot);
int bp_delete(BreakpointManager* mgr, int id);
int bp_enable(BreakpointManager* mgr, int id);
int bp_disable(BreakpointManager* mgr, int id);
void bp_list(BreakpointManager* mgr);
bool bp_should_break_at(BreakpointManager* mgr, uint32_t pc);

/* Expression evaluation */
ExprResult bp_evaluate_condition(const char* condition, uint32_t pc, uint32_t* registers, uint32_t* memory);

/* Watchpoint management */
int wp_add(BreakpointManager* mgr, uint32_t address, uint32_t length, WatchpointType type);
int wp_add_register(BreakpointManager* mgr, const char* reg_name, uint32_t reg_index);
int wp_delete(BreakpointManager* mgr, int id);
int wp_enable(BreakpointManager* mgr, int id);
int wp_disable(BreakpointManager* mgr, int id);
void wp_list(BreakpointManager* mgr);
bool wp_should_break_on_read(BreakpointManager* mgr, uint32_t addr);
bool wp_should_break_on_write(BreakpointManager* mgr, uint32_t addr, uint32_t value);
bool wp_should_break_on_register_change(BreakpointManager* mgr, uint32_t reg_index, uint32_t value);

/* Check all register watchpoints against the current register values.
 * regs[] is indexed by the register-watch index convention:
 * 0=PC, 1-4=I1-I4, 5=L, 6=B, 7=R.
 * Returns the watchpoint id that fired (change detected), or -1. */
#define WP_REG_INDEX_COUNT 8
int wp_check_registers(BreakpointManager* mgr, const uint32_t regs[WP_REG_INDEX_COUNT]);

/* Map a register name (PC, I1-I4, L, B, R) to its watch index, -1 if not watchable */
int wp_register_index_for_name(const char* name);


#endif /* BREAKPOINTS_H */
