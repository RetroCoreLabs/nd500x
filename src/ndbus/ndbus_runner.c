/*
 * ndbus_runner.c - one host thread per ND-5000
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdio.h>
#include <string.h>

#include "ndbus_runner.h"

static void runner_log(const NdbusRunner *runner, const char *message)
{
    if (runner == NULL || runner->host == NULL || runner->host->log == NULL)
    {
        return;
    }
    runner->host->log(runner->host->ctx, 0, message);
}

static void set_state(NdbusRunner *runner, NdbusRunnerState state)
{
    __atomic_store_n(&runner->state, (unsigned)state, __ATOMIC_RELEASE);
}

NdbusRunnerState ndbus_runner_state(const NdbusRunner *runner)
{
    if (runner == NULL)
    {
        return NDBUS_RUNNER_IDLE;
    }
    return (NdbusRunnerState)__atomic_load_n(&runner->state, __ATOMIC_ACQUIRE);
}

unsigned long long ndbus_runner_instructions(const NdbusRunner *runner)
{
    if (runner == NULL)
    {
        return 0;
    }
    /* Relaxed: a diagnostic counter. A reader that catches it mid-update sees a
     * slightly stale count, which is what a counter is for. */
    return __atomic_load_n(&runner->instructions, __ATOMIC_RELAXED);
}

bool ndbus_runner_init(NdbusRunner *runner, const NdbusCpuOps *cpu, NdbusCpuStepFn step,
                       const NdbusHostOps *host)
{
    if (runner == NULL || cpu == NULL || step == NULL || cpu->name == NULL)
    {
        return false;
    }
    memset(runner, 0, sizeof(*runner));
    runner->cpu = *cpu;
    runner->step = step;
    runner->host = host;
    runner->state = (unsigned)NDBUS_RUNNER_IDLE;
    return true;
}

void ndbus_runner_request_stop(NdbusRunner *runner)
{
    if (runner == NULL)
    {
        return;
    }
    __atomic_store_n(&runner->stop_requested, 1u, __ATOMIC_RELAXED);
    /* Only the RUNNING state becomes STOPPING. A runner that already exited on
     * its own must not be dragged back out of STOPPED. */
    unsigned expected = (unsigned)NDBUS_RUNNER_RUNNING;
    (void)__atomic_compare_exchange_n(&runner->state, &expected, (unsigned)NDBUS_RUNNER_STOPPING,
                                      false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

#ifndef __EMSCRIPTEN__

static void *runner_thread(void *arg)
{
    NdbusRunner *runner = (NdbusRunner *)arg;
    unsigned since_check = 0;

    for (;;)
    {
        /*
         * The stop flag is checked BETWEEN instructions, never inside one. A
         * thread stopped mid-instruction would leave the shared pool in a state
         * no guest could have produced - half a multi-byte write, a semaphore
         * taken and not released.
         *
         * Once every NDBUS_RUNNER_STOP_CHECK_INTERVAL instructions, because an
         * atomic load on every instruction is a cost on the emulator's hottest
         * path for a flag that changes at most twice in a run.
         */
        if (++since_check >= NDBUS_RUNNER_STOP_CHECK_INTERVAL)
        {
            since_check = 0;
            if (__atomic_load_n(&runner->stop_requested, __ATOMIC_RELAXED) != 0u)
            {
                break;
            }
        }

        if (!runner->step(runner->cpu.ctx))
        {
            /* The CPU stopped on its own - halted, breakpoint, fault. Not an
             * error, and not something to report as one. */
            break;
        }

        __atomic_fetch_add(&runner->instructions, 1ull, __ATOMIC_RELAXED);
    }

    set_state(runner, NDBUS_RUNNER_STOPPED);
    return NULL;
}

bool ndbus_runner_start(NdbusRunner *runner)
{
    if (runner == NULL || runner->step == NULL)
    {
        return false;
    }

    NdbusRunnerState state = ndbus_runner_state(runner);
    if (state == NDBUS_RUNNER_RUNNING || state == NDBUS_RUNNER_STOPPING)
    {
        runner_log(runner, "ndbus runner: already running");
        return false;
    }

    /* A runner that ran before must be JOINED before it runs again, or its old
     * thread is never reaped. */
    if (runner->thread_valid)
    {
        runner_log(runner, "ndbus runner: join the previous run before starting another");
        return false;
    }

    runner->stop_requested = 0;
    set_state(runner, NDBUS_RUNNER_RUNNING);

    if (pthread_create(&runner->thread, NULL, runner_thread, runner) != 0)
    {
        set_state(runner, NDBUS_RUNNER_IDLE);
        runner_log(runner, "ndbus runner: could not create the host thread");
        return false;
    }
    runner->thread_valid = true;
    return true;
}

void ndbus_runner_join(NdbusRunner *runner)
{
    if (runner == NULL || !runner->thread_valid)
    {
        return;
    }
    (void)pthread_join(runner->thread, NULL);
    runner->thread_valid = false;
    set_state(runner, NDBUS_RUNNER_IDLE);
}

unsigned ndbus_runner_pump(NdbusRunner *runner, unsigned max_steps)
{
    /* The runner thread executes the CPU; there is nothing to do here. */
    (void)runner;
    (void)max_steps;
    return 0;
}

#else /* __EMSCRIPTEN__ */

bool ndbus_runner_start(NdbusRunner *runner)
{
    if (runner == NULL || runner->step == NULL)
    {
        return false;
    }

    NdbusRunnerState state = ndbus_runner_state(runner);
    if (state == NDBUS_RUNNER_RUNNING || state == NDBUS_RUNNER_STOPPING)
    {
        runner_log(runner, "ndbus runner: already running");
        return false;
    }

    /* Same rule as the threaded build: a run that ended must be joined before
     * the next one starts. */
    if (runner->pumped)
    {
        runner_log(runner, "ndbus runner: join the previous run before starting another");
        return false;
    }

    /* One WebAssembly memory, no threads: there is nothing to create. The host's
     * main loop executes the CPU through ndbus_runner_pump(). */
    runner->stop_requested = 0;
    runner->pumped = true;
    set_state(runner, NDBUS_RUNNER_RUNNING);
    return true;
}

unsigned ndbus_runner_pump(NdbusRunner *runner, unsigned max_steps)
{
    unsigned done = 0;

    if (runner == NULL || !runner->pumped
        || ndbus_runner_state(runner) != NDBUS_RUNNER_RUNNING)
    {
        return 0;
    }

    while (done < max_steps)
    {
        if (__atomic_load_n(&runner->stop_requested, __ATOMIC_RELAXED) != 0u)
        {
            break;
        }
        if (!runner->step(runner->cpu.ctx))
        {
            /* The CPU stopped on its own, as in the threaded build. */
            set_state(runner, NDBUS_RUNNER_STOPPED);
            return done;
        }
        done++;
        runner->instructions++;
    }

    /* Asked to stop: the pump is the only executor, so the stop is complete here. */
    if (done < max_steps)
    {
        set_state(runner, NDBUS_RUNNER_STOPPED);
    }
    return done;
}

void ndbus_runner_join(NdbusRunner *runner)
{
    if (runner == NULL || !runner->pumped)
    {
        return;
    }
    runner->pumped = false;
    set_state(runner, NDBUS_RUNNER_IDLE);
}

#endif

void ndbus_runner_stop_and_join(NdbusRunner *runner)
{
    ndbus_runner_request_stop(runner);
    ndbus_runner_join(runner);
}
