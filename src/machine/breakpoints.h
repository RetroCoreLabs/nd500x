#pragma once
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

/* Watchpoint types */
typedef enum {
    WP_TYPE_READ  = 1,  /* Break on memory read */
    WP_TYPE_WRITE = 2,  /* Break on memory write */
    WP_TYPE_CHANGE = 3  /* Break on memory value change */
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
    uint32_t address;         /* Memory address to watch */
    uint32_t length;          /* Number of bytes to watch */
    WatchpointType type;
    bool enabled;
    uint32_t last_value;      /* For CHANGE type */
    uint32_t hit_count;
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
int bp_delete(BreakpointManager* mgr, int id);
int bp_enable(BreakpointManager* mgr, int id);
int bp_disable(BreakpointManager* mgr, int id);
void bp_list(BreakpointManager* mgr);
bool bp_should_break_at(BreakpointManager* mgr, uint32_t pc);

/* Watchpoint management */
int wp_add(BreakpointManager* mgr, uint32_t address, uint32_t length, WatchpointType type);
int wp_delete(BreakpointManager* mgr, int id);
int wp_enable(BreakpointManager* mgr, int id);
int wp_disable(BreakpointManager* mgr, int id);
void wp_list(BreakpointManager* mgr);
bool wp_should_break_on_read(BreakpointManager* mgr, uint32_t addr);
bool wp_should_break_on_write(BreakpointManager* mgr, uint32_t addr, uint32_t value);

