#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "machine_protos.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/nd500_mmu.h"

/* Background run loop for native builds.
 *
 * The guard used to be __unix__, which the MinGW compiler does not define - so
 * a Windows build silently had NO run loop and NO nd500_dbg_run(). Everything
 * up to "[ndix] run" printed normally and then nothing happened at all: the
 * machine never started, so the kernel never got as far as its banner or the
 * login prompt. It looked like a console-output problem and was not.
 *
 * ND500_NATIVE_THREADS is the honest test - "this build has real threads" -
 * and covers Windows and macOS as well as Linux. WebAssembly is the one target
 * that genuinely has none; there the debugger drives stepping itself.
 *
 * pthreads are available everywhere here: winpthreads under MinGW/MSYS2, and
 * libSystem on macOS. CMake links them through Threads::Threads. */
#if !defined(__EMSCRIPTEN__)
#define ND500_NATIVE_THREADS 1
#endif

#ifdef ND500_NATIVE_THREADS
#include <pthread.h>

static void* run_thread(void* arg) {
	Nd500Machine* m = (Nd500Machine*)arg;
	uint32_t batch = 0;
	while (m->run_flag) {
		/* Execute one instruction - returns false if trap occurred */
		if (m->cpu && !nd500_cpu_step(m->cpu)) {
			/* Trap occurred - stop execution */
			break;
		}
		/* Yield briefly every 64K instructions so other threads
		 * (REPL, DAP server) stay responsive without throttling
		 * execution speed. */
		if ((++batch & 0xFFFF) == 0) {
			struct timespec ts = {0, 100000}; /* 0.1ms */
			nanosleep(&ts, NULL);
		}
	}
	return NULL;
}

void nd500_dbg_run(Nd500Machine* m) {
	if (!m) return;
	if (m->run_flag) return;
	/* Allow leaving a breakpoint the CPU is currently parked on */
	if (m->cpu) {
		m->bp_resume_pc = m->cpu->PC;
		m->bp_resume_skip = 1;
	}
	m->run_flag = 1;
	pthread_t t;
	(void)pthread_create(&t, NULL, run_thread, m);
	(void)pthread_detach(t);
}

void nd500_dbg_stop(Nd500Machine* m) {
	if (!m) return;
	m->run_flag = 0;
}
#endif

/* ═══════════════════════════════════════════════════════
 * MMU CONTROL FUNCTIONS
 * ═══════════════════════════════════════════════════════ */

/**
 * Enable MMU address translation
 * When enabled, all memory accesses go through nd500_mmu_translate()
 */
void nd500_machine_enable_mmu(Nd500Machine* m) {
	if (!m) return;
	m->mmu_enabled = 1;
	if (m->cpu) {
		nd500_mmu_enable(m->cpu);
	}
}

/**
 * Disable MMU address translation
 * When disabled, memory accesses use direct physical addressing
 */
void nd500_machine_disable_mmu(Nd500Machine* m) {
	if (!m) return;
	m->mmu_enabled = 0;
	if (m->cpu) {
		nd500_mmu_disable(m->cpu);
	}
}

/**
 * Check if MMU is enabled
 * Returns true if EITHER program or data MMU is enabled
 */
int nd500_machine_mmu_is_enabled(Nd500Machine* m) {
	if (!m || !m->cpu) return 0;
	/* Check the actual CPU MMU state, not the cached flag */
	return nd500_mmu_is_enabled(m->cpu);
}

