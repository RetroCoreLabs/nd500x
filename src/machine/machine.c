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

/* Set while a run loop OUTSIDE this file owns the CPU - today that is the
 * SINTRAN shell's run_domain, which steps a domain started from the '@'
 * prompt on its own thread. nd500_dbg_run must then NOT spawn run_thread:
 * it only raises run_flag, and the loop that already owns the CPU picks it
 * up. Two threads stepping the same Nd500Cpu executes each instruction
 * twice, which is worse than the race the CPU lock removes. */
static volatile int g_cpu_external_driver = 0;

void nd500_cpu_set_external_driver(int on) { g_cpu_external_driver = on ? 1 : 0; }
int  nd500_cpu_has_external_driver(void)   { return g_cpu_external_driver; }

#ifdef ND500_NATIVE_THREADS
#include <pthread.h>
#include <sched.h>

/* The CPU lock. Whoever advances the CPU holds it while stepping; whoever
 * READS CPU or memory state from another thread (the DAP adapter) takes it
 * for the duration of the read.
 *
 * There is more than one thing that drives the CPU: run_thread below, and
 * the SINTRAN shell's own loops (run_domain and shell_execute_command in
 * src/frontend/nd500x/nd500x_shell.c), which call nd500_cpu_step directly on
 * the shell thread. Without this lock a DAP register or memory read taken
 * while the shell thread is mid-instruction reads a half-updated Nd500Cpu.
 *
 * Recursive on purpose: nd500_dbg_step and the DAP callbacks both take it,
 * and the callbacks call into the step path. */
static pthread_mutex_t g_cpu_mutex;
static pthread_once_t g_cpu_mutex_once = PTHREAD_ONCE_INIT;
static int g_cpu_mutex_ready = 0;

/* Recursion depth of the CURRENT holder. Only ever read or written by the
 * thread that holds the lock, so it needs no protection of its own. It exists
 * because the shell nests: MON 317B UECOM runs shell_execute_command's own
 * step loop from inside run_domain's, so the loop that wants to release the
 * CPU (to block on input) is not always the one that took it. Releasing one
 * level there would leave the outer level held and a DAP read waiting for as
 * long as the guest waits for a line - which is forever, at a prompt. */
static int g_cpu_depth = 0;

static void cpu_mutex_init(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_cpu_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    g_cpu_mutex_ready = 1;
}

void nd500_cpu_lock(void) {
    pthread_once(&g_cpu_mutex_once, cpu_mutex_init);
    pthread_mutex_lock(&g_cpu_mutex);
    g_cpu_depth++;
}

void nd500_cpu_unlock(void) {
    if (!g_cpu_mutex_ready) return;
    g_cpu_depth--;
    pthread_mutex_unlock(&g_cpu_mutex);
}

/* Drop EVERY level this thread holds; returns the count to hand back to
 * nd500_cpu_lock_resume. For a run loop that is about to block. */
int nd500_cpu_lock_suspend(void) {
    if (!g_cpu_mutex_ready) return 0;
    int n = g_cpu_depth;
    g_cpu_depth = 0;
    for (int i = 0; i < n; i++) pthread_mutex_unlock(&g_cpu_mutex);
    return n;
}

void nd500_cpu_lock_resume(int n) {
    if (!g_cpu_mutex_ready || n <= 0) return;
    for (int i = 0; i < n; i++) pthread_mutex_lock(&g_cpu_mutex);
    g_cpu_depth = n;
}

/* Hand the CPU over for a moment: a run loop calls this every few thousand
 * instructions so a waiting DAP read actually gets in. Dropping the lock is
 * not enough on its own - the same thread usually reacquires it before the
 * waiter is scheduled - so yield in between. */
void nd500_cpu_lock_yield(void) {
    int n = nd500_cpu_lock_suspend();
    if (n <= 0) return;
    sched_yield();
    nd500_cpu_lock_resume(n);
}

static void* run_thread(void* arg) {
    Nd500Machine* m = (Nd500Machine*)arg;
    uint32_t batch = 0;
    nd500_cpu_lock();
    while (m->run_flag) {
        /* Execute one instruction - returns false if trap occurred */
        if (m->cpu && !nd500_cpu_step(m->cpu)) {
            /* Trap occurred - stop execution */
            break;
        }
        /* Let a waiting DAP read in every 4K instructions. */
        if ((batch & 0xFFF) == 0) nd500_cpu_lock_yield();
        /* Yield briefly every 64K instructions so other threads
         * (REPL, DAP server) stay responsive without throttling
         * execution speed. */
        if ((++batch & 0xFFFF) == 0) {
            struct timespec ts = {0, 100000}; /* 0.1ms */
            int n = nd500_cpu_lock_suspend();
            nanosleep(&ts, NULL);
            nd500_cpu_lock_resume(n);
        }
    }
    nd500_cpu_unlock();
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
    /* Someone else's loop is already driving; raising the flag is the whole
     * of "continue" for it (see nd500_cpu_set_external_driver above). */
    if (g_cpu_external_driver) return;
    pthread_t t;
    (void)pthread_create(&t, NULL, run_thread, m);
    (void)pthread_detach(t);
}

void nd500_dbg_stop(Nd500Machine* m) {
    if (!m) return;
    m->run_flag = 0;
}
#else
/* WebAssembly: one thread, so there is nothing to serialize. */
void nd500_cpu_lock(void) {}
void nd500_cpu_unlock(void) {}
void nd500_cpu_lock_yield(void) {}
int  nd500_cpu_lock_suspend(void) { return 0; }
void nd500_cpu_lock_resume(int n) { (void)n; }
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

