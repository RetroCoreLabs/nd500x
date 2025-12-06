/*
 * ND-500 Indirect Segment Handling
 *
 * Implements cross-domain calls via indirect segment capabilities.
 * Segment 31 is reserved for SINTRAN monitor calls (MON).
 *
 * Reference: ND500_PCB_MMU_COMPLETE_REFERENCE.md
 * C# Reference: CpuND500.IndirectSegments.cs
 */

#include "nd500_indirect.h"
#include "nd500_mmu.h"
#include "instruction_helpers.h"
#include "../machine/machine_protos.h"
#include "../libmon/mon.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * INTERNAL CONSTANTS
 * ========================================================================= */

/* Segment 31 is reserved for SINTRAN MON calls */
#define SINTRAN_SEGMENT 31

/* =========================================================================
 * MEMORY ACCESS CALLBACKS FOR LIBMON
 *
 * These callbacks allow libmon handlers to read/write memory through
 * the ND-500 MMU when necessary.
 * ========================================================================= */

static uint32_t mon_read_word_cb(void* cpu_ptr, uint32_t addr) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu || !cpu->machine) return 0;

    /* Use MMU if enabled, otherwise direct access */
    uint32_t phys_addr = addr;
    if (cpu->machine->mmu_enabled) {
        phys_addr = nd500_mmu_translate(cpu, addr, 0, 0); /* read, data */
    }

    /* Read 32-bit word (big-endian on ND-500) */
    uint32_t value = 0;
    value |= (uint32_t)nd500_bus_read8(cpu->machine, phys_addr) << 24;
    value |= (uint32_t)nd500_bus_read8(cpu->machine, phys_addr + 1) << 16;
    value |= (uint32_t)nd500_bus_read8(cpu->machine, phys_addr + 2) << 8;
    value |= (uint32_t)nd500_bus_read8(cpu->machine, phys_addr + 3);
    return value;
}

static void mon_write_word_cb(void* cpu_ptr, uint32_t addr, uint32_t val) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu || !cpu->machine) return;

    /* Use MMU if enabled, otherwise direct access */
    uint32_t phys_addr = addr;
    if (cpu->machine->mmu_enabled) {
        phys_addr = nd500_mmu_translate(cpu, addr, 1, 0); /* write, data */
    }

    /* Write 32-bit word (big-endian on ND-500) */
    nd500_bus_write8(cpu->machine, phys_addr, (uint8_t)(val >> 24));
    nd500_bus_write8(cpu->machine, phys_addr + 1, (uint8_t)(val >> 16));
    nd500_bus_write8(cpu->machine, phys_addr + 2, (uint8_t)(val >> 8));
    nd500_bus_write8(cpu->machine, phys_addr + 3, (uint8_t)val);
}

static uint8_t mon_read_byte_cb(void* cpu_ptr, uint32_t addr) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu || !cpu->machine) return 0;

    /* Use MMU if enabled */
    uint32_t phys_addr = addr;
    if (cpu->machine->mmu_enabled) {
        phys_addr = nd500_mmu_translate(cpu, addr, 0, 0);
    }

    return nd500_bus_read8(cpu->machine, phys_addr);
}

static void mon_write_byte_cb(void* cpu_ptr, uint32_t addr, uint8_t val) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu || !cpu->machine) return;

    /* Use MMU if enabled */
    uint32_t phys_addr = addr;
    if (cpu->machine->mmu_enabled) {
        phys_addr = nd500_mmu_translate(cpu, addr, 1, 0);
    }

    nd500_bus_write8(cpu->machine, phys_addr, val);
}

static void mon_set_k_flag_cb(void* cpu_ptr, int value) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu) return;

    uint32_t old_st1 = cpu->ST1;
    if (value) {
        cpu->ST1 |= (1u << ND500_ST_BIT_K);  /* Set K flag */
    } else {
        cpu->ST1 &= ~(1u << ND500_ST_BIT_K); /* Clear K flag */
    }
    nd500_log("MON K flag: %d -> %d (ST1: 0x%08X -> 0x%08X)",
              (old_st1 >> ND500_ST_BIT_K) & 1, value, old_st1, cpu->ST1);
}

