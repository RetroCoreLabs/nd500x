/*
 * Clock.c - ND-500 CLINIT and CLREAD instructions (SYSTEM class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * ND-500 Reference Manual ND-05.009.4, sections 15.17 and 15.18
 * ('87 extensions):
 *
 *   CLINIT   FF1EH, 177436B   0 -> <clock>
 *     "Privileged instruction"
 *     "The CPU contains a local clock running at 1 microsecond cycle time."
 *     "Clock is reset and started."
 *     Trap conditions: none. Data status bits: unaffected.
 *
 *   CLREAD   FF1FH, 177437B   <clock> -> W1
 *     "The clock value is read into register number 1. Time is an integer
 *     value giving the number of microseconds since the last CLINIT
 *     instruction."
 *     "the clock counts for a period of 2**32 microseconds after which it
 *     starts from zero again."
 *     Trap conditions: none. Data status bits: <clock> = 0 -> Z,
 *     <clock>.signbit -> S.
 *
 * CLREAD is not described as privileged, so it is not treated as one.
 *
 * MEASURED 11-OCT-2026 under SINTRAN III VSX/500 L with two ND-5000 CPUs: the
 * swapper executes CLREAD at P=0x08008881 after it has kicked the other CPU
 * with WEXT. With one CPU it never reaches either instruction.
 *
 * THE CLOCK IS THE HOST'S. The real one counts microseconds of real time, so
 * this reads the host's monotonic clock. A CPU that never executed CLINIT
 * counts from the first time its clock is looked at; the manual does not say
 * what such a clock holds.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <time.h>

static uint64_t host_microseconds(void) {
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)(ts.tv_nsec / 1000);
}

void nd500_instr_Clinit(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;
    }
    cpu->local_clock_base_us = host_microseconds();
    cpu->local_clock_started = 1;
}

void nd500_instr_Clread(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    (void)fi;
    uint64_t now = host_microseconds();
    if (!cpu->local_clock_started) {
        cpu->local_clock_base_us = now;
        cpu->local_clock_started = 1;
    }
    /* 32 bits of microseconds: wraps after 2**32, as the manual says. */
    uint32_t clock = (uint32_t)(now - cpu->local_clock_base_us);
    nd500_write_integer_register(cpu, 1, clock);
    nd500_set_flags_zs(cpu, clock, ND500_DTYPE_WORD);
}
