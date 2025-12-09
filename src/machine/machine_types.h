#pragma once
#include <stdint.h>

struct Nd500Cpu; /* forward */
struct BreakpointManager; /* forward */

/* Why execution stopped */
typedef enum {
	STOP_NONE = 0,
	STOP_USER_REQUESTED,
	STOP_BREAKPOINT,
	STOP_WATCHPOINT_READ,
	STOP_WATCHPOINT_WRITE,
	STOP_TRAP_PAGE_FAULT,
	STOP_TRAP_PROTECTION_VIOLATION,
	STOP_TRAP_ILLEGAL_INSTRUCTION,
	STOP_TRAP_ILLEGAL_OPERAND,
	STOP_TRAP_DIVIDE_BY_ZERO,
	STOP_TRAP_FLOATING_OVERFLOW,
	STOP_TRAP_FLOATING_UNDERFLOW,
	STOP_TRAP_INVALID_OPERATION,
	STOP_TRAP_STACK_OVERFLOW,
	STOP_TRAP_STACK_UNDERFLOW,
	STOP_TRAP_INTEGER_OVERFLOW,
	STOP_TRAP_OTHER,
	STOP_INVALID_INSTRUCTION_00,
	STOP_MON_HALT,
	STOP_MON_UNIMPLEMENTED,
} StopReason;

typedef struct Nd500Machine {
	uint8_t* memory;
	uint32_t memory_size;
	volatile int run_flag;
	StopReason stop_reason;
	uint32_t stop_addr;      /* PC or data address where stop occurred */
	uint32_t stop_data;      /* Additional context (e.g. MON number) */
	struct Nd500Cpu* cpu; /* linked CPU for debug APIs */
	struct BreakpointManager* bp_mgr; /* Breakpoint/watchpoint manager */
	int mmu_enabled; /* MMU address translation enabled */
} Nd500Machine;

/* Get human-readable stop reason string */
const char* nd500_stop_reason_str(StopReason reason);