static void mon_set_error_code_cb(void* cpu_ptr, int32_t code) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu) return;

    /* Error code goes in I1 register */
    cpu->I[0] = (uint32_t)code;
}

static void mon_set_i1_cb(void* cpu_ptr, uint32_t value) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu) return;
    cpu->I[0] = value;
}

static uint32_t mon_get_i1_cb(void* cpu_ptr) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu) return 0;
    return cpu->I[0];
}

/* =========================================================================
 * INDIRECT SEGMENT CHECK AND HANDLING
 * ========================================================================= */

int nd500_check_indirect_call(
    Nd500Cpu* cpu,
    uint32_t target_addr,
    uint32_t arg_count,
    const uint32_t* arg_addresses,
    uint32_t instruction_addr,
    uint32_t* out_resolved)
{
    if (!cpu || !out_resolved) {
        return INDIRECT_ERROR;
    }

    /* Extract segment number from address (bits 31-27) */
    uint32_t segment = (target_addr >> 27) & 0x1F;
    uint32_t offset = target_addr & 0x07FFFFFF;

    /* If MMU is disabled, all calls are direct */
    if (!cpu->machine || !cpu->machine->mmu_enabled) {
        *out_resolved = target_addr;
        return INDIRECT_DIRECT;
    }

    /* Get program capability for this segment in current executing domain */
    uint16_t pc = nd500_mmu_get_program_capability(cpu, cpu->CED, segment);

    /* Check PC_IND flag (bit 15) - if clear, this is a direct call */
    if ((pc & PC_IND) == 0) {
        *out_resolved = target_addr;
        return INDIRECT_DIRECT;
    }

    /* ═══════════════════════════════════════════════════════════════════════
     * INDIRECT CALL - Extract target domain and segment from capability
     * ═══════════════════════════════════════════════════════════════════════ */

    /* Extract target domain (bits 13-5, 9 bits) and segment (bits 4-0, 5 bits) */
    uint32_t target_domain = (pc & PC_DOM) >> 5;
    uint32_t target_segment = pc & PC_SEG;

    /* ═══════════════════════════════════════════════════════════════════════
     * SINTRAN SEGMENT 31 - MON CALL HANDLING
     *
     * Segment 31 is reserved for SINTRAN monitor calls.
     * The offset IS the MON number (not divided by 4).
     * ═══════════════════════════════════════════════════════════════════════ */

    if (target_segment == SINTRAN_SEGMENT) {
        /* MON number is the offset directly (from assembly analysis) */
        uint32_t mon_number = offset;

        /* Build MON context */
        MonContext ctx;
        memset(&ctx, 0, sizeof(ctx));

        ctx.cpu = cpu;
        ctx.machine = cpu->machine;
        ctx.mon_number = mon_number;
        ctx.instruction_address = instruction_addr;
        ctx.return_address = cpu->pending_call_return_address;
        ctx.arg_count = arg_count;

        /* Copy argument addresses */
        for (uint32_t i = 0; i < arg_count && i < MON_MAX_ARGS; i++) {
            ctx.arg_addresses[i] = arg_addresses[i];
        }

        /* Setup memory access callbacks */
        ctx.read_word = mon_read_word_cb;
        ctx.write_word = mon_write_word_cb;
        ctx.read_byte = mon_read_byte_cb;
        ctx.write_byte = mon_write_byte_cb;

        /* Setup flag/register callbacks */
        ctx.set_k_flag = mon_set_k_flag_cb;
        ctx.set_error_code = mon_set_error_code_cb;
        ctx.set_i1 = mon_set_i1_cb;
        ctx.get_i1 = mon_get_i1_cb;

        /* Dispatch MON call */
        MonResult result = mon_dispatch(&ctx);
        (void)result; /* Result is informational - check flags instead */

        /* ND-500 specific: Place error code in W1 if error occurred
         * This is redundant with the set_error_code callback, but ensures
         * the error code is always placed in W1 for ND-500 */
        if (ctx.error_flag && ctx.error_code != 0) {
            cpu->I[0] = (uint32_t)ctx.error_code;  /* W1 = I[0] on ND-500 */
        }

        /* Check for halt request (MON 0B LEAVE or unimplemented) */
        if (ctx.halt_requested) {
            cpu->machine->run_flag = 0;
            cpu->machine->stop_reason = ctx.halt_reason ? ctx.halt_reason : "MON halt";
            if (ctx.halt_reason) {
                /* Store halt reason for debugger display */
                printf("[MON] CPU halted: %s\n", ctx.halt_reason);
            }
            *out_resolved = ctx.return_address;
            return INDIRECT_ERROR;
        }

        /* Check for break request (unimplemented MON with BREAK behavior) */
        if (ctx.break_requested) {
            cpu->machine->run_flag = 0;
            cpu->machine->stop_reason = "Unimplemented MON call";
            const char* mon_name = mon_get_name(ctx.mon_number);
            const char* mon_octal = mon_get_octal(ctx.mon_number);
            printf("[MON] Break: unimplemented MON %s (%s) with %u args\n",
                   mon_octal ? mon_octal : "?",
                   mon_name ? mon_name : "UNKNOWN",
                   ctx.arg_count);
            for (uint32_t i = 0; i < ctx.arg_count && i < MON_MAX_ARGS; i++) {
                uint16_t val = ctx.read_word ? ctx.read_word(ctx.cpu, ctx.arg_addresses[i]) : 0;
                printf("  Arg[%u] @ 0x%08X = 0x%04X (%u)\n",
                       i, ctx.arg_addresses[i], val, val);
            }
            *out_resolved = ctx.return_address;
            return INDIRECT_BREAK;
        }

        /* MON call completed - return to caller */
        *out_resolved = ctx.return_address;
        return INDIRECT_HANDLED;
    }

    /* ═══════════════════════════════════════════════════════════════════════
     * OTHER INDIRECT SEGMENTS - DOMAIN SWITCHING
     *
     * For non-SINTRAN indirect segments, we need to:
     * 1. Read the Start Address Vector from target segment
     * 2. Prepare domain switch context
     * 3. Return entry point address
     *
     * This is not yet implemented - just log and continue.
     * ═══════════════════════════════════════════════════════════════════════ */

    printf("[INDIRECT] Domain switch not implemented: "
           "segment %u -> domain %u segment %u (offset 0x%08X)\n",
           segment, target_domain, target_segment, offset);

    /* For now, treat as direct call (will likely fail, but allows debugging) */
    *out_resolved = target_addr;
    return INDIRECT_DIRECT;
}

/* =========================================================================
 * SEGMENT 31 SETUP
 *
 * Called during DOM/SEG loading to configure segment 31 for SINTRAN calls.
 * ========================================================================= */

void nd500_setup_sintran_segment(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu) return;

    /* Setup PC[31] as indirect segment pointing to domain 0, segment 31
     * PC_IND (0x8000) | domain 0 in bits 13-5 | segment 31 in bits 4-0
     * = 0x8000 | (0 << 5) | 31 = 0x801F */
    uint16_t capability = PC_IND | (0 << 5) | SINTRAN_SEGMENT;

    nd500_mmu_set_program_capability(cpu, domain, SINTRAN_SEGMENT, capability);
}

/* =========================================================================
 * HELPER FUNCTIONS
 * ========================================================================= */

int nd500_program_mmu_enabled(Nd500Cpu* cpu) {
    if (!cpu || !cpu->machine) return 0;
    return cpu->machine->mmu_enabled;
}
