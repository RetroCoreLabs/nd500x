/**
 * @file ndbus_runner.h
 * @brief One host thread per ND-5000, and stopping it cleanly.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Each ND-5000 is a separate processor on the MFbus, so each one gets a host
 * thread. They meet only in the shared pool, where the rules of
 * ndbus_pool.h apply: ordinary accesses take no lock, and exactly three things
 * are synchronized - TSET, the doorbell and the TLB shootdown.
 *
 * NATIVE ONLY. Under __EMSCRIPTEN__ there is one WebAssembly memory and no
 * threads, and `ndbus_runner_start()` REFUSES rather than pretending: a runner
 * that silently ran nothing would leave the browser looking like a hung machine.
 * The browser runs one ND-5000 from the main loop, which is what it has always
 * done and what NDIX needs.
 *
 * STOPPING IS THE PART THAT GOES WRONG. A thread that is killed mid-instruction
 * leaves the shared pool in a state no guest could have produced, and a thread
 * that is never joined outlives the pool it is reading. So: the runner is ASKED
 * to stop, it notices between instructions, and the caller JOINS it before
 * anything it touched is freed. There is no cancel and no kill.
 */

#ifndef NDBUS_RUNNER_H
#define NDBUS_RUNNER_H

#include "ndbus_types.h"

/**
 * @brief Run the CPU for one instruction.
 *
 * What the runner needs from a CPU, beyond the NdbusCpuOps vtable: a way to run
 * it.
 *
 * @param ctx The CPU's own context, as carried in NdbusCpuOps.
 * @return true when the CPU should keep running. false when the CPU has stopped
 *         on its own - halted, hit a breakpoint, faulted - and the runner then
 *         exits without being asked. There is no separate error return: a
 *         failure that stops the CPU is reported as false like any other stop.
 */
typedef bool (*NdbusCpuStepFn)(void *ctx);

/** @brief Lifecycle state of a runner thread. */
typedef enum
{
    NDBUS_RUNNER_IDLE = 0,  /**< never started, or joined */
    NDBUS_RUNNER_RUNNING,   /**< the thread is executing instructions */
    NDBUS_RUNNER_STOPPING,  /**< asked to stop, has not exited yet */
    NDBUS_RUNNER_STOPPED    /**< the thread function has returned */
} NdbusRunnerState;

/**
 * @brief How many instructions run between two checks of the stop flag.
 *
 * Checking every instruction puts an atomic load on the hottest path in the
 * emulator for a flag that changes at most twice in a run. Checking too rarely
 * makes shutdown feel hung. A few thousand instructions is well under a
 * millisecond of emulated time and is invisible on the instruction path.
 */
#define NDBUS_RUNNER_STOP_CHECK_INTERVAL 4096u

typedef struct NdbusRunner NdbusRunner;

/**
 * @brief Prepare a runner. Does not start a thread.
 * @param runner The runner to initialise.
 * @param cpu    The CPU vtable; `cpu.name` is required.
 * @param step   The per-instruction step function; required.
 * @param host   Host callbacks for logging; may be NULL.
 * @return true when prepared, false when `step` or `cpu.name` is missing. No
 *         thread exists either way, so a false return needs no cleanup.
 */
bool ndbus_runner_init(NdbusRunner *runner, const NdbusCpuOps *cpu, NdbusCpuStepFn step,
                       const NdbusHostOps *host);

/**
 * @brief Start the thread.
 * @param runner The prepared runner.
 * @return true when the thread was created. false when a thread could not be
 *         created, when the runner is already running, or - always - under
 *         __EMSCRIPTEN__, where there are no threads and refusing is better than
 *         silently running nothing.
 */
bool ndbus_runner_start(NdbusRunner *runner);

/**
 * @brief Ask the runner to stop.
 *
 * Returns immediately; the thread notices between instructions, at most
 * NDBUS_RUNNER_STOP_CHECK_INTERVAL instructions later. Never inside an
 * instruction: there is no cancel and no kill.
 *
 * @param runner The runner to ask.
 * @return Nothing. Safe to call on a runner that is not running, and safe to
 *         call more than once; neither case is an error.
 */
void ndbus_runner_request_stop(NdbusRunner *runner);

/**
 * @brief Wait for the thread to exit and reap it.
 *
 * MUST be called before anything the CPU touches is freed - the pool, the CPU
 * itself, the TLB. Requesting a stop is not enough: the thread may be mid
 * instruction when the request arrives.
 *
 * @param runner The runner to join.
 * @return Nothing. A runner that never started, or that has already been joined,
 *         returns at once rather than reporting an error.
 */
void ndbus_runner_join(NdbusRunner *runner);

/**
 * @brief Ask and wait, which is what a shutdown path almost always wants.
 * @param runner The runner to stop and join.
 * @return Nothing. Cannot fail, for the same reasons as ndbus_runner_join().
 */
void ndbus_runner_stop_and_join(NdbusRunner *runner);

/**
 * @brief The runner's current lifecycle state.
 * @param runner The runner to query.
 * @return One of the NdbusRunnerState values. There is no error value;
 *         NDBUS_RUNNER_IDLE covers "never started, or joined".
 */
NdbusRunnerState ndbus_runner_state(const NdbusRunner *runner);

/**
 * @brief How many instructions this runner executed.
 *
 * Useful for a test to prove a thread ran at all, and for a diagnostic to show
 * one that is stuck.
 *
 * @param runner The runner to query.
 * @return The instruction count. 0 means none ran; there is no error value.
 */
unsigned long long ndbus_runner_instructions(const NdbusRunner *runner);

#ifndef __EMSCRIPTEN__
#include <pthread.h>
#endif

/** @brief One host thread running one ND-5000, plus its stop handshake. */
struct NdbusRunner
{
    NdbusCpuOps        cpu;       /**< the CPU vtable, copied at init */
    NdbusCpuStepFn     step;      /**< the per-instruction step function */
    const NdbusHostOps *host;     /**< host callbacks for logging; may be NULL */

    /** Written by the controlling thread, read by the runner thread. Relaxed on
     * both sides: it only has to arrive, and it is checked repeatedly. */
    volatile unsigned  stop_requested;

    /** Written by the runner thread, read by anyone. One of NdbusRunnerState. */
    volatile unsigned  state;
    unsigned long long instructions; /**< instructions executed so far */

#ifndef __EMSCRIPTEN__
    pthread_t          thread;       /**< the host thread, valid when thread_valid */
    bool               thread_valid; /**< true between a successful start and a join */
#endif
};

#endif /* NDBUS_RUNNER_H */
