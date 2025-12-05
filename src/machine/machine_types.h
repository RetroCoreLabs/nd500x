#pragma once
#include <stdint.h>

struct Nd500Cpu; /* forward */
struct BreakpointManager; /* forward */

typedef struct Nd500Machine {
	uint8_t* memory;
	uint32_t memory_size;
	volatile int run_flag;
	const char* stop_reason; /* Why execution stopped (NULL if step count) */
	struct Nd500Cpu* cpu; /* linked CPU for debug APIs */
	struct BreakpointManager* bp_mgr; /* Breakpoint/watchpoint manager */
	int mmu_enabled; /* MMU address translation enabled */
} Nd500Machine;


