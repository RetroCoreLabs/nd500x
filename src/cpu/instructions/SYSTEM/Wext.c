/*
 * Wext.c - ND-500 WEXT and REXT instructions (SYSTEM class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WEXT - Write to device external to CPU ('87 extension)
 * REXT - Read from device external to CPU ('87 extension)
 *
 * ND-500 Reference Manual ND-05.009.4, sections 16.28 and 16.29:
 *
 *   Wn REXT <device/r/W>   FFE8H, 177750B+n-1   <device> -> In
 *   Wn WEXT <device/r/W>   FFEC,  177754B+n-1   In -> <device>
 *
 *   "Privileged instruction."
 *   "Device = 0 : OCTO-bus / ACCP."  "Further devices will be supported in
 *   later versions."
 *   WEXT: "Unable to write data 1 -> K, else 0 -> K"
 *   REXT: "Nothing read 1 -> K, else 0 -> K"
 *
 * WHAT THE MICROCODE DOES (MICRO-5800-B30, entries 001053 REXT and 001055 WEXT,
 * bodies at 010517 and 010546):
 *
 *   - privilege test, else ILLEG;
 *   - the device number is tested against 7: anything above 7 goes to
 *     WRIT_DEVIL / READ_DEVIL, which set K = 1 and do nothing else, and of
 *     0..7 only vector 0 (WRIT_DEV00 / READ_DEV00) does any work;
 *   - WRIT_DEV00 tests an AFLAG bit; if the ACCP cannot take a word K = 1,
 *     otherwise the register goes to the ACCP input buffer (D,SPEC,AIB) and
 *     K = 0. That is the same buffer the microcode's own GIVEINT writes its
 *     answer-interrupt frame to, so the register holds an octobus frame in
 *     transmit format: destination station in bits 13-8;
 *   - READ_DEV00 tests an AFLAG bit; with a word waiting the register gets the
 *     ACCP output buffer (A,SPEC,AOB) and K = 0, otherwise the register gets 0
 *     and K = 1.
 *
 * MEASURED 11-OCT-2026 under SINTRAN III VSX/500 L with two ND-5000 CPUs: the
 * swapper on CPU 1 executes W3 WEXT at P=0x08008A68 as soon as a process is
 * placed on CPU 2, and never with one CPU. Without the instruction it took an
 * illegal-instruction trap and SINTRAN reported "The Swapper stopped".
 *
 * The device itself is not the CPU's: an embedder that has an octobus installs
 * the two hooks with nd500_set_ext_device_hooks(). With none installed there is
 * no external device, and both instructions report that through K.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/* The device number, or -1 with a trap raised or the instruction aborted. */
static long ext_device_operand(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi, const char* name) {
    if (fi->operand_count != 1) {
        printf("[ERROR] %s at PC=0x%08X: Expected 1 operand, got %u\n",
               name, fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return -1;
    }
    if (fi->target_register < 1 || fi->target_register > 4) {
        trap_illegal_operand(cpu, fi->address);
        return -1;
    }
    if (!nd500_require_privilege(cpu, fi->address)) {
        return -1;
    }
    uint64_t device = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return -1;
    }
    /* Kept as a number the caller can test; a device above 2^31 is no device. */
    return (device > 0x7FFFFFFFu) ? 0x7FFFFFFFL : (long)device;
}

static void ext_set_k(Nd500Cpu* cpu, int unable) {
    if (unable) {
        cpu->ST1 |= ND500_FLAG_K;
    } else {
        cpu->ST1 &= ~ND500_FLAG_K;
    }
}

void nd500_instr_Wext(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    long device = ext_device_operand(cpu, fi, "WEXT");
    if (device < 0) {
        return;
    }
    uint32_t value = nd500_read_integer_register(cpu, fi->target_register);
    /* Only device 0 exists; the microcode sends every other number to WRIT_DEVIL. */
    int unable = (device != 0) || (nd500_ext_device_write(cpu, (uint32_t)device, value) != 0);
    ext_set_k(cpu, unable);
}

void nd500_instr_Rext(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    long device = ext_device_operand(cpu, fi, "REXT");
    if (device < 0) {
        return;
    }
    uint32_t value = 0;
    int nothing = (device != 0) || (nd500_ext_device_read(cpu, (uint32_t)device, &value) != 0);
    /* READ_DEVIL and RDEV_00_1 both clear the value before it is stored. */
    nd500_write_integer_register(cpu, fi->target_register, nothing ? 0u : value);
    ext_set_k(cpu, nothing);
}
