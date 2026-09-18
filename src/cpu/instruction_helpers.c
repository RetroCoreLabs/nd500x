/*
 * instruction_helpers.c - helpers shared by the instruction handlers: memory access, flags, types
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "instruction_helpers.h"
#include "cpu_protos.h"
#include "nd500_mmu.h"
#include "../machine/machine_protos.h"
#include <stdio.h>
#include <stdlib.h>   /* getenv - implicit-int prototype truncates the char* -> wild-pointer crash */
#include <math.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

/* ND500X_UWATCH=1: log any write to the FIRST 12 BYTES (u_pcb, u_how, u_procp)
 * of EVERY u-area window the kernel owns. Those two pointers were found
 * corrupted for init when the kernel died in setprt(); this names the writer.
 *
 * Window addresses come from kernel/MASTER/machine/locore.c (.set directives)
 * with MAXUSERS=32 -> NPROC=256, UPAGES=4, NBPG=2048, _Sysbase=0x18000000:
 *   _u        0xE8000000  (segment 29, the RUNNING process' u-area)
 *   _forkutl  0x18180000  (_Sysbase + NPROC*3*NBPG)
 *   _xswaputl 0x18182000  _xswap2utl 0x18184000
 *   _swaputl  0x18186000  _pushutl   0x18188000
 */
static void nd_uwatch(Nd500Cpu* cpu, uint32_t vaddr, uint32_t paddr, uint32_t value, int width)
{
    static const struct { uint32_t base; const char* name; } win[] = {
        { 0xE8000000u, "u"        },
        { 0x18180000u, "forkutl"  },
        { 0x18182000u, "xswaputl" },
        { 0x18184000u, "xswap2utl"},
        { 0x18186000u, "swaputl"  },
        { 0x18188000u, "pushutl"  },
    };
    static int on = -1;
    if (on < 0) on = nd500_settings()->uwatch;
    if (!on) return;
    /* A store whose bytes cross a 2KB page boundary is translated ONCE, for
     * its first byte, then written at paddr+1..3 - so the tail bytes land in
     * whatever frame physically follows, not in the frame the NEXT virtual
     * page is mapped to. Log every straddling store in the u-area window. */
    if (vaddr >= 0xE8000000u && vaddr < 0xE8002000u) {
        uint32_t span = (uint32_t)(width / 8);
        if (span > 1 && ((vaddr & 0x7FFu) + span) > 0x800u) {
            static unsigned ns = 0;
            if (ns++ < 100)
                fprintf(stderr, "[UWATCH-STRADDLE] w%-2d va=0x%08X pa=0x%08X <- 0x%08X  PC=0x%08X CED=%u B=0x%08X\n",
                        width, vaddr, paddr, value, cpu->PC, cpu->CED, cpu->B);
        }
    }
    for (unsigned i = 0; i < sizeof(win)/sizeof(win[0]); i++) {
        if (vaddr >= win[i].base && vaddr < win[i].base + 12u) {
            static unsigned n = 0;
            if (n++ < 400)
                fprintf(stderr, "[UWATCH] %-9s+%u w%-2d va=0x%08X pa=0x%08X <- 0x%08X  PC=0x%08X CED=%u B=0x%08X\n",
                        win[i].name, vaddr - win[i].base, width, vaddr, paddr,
                        value, cpu->PC, cpu->CED, cpu->B);
            return;
        }
    }
}


/*
 * A multi-byte access is translated ONCE, for its first byte, and the tail
 * bytes are then read/written at paddr+1..N-1. That is only valid while the
 * whole access stays inside one NBPG (2KB) page: consecutive virtual pages are
 * NOT physically contiguous, so a straddling access must be split and each
 * half translated on its own.
 *
 * Found in NDIX multiuser boot: ino_close()'s `cfunc = cdevsw[major(dev)].d_close`
 * stores a word to the kernel stack at u-area VA 0xE8000FFE. The tail two bytes
 * landed in the frame that physically FOLLOWS 0xE8000800's frame instead of the
 * frame VA 0xE8001000 maps to - which was another process' u-area page 0 - and
 * shredded the high half of its u_pcb (0xE0000300 -> 0xD5640300). setprt() then
 * dereferenced the wild pointer and the kernel died.
 *
 * Splitting into per-byte accesses is restart-safe: if the second page faults,
 * the instruction is re-executed and stores the same bytes again.
 */
static int nd_crosses_page(const Nd500Cpu* cpu, uint32_t vaddr, uint32_t span) {
    return cpu && cpu->machine && cpu->machine->mmu_enabled &&
           ((vaddr & (uint32_t)(NBPG - 1)) + span) > (uint32_t)NBPG;
}

/*
 * True once the current byte of a split access has faulted, so the split loop
 * must stop immediately.
 *
 * Getting this wrong is fatal, not merely wasteful. When the byte landing in
 * the second page faults, raise_trap DISPATCHES to the guest handler: it sets
 * in_trap_handler, points PC at the handler and CLEARS the global trap state.
 * A loop that kept going would then access the still-unmapped remaining
 * byte(s) while already inside the handler - a double fault, which raise_trap
 * cannot nest and therefore halts the machine on. That is why the halt was
 * always reported at the LAST byte of the access (0x157FE+3 = 0x15801 in
 * cc1's pftn.c stab[] init, 0xC7FE+3 = 0xC801 in the ptest2 reducer) rather
 * than at the first byte that actually faulted.
 *
 * Stopping at the first fault is correct because the split is restartable:
 * the bytes already written to the first page get written again, identically,
 * when the kernel's pagein returns and the instruction re-executes.
 *
 * instr_aborted is checked as well as nd500_trap_occurred() because a
 * dispatched handler clears the trap state synchronously - see the
 * instr_aborted comment in cpu_protos.h.
 */
static int nd_split_faulted(const Nd500Cpu* cpu) {
    return nd500_trap_occurred() || (cpu && cpu->instr_aborted);
}

/* ============================================================================
 * INTERNAL HELPER: MMU-Aware Memory Access via bus functions
 * ============================================================================
 * We use nd500_bus_read/write functions from machine/io.c which handle MMU
 * translation automatically when cpu->machine->mmu_enabled is set.
 */


/* ============================================================================
 * MEMORY ACCESS HELPERS (Little-Endian Multi-Byte)
 * ============================================================================
 */

uint8_t nd500_read_memory_8(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    uint8_t value = nd500_bus_read8(cpu->machine, paddr);
    /* _Udata path-read trace (env ND500X_PATHDBG): namei/copyinstr reads init's
     * exec path "/etc/init" from user data via the _Udata(seg-30, 0xF0000000)
     * window. Log the bytes the kernel actually gets, with the translated paddr,
     * to see if the seg-30 page maps init's real dcode or returns 0. */
    if (vaddr >= 0xF0000000u && vaddr < 0xF0000100u && cpu->CED == 0 && nd500_settings()->pathdbg)
        fprintf(stderr, "[PATHDBG] read8 Udata[0x%08X] pa=0x%08X = 0x%02X '%c' @PC=0x%08X\n",
                vaddr, paddr, value, (value>=32&&value<127)?value:'.', cpu->PC);
    MEMTRACE_RD("[MEMTRACE] read_8:  vaddr=0x%08X paddr=0x%08X value=0x%02X\n", vaddr, paddr, value);
    return value;
}

/**
 * Read 8-bit value from PROGRAM space (instruction fetch path)
 *
 * On ND-500, program and data use separate capability tables:
 * - Program space (PMON): Used for instruction fetch
 * - Data space (DMON): Used for data read/write
 *
 * This function uses is_instruction=1 to access program capabilities.
 * Used by CALL/CALLG to validate entry point opcodes.
 *
 * Reference: ND-500 Reference Manual, Chapter 2 (Memory Architecture)
 */
uint8_t nd500_fetch_memory_8(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 1); // is_write=0, is_instruction=1 (PROGRAM space!)
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    uint8_t value = nd500_bus_read8(cpu->machine, paddr);
    MEMTRACE_RD("[MEMTRACE] fetch_8: vaddr=0x%08X paddr=0x%08X value=0x%02X (PROGRAM SPACE)\n", vaddr, paddr, value);
    return value;
}

void nd500_write_memory_8(Nd500Cpu* cpu, uint32_t vaddr, uint8_t value) {
    if (!cpu || !cpu->machine) return;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0
        if (nd500_trap_occurred() || cpu->instr_aborted) return;  // translation faulted (handler may have cleared trap state - check abort flag too)
    }
    nd_uwatch(cpu, vaddr, paddr, (uint32_t)value, 8);

    MEMTRACE_WR("[MEMTRACE] write_8: vaddr=0x%08X paddr=0x%08X value=0x%02X\n", vaddr, paddr, value);
    { nd500_ptewatch_wr(cpu->PC, vaddr, paddr, value, 8); }
    /* Kernel path-copy tracer (env ND500X_SLASHDBG): log CED=0 byte stores of
     * '/' - the first byte of every path namei copies into its geteblk buffer.
     * Reveals WHICH virtual address the name buffer really is and where it
     * translates physically (the post-exec namei-ENOENT hunt). */
    {
        static int sld = -1;
        if (sld < 0) sld = nd500_settings()->slashdbg;
        if (sld && cpu->CED == 0 && value == 0x2F) {
            static unsigned n = 0;
            if (n++ < 60)
                fprintf(stderr, "[SLASH] w8 vaddr=0x%08X paddr=0x%08X PC=0x%08X\n",
                        vaddr, paddr, cpu->PC);
        }
    }
    nd500_bus_write8(cpu->machine, paddr, value);
}

uint16_t nd500_read_memory_16(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    if (nd_crosses_page(cpu, vaddr, 2)) {   /* see nd_crosses_page */
        uint8_t b0 = nd500_read_memory_8(cpu, vaddr);
        if (nd_split_faulted(cpu)) return 0;   /* see nd_split_faulted */
        uint8_t b1 = nd500_read_memory_8(cpu, vaddr + 1);
        return (uint16_t)(((uint16_t)b0 << 8) | (uint16_t)b1);
    }

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    // Read two bytes BIG-ENDIAN from physical address (ND-500 spec)
    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    uint16_t value = ((uint16_t)b0 << 8) | (uint16_t)b1;
    MEMTRACE_RD("[MEMTRACE] read_16: vaddr=0x%08X paddr=0x%08X value=0x%04X\n", vaddr, paddr, value);
    return value;
}

void nd500_write_memory_16(Nd500Cpu* cpu, uint32_t vaddr, uint16_t value) {
    if (!cpu || !cpu->machine) return;

    if (nd_crosses_page(cpu, vaddr, 2)) {   /* see nd_crosses_page */
        nd500_write_memory_8(cpu, vaddr,     (uint8_t)((value >> 8) & 0xFF));
        if (nd_split_faulted(cpu)) return;   /* see nd_split_faulted */
        nd500_write_memory_8(cpu, vaddr + 1, (uint8_t)(value & 0xFF));
        return;
    }

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0
        if (nd500_trap_occurred() || cpu->instr_aborted) return;  // translation faulted (handler may have cleared trap state - check abort flag too)
    }

    MEMTRACE_WR("[MEMTRACE] write_16: vaddr=0x%08X paddr=0x%08X value=0x%04X\n", vaddr, paddr, value);
    { nd500_ptewatch_wr(cpu->PC, vaddr, paddr, value, 16); }
    /* usrpt-window PTE-write trace (env ND500X_UPTWDBG): the kernel writes user
     * page-table entries (pfns) as halfword stores through the usrpt(seg-4) KVA
     * window [0x20000000..0x28000000). Log vaddr->paddr->pfn so we can see where
     * the data-page-0 PTE lands physically vs the emulator's seg-30 L2 (0x489000). */
    if (vaddr >= 0x20000000u && vaddr < 0x20008000u && value != 0 && value < 0x2000u
        && cpu->CED == 0 && nd500_settings()->uptwdbg) {
        fprintf(stderr, "[UPTWDBG] usrpt write vaddr=0x%08X -> paddr=0x%08X pfn=0x%04X @PC=0x%08X\n",
                vaddr, paddr, value, cpu->PC);
    }
    if (vaddr == 0x30001004u && nd500_settings()->ipcurdbg) {
        fprintf(stderr, "[IPCURDBG] ip_current := %u  @PC=0x%08X L=0x%08X B=0x%08X R=0x%08X\n",
                value, cpu->PC, cpu->L, cpu->B, cpu->R);
    }
    // Write two bytes BIG-ENDIAN to physical address (ND-500 spec)
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)(value & 0xFF));
}

uint32_t nd500_read_memory_32(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    if (nd_crosses_page(cpu, vaddr, 4)) {   /* see nd_crosses_page */
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            v = (v << 8) | (uint32_t)nd500_read_memory_8(cpu, vaddr + i);
            if (nd_split_faulted(cpu)) return 0;   /* see nd_split_faulted */
        }
        return v;
    }

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    // Read four bytes BIG-ENDIAN from physical address (ND-500 spec)
    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, paddr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, paddr + 3);
    uint32_t value = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
                     ((uint32_t)b2 << 8) | (uint32_t)b3;
    MEMTRACE_RD("[MEMTRACE] read_32: vaddr=0x%08X paddr=0x%08X value=0x%08X\n", vaddr, paddr, value);
    /* _Udata-window read trace (env ND500X_UDATADBG): the kernel reads user syscall
     * args via fuword = *(_Udata(0xF0000000)+uaddr). For init's execve the syscall code
     * is at user B+20=0x08000014 -> _Udata window 0xF8000014. Log reads in that window
     * (CED==0) to see whether fuword resolves init's args (code should be 0x3B=59). */
    {
        static int udbg = -1;
        if (udbg < 0) udbg = nd500_settings()->udatadbg;
        if (udbg && cpu->CED == 0 && vaddr >= 0xF0000000u)
            fprintf(stderr, "[UDATA] read vaddr=0x%08X paddr=0x%08X value=0x%08X (fuword) B=0x%08X\n", vaddr, paddr, value, cpu->B);
    }
    /* PTE read-watch (env ND500X_PTEWATCH): the kernel READS init's stack pte
     * (pg_pfnum in low 30 bits) through the usrpt KVA window; logging that read
     * reveals the PHYSICAL address the kernel's pte actually lives at, to compare
     * against the emulator's PST->L1->L2 slot (0x48A000). If they differ, the
     * usrpt-window mapping and the L1 walk have diverged - the coherence bug. */
    {
        nd500_ptewatch_read(vaddr, paddr, value);
    }
    return value;
}

void nd500_write_memory_32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value) {
    if (!cpu || !cpu->machine) return;

    if (nd_crosses_page(cpu, vaddr, 4)) {   /* see nd_crosses_page */
        for (int i = 0; i < 4; i++) {
            nd500_write_memory_8(cpu, vaddr + i, (uint8_t)((value >> (24 - 8 * i)) & 0xFF));
            if (nd_split_faulted(cpu)) return;   /* see nd_split_faulted */
        }
        return;
    }

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            MEMTRACE_WR("[MEMTRACE] write_32: MMU trap! vaddr=0x%08X value=0x%08X\n", vaddr, value);
            return;  // translation faulted (handler may have cleared trap state)
        }
    }
    nd_uwatch(cpu, vaddr, paddr, value, 32);

    // Debug: warn if writing to address beyond physical memory
    if (paddr >= cpu->machine->memory_size) {
        MEMTRACE_WR("[MEMTRACE] write_32: BEYOND MEMORY! vaddr=0x%08X paddr=0x%08X value=0x%08X mem_size=0x%X\n",
              vaddr, paddr, value, cpu->machine->memory_size);
        return;  // Don't write beyond memory!
    }

    MEMTRACE_WR("[MEMTRACE] write_32: vaddr=0x%08X paddr=0x%08X value=0x%08X\n", vaddr, paddr, value);

    /* PTE write-watch at the CPU-store level (env ND500X_PTEWATCH). CPU stores
     * go byte-wise through bus_write8, so the value-level watch in io.c never
     * sees them - catch them here with full context (vaddr AND paddr). Logs any
     * store whose low-30-bits (the ND-500 real-pte pg_pfnum field) equals the
     * watched frame, or whose paddr lands in the watched L2 page. This reveals
     * WHICH kernel virtual address (usrpt window 0x20000000+off?) the kernel
     * writes init's stack pte to, and WHICH physical page that translates to -
     * to compare against the emulator's PST->L1->L2 slot (0x48A000). */
    {
        nd500_ptewatch_store(vaddr, paddr, value);
        nd500_ptewatch_wr(cpu->PC, vaddr, paddr, value, 32);
    }

    // Write four bytes BIG-ENDIAN to physical address (ND-500 spec)
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)((value >> 24) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)((value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 2, (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 3, (uint8_t)(value & 0xFF));
}

int nd500_heap_alloc_block(Nd500Cpu* cpu, uint8_t log_size, uint32_t pc,
                           uint32_t* out_addr) {
    /* TOS points to the heap variables (section 3.3, Figure 4). */
    uint32_t heap_vars_addr = cpu->TOS;
    if (heap_vars_addr == 0) {
        trap_stack_overflow(cpu, pc);
        return 0;
    }

    /* MAXL at +0 - requesting larger than the heap allows is STO. */
    uint32_t max_log = nd500_read_memory_32(cpu, heap_vars_addr + 0);
    if (log_size > max_log) {
        trap_stack_overflow(cpu, pc);
        return 0;
    }

    /* Exact-size element available in FLOG[log_size]? Unlink it. */
    uint32_t freelist_addr = heap_vars_addr + 12 + ((uint32_t)log_size * 4);
    uint32_t block_addr = nd500_read_memory_32(cpu, freelist_addr);

    if (block_addr != 0) {
        uint32_t next_block = nd500_read_memory_32(cpu, block_addr);
        nd500_write_memory_32(cpu, freelist_addr, next_block);
    } else {
        /* Search the larger freelists and split down to the desired size. */
        for (uint8_t k = (uint8_t)(log_size + 1); k <= max_log; k++) {
            freelist_addr = heap_vars_addr + 12 + ((uint32_t)k * 4);
            block_addr = nd500_read_memory_32(cpu, freelist_addr);
            if (block_addr != 0) {
                uint32_t next_block = nd500_read_memory_32(cpu, block_addr);
                nd500_write_memory_32(cpu, freelist_addr, next_block);

                /* Chop into halves; each upper half goes onto its freelist.
                 * Sizes are in words, addresses in bytes (1 word = 4 bytes). */
                uint8_t found_size = k;
                while (found_size > log_size) {
                    found_size--;
                    uint32_t half_block_size_bytes = (1U << found_size) * 4;
                    uint32_t buddy_addr = block_addr + half_block_size_bytes;
                    uint32_t buddy_list_addr = heap_vars_addr + 12 + ((uint32_t)found_size * 4);
                    uint32_t old_head = nd500_read_memory_32(cpu, buddy_list_addr);
                    nd500_write_memory_32(cpu, buddy_addr, old_head);
                    nd500_write_memory_32(cpu, buddy_list_addr, buddy_addr);
                }
                break;
            }
        }

        /* No element of the requested size or larger available - raise STO so
         * the program's own trap handler can seed/extend the heap. GETB/ENTB
         * must never touch STAH/ENDH (section 3.3: reserved for trap
         * handlers); re-seeding here would hand out blocks that overlap live
         * allocations and corrupt the heap.
         *
         * NOTE (2026-07-26): an emulator "grow the heap here when the program has
         * no trap-27 handler" fallback was tried for vendor DOMs like NC-A06 that
         * install no STO handler. It REGRESSED the working compile (NRF 933->0):
         * with nested UECOM programs each establishing their own heaps at the same
         * segment VAs, plus the RAM snapshot/restore around the nested run, the
         * heap-vars get written from several sources and the injected heap
         * conflicts. Reverted - see NC_CRASH_...md UPDATE 76c. The overflow stays
         * cosmetic (the :NRF is written before it and the process exits). */
        if (block_addr == 0) {
            if (nd500_settings()->heapdbg) {
                uint32_t stah = nd500_read_memory_32(cpu, heap_vars_addr + 4);
                uint32_t endh = nd500_read_memory_32(cpu, heap_vars_addr + 8);
                fprintf(stderr, "[HEAPDBG] STO(exhausted) PC=0x%08X req_log=%u max_log=%u "
                        "TOS=0x%08X STAH=0x%08X ENDH=0x%08X freelists:",
                        pc, log_size, max_log, cpu->TOS, stah, endh);
                for (uint8_t q = 0; q <= max_log && q < 32; q++) {
                    uint32_t fa = heap_vars_addr + 12 + (uint32_t)q * 4;
                    fprintf(stderr, " [%u]=0x%08X", q, nd500_read_memory_32(cpu, fa));
                }
                fprintf(stderr, "\n");
            }
            trap_stack_overflow(cpu, pc);
            return 0;
        }
    }

    if (nd500_settings()->heapdbg) {
        static uint32_t last_tos = 0xFFFFFFFF;
        if (heap_vars_addr != last_tos) {
            last_tos = heap_vars_addr;
            uint32_t stah = nd500_read_memory_32(cpu, heap_vars_addr + 4);
            uint32_t endh = nd500_read_memory_32(cpu, heap_vars_addr + 8);
            fprintf(stderr, "[HEAPDBG] GETB-OK PC=0x%08X block=0x%08X req_log=%u TOS=0x%08X "
                    "STAH=0x%08X ENDH=0x%08X (first success for this heap)\n",
                    pc, block_addr, log_size, heap_vars_addr, stah, endh);
        }
    }

    *out_addr = block_addr;
    return 1;
}

/*
 * A 64-bit access is two 32-bit halves, and the second must NOT be attempted
 * once the first has faulted - see nd_split_faulted. Both halves usually sit
 * in the SAME page, so this is a distinct bug from the straddle case: any
 * 64-bit access to a not-yet-resident page faulted on the low half, got the
 * handler dispatched, then ran the high half from inside the handler and
 * double-faulted. Seen as the native /lib/as dying at PC=0x4799 - the fault
 * was dispatched for 0x13990 and the machine then halted on 0x13994.
 */
uint64_t nd500_read_memory_64(Nd500Cpu* cpu, uint32_t vaddr) {
    // Read eight bytes BIG-ENDIAN (ND-500 spec)
    uint32_t high = nd500_read_memory_32(cpu, vaddr);
    if (nd_split_faulted(cpu)) return 0;
    uint32_t low  = nd500_read_memory_32(cpu, vaddr + 4);
    return ((uint64_t)high << 32) | (uint64_t)low;
}

void nd500_write_memory_64(Nd500Cpu* cpu, uint32_t vaddr, uint64_t value) {
    // Write eight bytes BIG-ENDIAN (ND-500 spec)
    nd500_write_memory_32(cpu, vaddr,     (uint32_t)((value >> 32) & 0xFFFFFFFF));
    if (nd_split_faulted(cpu)) return;
    nd500_write_memory_32(cpu, vaddr + 4, (uint32_t)(value & 0xFFFFFFFF));
}


/* ============================================================================
 * DOMAIN-AWARE MEMORY ACCESS (for ALT prefix support)
 * ============================================================================
 * The ALT prefix allows instructions to access data in CAD (Current Alternative
 * Domain) instead of CED (Current Executing Domain). This is essential for
 * cross-domain calls where a callee needs to access caller's data space.
 *
 * Reference: ND-500 Reference Manual, Chapter 6 (Domain System)
 */

uint8_t nd500_read_memory_8_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain) {
    if (!cpu || !cpu->machine) return 0;

    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate_domain(cpu, vaddr, 0, 0, domain);
        if (nd500_trap_occurred()) return 0;
    }

    uint8_t value = nd500_bus_read8(cpu->machine, paddr);
    if (vaddr >= 0xF0000000u && vaddr < 0xF0000100u && nd500_settings()->pathdbg)
        fprintf(stderr, "[PATHDBG] read Udata[0x%08X] pa=0x%08X = 0x%02X '%c' @PC=0x%08X\n",
                vaddr, paddr, value, (value>=32&&value<127)?value:'.', cpu->PC);
    MEMTRACE_RD("[MEMTRACE] read_8_domain:  vaddr=0x%08X paddr=0x%08X domain=%d value=0x%02X\n",
                vaddr, paddr, domain, value);
    return value;
}

void nd500_write_memory_8_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t value, uint8_t domain) {
    if (!cpu || !cpu->machine) return;

    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate_domain(cpu, vaddr, 1, 0, domain);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;
    }
    nd_uwatch(cpu, vaddr, paddr, (uint32_t)value, 8);

    MEMTRACE_WR("[MEMTRACE] write_8_domain: vaddr=0x%08X paddr=0x%08X domain=%d value=0x%02X\n",
                vaddr, paddr, domain, value);
    /* Kernel path-copy tracer (env ND500X_SLASHDBG) - see write_memory_8. */
    {
        static int sld = -1;
        if (sld < 0) sld = nd500_settings()->slashdbg;
        if (sld && domain == 0 && value == 0x2F) {
            static unsigned n = 0;
            if (n++ < 60)
                fprintf(stderr, "[SLASH] w8dom vaddr=0x%08X paddr=0x%08X PC=0x%08X\n",
                        vaddr, paddr, cpu->PC);
        }
    }
    /* _Udata window WRITE trace (env ND500X_UDATADBG): where does the kernel's
     * copyout of init's dcode ("/etc/init") land physically? */
    {
        static int udbg = -1;
        if (udbg < 0) udbg = nd500_settings()->udatadbg;
        if (udbg && vaddr >= 0xF0000000u && vaddr < 0xF8000000u) {
            static uint64_t n = 0;
            if (n++ < 200)
                fprintf(stderr, "[UDATA8DW] write vaddr=0x%08X paddr=0x%08X val=0x%02X '%c' PC=0x%08X\n",
                        vaddr, paddr, value, (value >= 32 && value < 127) ? value : '.', cpu->PC);
        }
    }
    nd500_bus_write8(cpu->machine, paddr, value);
}

uint16_t nd500_read_memory_16_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain) {
    if (!cpu || !cpu->machine) return 0;

    if (nd_crosses_page(cpu, vaddr, 2)) {   /* see nd_crosses_page */
        uint8_t b0 = nd500_read_memory_8_domain(cpu, vaddr, domain);
        if (nd_split_faulted(cpu)) return 0;   /* see nd_split_faulted */
        uint8_t b1 = nd500_read_memory_8_domain(cpu, vaddr + 1, domain);
        return (uint16_t)(((uint16_t)b0 << 8) | (uint16_t)b1);
    }

    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate_domain(cpu, vaddr, 0, 0, domain);
        if (nd500_trap_occurred()) return 0;
    }

    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    uint16_t value = ((uint16_t)b0 << 8) | (uint16_t)b1;
    MEMTRACE_RD("[MEMTRACE] read_16_domain: vaddr=0x%08X paddr=0x%08X domain=%d value=0x%04X\n",
                vaddr, paddr, domain, value);
    return value;
}

void nd500_write_memory_16_domain(Nd500Cpu* cpu, uint32_t vaddr, uint16_t value, uint8_t domain) {
    if (!cpu || !cpu->machine) return;

    if (nd_crosses_page(cpu, vaddr, 2)) {   /* see nd_crosses_page */
        nd500_write_memory_8_domain(cpu, vaddr,     (uint8_t)((value >> 8) & 0xFF), domain);
        if (nd_split_faulted(cpu)) return;   /* see nd_split_faulted */
        nd500_write_memory_8_domain(cpu, vaddr + 1, (uint8_t)(value & 0xFF), domain);
        return;
    }

    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate_domain(cpu, vaddr, 1, 0, domain);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;
    }

    MEMTRACE_WR("[MEMTRACE] write_16_domain: vaddr=0x%08X paddr=0x%08X domain=%d value=0x%04X\n",
                vaddr, paddr, domain, value);
    if (vaddr == 0x30001004u && value != 0 && nd500_settings()->ipcurdbg) {
        static uint64_t wn = 0;
        if (wn++ < 40)
            fprintf(stderr, "[IPCURDBG-D] ip_current := %u  @PC=0x%08X L=0x%08X dom=%d B=0x%08X\n",
                    value, cpu->PC, cpu->L, domain, cpu->B);
    }
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)(value & 0xFF));
}

uint32_t nd500_read_memory_32_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain) {
    if (!cpu || !cpu->machine) return 0;

    if (nd_crosses_page(cpu, vaddr, 4)) {   /* see nd_crosses_page */
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            v = (v << 8) | (uint32_t)nd500_read_memory_8_domain(cpu, vaddr + i, domain);
            if (nd_split_faulted(cpu)) return 0;   /* see nd_split_faulted */
        }
        return v;
    }

    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate_domain(cpu, vaddr, 0, 0, domain);
        if (nd500_trap_occurred()) return 0;
    }

    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, paddr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, paddr + 3);
    uint32_t value = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
                     ((uint32_t)b2 << 8) | (uint32_t)b3;
    MEMTRACE_RD("[MEMTRACE] read_32_domain: vaddr=0x%08X paddr=0x%08X domain=%d value=0x%08X\n",
                vaddr, paddr, domain, value);
    /* _Udata/_Ustack window read trace (env ND500X_UDATADBG): fuword and the
     * execve path read user memory through seg 30/31 via THIS domain variant
     * (the non-domain read_memory_32 probe never fires). */
    {
        static int udbg = -1;
        if (udbg < 0) udbg = nd500_settings()->udatadbg;
        if (udbg && domain == 0 && vaddr >= 0xF0000000u) {
            static uint64_t n = 0;
            if (n++ < 200)
                fprintf(stderr, "[UDATA32D] read vaddr=0x%08X paddr=0x%08X value=0x%08X PC=0x%08X\n",
                        vaddr, paddr, value, cpu->PC);
        }
    }
    return value;
}

void nd500_write_memory_32_domain(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value, uint8_t domain) {
    if (!cpu || !cpu->machine) return;

    if (nd_crosses_page(cpu, vaddr, 4)) {   /* see nd_crosses_page */
        for (int i = 0; i < 4; i++) {
            nd500_write_memory_8_domain(cpu, vaddr + i,
                                        (uint8_t)((value >> (24 - 8 * i)) & 0xFF), domain);
            if (nd_split_faulted(cpu)) return;   /* see nd_split_faulted */
        }
        return;
    }

    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate_domain(cpu, vaddr, 1, 0, domain);
        if (nd500_trap_occurred() || cpu->instr_aborted) return;
    }
    nd_uwatch(cpu, vaddr, paddr, value, 32);

    MEMTRACE_WR("[MEMTRACE] write_32_domain: vaddr=0x%08X paddr=0x%08X domain=%d value=0x%08X\n",
                vaddr, paddr, domain, value);
    { nd500_ptewatch_wr(cpu->PC, vaddr, paddr, value, 32); }
    /* _Udata window WRITE trace (env ND500X_UDATADBG) - see write_8_domain. */
    {
        static int udbg = -1;
        if (udbg < 0) udbg = nd500_settings()->udatadbg;
        if (udbg && vaddr >= 0xF0000000u && vaddr < 0xF8000000u) {
            static uint64_t n = 0;
            if (n++ < 200)
                fprintf(stderr, "[UDATA32DW] write vaddr=0x%08X paddr=0x%08X val=0x%08X PC=0x%08X\n",
                        vaddr, paddr, value, cpu->PC);
        }
    }
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)((value >> 24) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)((value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 2, (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 3, (uint8_t)(value & 0xFF));
}

uint64_t nd500_read_memory_64_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain) {
    uint32_t high = nd500_read_memory_32_domain(cpu, vaddr, domain);
    if (nd_split_faulted(cpu)) return 0;   /* see nd500_read_memory_64 */
    uint32_t low  = nd500_read_memory_32_domain(cpu, vaddr + 4, domain);
    return ((uint64_t)high << 32) | (uint64_t)low;
}

void nd500_write_memory_64_domain(Nd500Cpu* cpu, uint32_t vaddr, uint64_t value, uint8_t domain) {
    nd500_write_memory_32_domain(cpu, vaddr,     (uint32_t)((value >> 32) & 0xFFFFFFFF), domain);
    if (nd_split_faulted(cpu)) return;   /* see nd500_read_memory_64 */
    nd500_write_memory_32_domain(cpu, vaddr + 4, (uint32_t)(value & 0xFFFFFFFF), domain);
}


/* ============================================================================
 * OPERAND ACCESS HELPERS
 * ============================================================================
 */

uint8_t nd500_read_operand_byte(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return (uint8_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_BYTE);
}

uint16_t nd500_read_operand_halfword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return (uint16_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_HALFWORD);
}

uint32_t nd500_read_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return (uint32_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_WORD);
}

uint64_t nd500_read_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return nd500_read_operand_value(cpu, operand, ND500_DTYPE_DOUBLEWORD);
}

bool nd500_temm_allows_change(uint32_t changed_bits, uint32_t temm) {
    /* A change is permitted only if every changed bit is set in TEMM. */
    return (changed_bits & ~temm) == 0;
}

void nd500_write_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint32_t value) {
    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        nd500_write_integer_register(cpu, operand->reg, value);
        return;
    }

    // Memory operand
    nd500_write_memory_32(cpu, operand->effective_address, value);
}


/* ============================================================================
 * REGISTER ACCESS HELPERS
 * ============================================================================
 */

uint32_t nd500_read_integer_register(Nd500Cpu* cpu, uint8_t reg_num) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid integer register number %u\n", reg_num);
        return 0;
    }
    return cpu->I[reg_num - 1];  // I1=I[0], I2=I[1], I3=I[2], I4=I[3]
}

void nd500_write_integer_register(Nd500Cpu* cpu, uint8_t reg_num, uint32_t value) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid integer register number %u\n", reg_num);
        return;
    }
    cpu->I[reg_num - 1] = value;
}

uint64_t nd500_read_register_by_type(Nd500Cpu* cpu, uint8_t reg_num, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_FLOAT:      return nd500_read_float_register(cpu, reg_num);
        case ND500_DTYPE_DOUBLEWORD: return nd500_read_double_register(cpu, reg_num);
        default:
            return nd500_mask_to_datatype(nd500_read_integer_register(cpu, reg_num), dtype);
    }
}

uint32_t nd500_read_float_register(Nd500Cpu* cpu, uint8_t reg_num) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid float register number %u\n", reg_num);
        return 0;
    }
    return cpu->A[reg_num - 1];  // A1=A[0], A2=A[1], A3=A[2], A4=A[3]
}

void nd500_write_float_register(Nd500Cpu* cpu, uint8_t reg_num, uint32_t value) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid float register number %u\n", reg_num);
        return;
    }
    cpu->A[reg_num - 1] = value;
}

uint64_t nd500_read_double_register(Nd500Cpu* cpu, uint8_t reg_num) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid double register number %u\n", reg_num);
        return 0;
    }
    /* Dn = An:En with An the HIGH half and En the low half.
     *
     * Evidence (primary, 1988): the dbx opcode table
     * baseline/ucb/dbx/ops.nd500 gives "neg f" and "neg d" the SAME octal
     * opcode 0224 (0x94) - one instruction serves both.  That is only
     * possible if a float and a double keep their sign bit in the same
     * physical register, i.e. An.  It also matches the data formats: an
     * ND-500 single (sign|exp9|mant22) is bit-for-bit the top 32 bits of an
     * ND-500 double (sign|exp9|mant54) - verified against the 1988 libc,
     * where genlib/atof.o stores 10.0 as 0x4110000000000000 and the
     * assembler emits .float 10.0 as 0x41100000.
     *
     * With the halves swapped, "neg" on a negative double flipped a bit in
     * the mantissa's low word instead of the sign, so libc's iszero_d()
     * never took the absolute value and ecvt() decided every value was
     * "close enough" to every power of ten - printf("%f") printed a 170-digit
     * run of 1000...000 for any double. */
    uint32_t a_val = cpu->A[reg_num - 1];
    uint32_t e_val = cpu->E[reg_num - 1];
    return ((uint64_t)a_val << 32) | (uint64_t)e_val;
}

void nd500_write_double_register(Nd500Cpu* cpu, uint8_t reg_num, uint64_t value) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid double register number %u\n", reg_num);
        return;
    }
    /* Split 64-bit value into A (high half) and E (low half) - see
     * nd500_read_double_register for why An is the high half. */
    cpu->A[reg_num - 1] = (uint32_t)((value >> 32) & 0xFFFFFFFF);
    cpu->E[reg_num - 1] = (uint32_t)(value & 0xFFFFFFFF);
}


/* ============================================================================
 * FLAG MANIPULATION HELPERS
 * ============================================================================
 */

void nd500_set_flags_zs(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype) {
    /* Z and S are computed below; C and O are cleared because every data
     * status bit an instruction's list does not name is reset (manual 6.5.1).
     * Callers that name C or O set them after this. */
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);

    // Set Z flag if value is zero (masked to data type)
    uint64_t mask = 0;
    bool sign_bit = false;

    switch (dtype) {
        case ND500_DTYPE_BIT:
            /* Single bit: mask=1, no sign bit */
            mask = 0x1;
            sign_bit = false;  /* A single bit has no sign */
            break;
        case ND500_DTYPE_BYTE:
            mask = 0xFF;
            sign_bit = (value & 0x80) != 0;
            break;
        case ND500_DTYPE_HALFWORD:
            mask = 0xFFFF;
            sign_bit = (value & 0x8000) != 0;
            break;
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_FLOAT:
            /* F is the 32-bit pattern here: a float with exponent 0 but a
             * nonzero sign or mantissa is not "= 0" for a store. */
            mask = 0xFFFFFFFF;
            sign_bit = (value & 0x80000000) != 0;
            break;
        case ND500_DTYPE_DOUBLEWORD:
            mask = 0xFFFFFFFFFFFFFFFFULL;
            sign_bit = (value & 0x8000000000000000ULL) != 0;
            break;
    }

    if ((value & mask) == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    }

    // Set S flag if sign bit is set
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    }
}

void nd500_set_flags_zs_float(Nd500Cpu* cpu, uint64_t value, bool is_double) {
    /* Z and S are computed below; C and O are cleared (manual 6.5.1, as in
     * nd500_set_flags_zs). */
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);

    if (is_double) {
        // 64-bit double: sign bit at position 63, magnitude is bits 62:0
        bool sign_bit = (value & 0x8000000000000000ULL) != 0;
        bool is_zero = (value & 0x7FFFFFFFFFFFFFFFULL) == 0;  // +0 or -0

        if (is_zero) {
            cpu->ST1 |= ND500_FLAG_Z;
        }
        if (sign_bit) {
            cpu->ST1 |= ND500_FLAG_S;
        }
    } else {
        // 32-bit float: sign bit at position 31, magnitude is bits 30:0
        bool sign_bit = (value & 0x80000000) != 0;
        bool is_zero = (value & 0x7FFFFFFF) == 0;  // +0 or -0

        if (is_zero) {
            cpu->ST1 |= ND500_FLAG_Z;
        }
        if (sign_bit) {
            cpu->ST1 |= ND500_FLAG_S;
        }
    }
}

uint64_t nd500_float_finish(Nd500Cpu* cpu, uint32_t pc, double result, bool is_double) {
    /* Shared tail for floating-point arithmetic instructions (ADD3/SUB2/SUB3/
     * MUL2/MUL3/MULAD/DIV2/DIV3 float paths). Microcode ST,SAVF sets Z,S and
     * conditionally FU,FO; the Reference "unlisted-bit" rule (4040) clears every
     * data-status bit the instruction does not name, so C and O are CLEARED.
     * Returns the ND-500 result bits (float in the low 32, double in the 64)
     * for the caller to store to its destination (operand or register). */

    /* Convert the result to ND-500 NATIVE float/double bits via the full-range
     * codec, which also reports floating overflow/underflow by the native exponent
     * range (Reference Manual: a signed exponent requiring more than 9 bits ->
     * e_field > 511 = FO, e_field < 1 = FU). A non-finite host result (inf/NaN)
     * is always an overflow. This replaces the earlier host-double isinf/1e-38
     * heuristic, which used the narrower IEEE range instead of the native one. */
    bool codec_ovfl = false;
    bool codec_unfl = false;
    uint64_t result_bits = is_double
        ? nd500_native_double_from_double(result, &codec_ovfl, &codec_unfl)
        : (uint64_t)nd500_native_single_from_double(result, &codec_ovfl, &codec_unfl);

    bool fovfl = codec_ovfl || isinf(result) || isnan(result);
    bool funfl = (!fovfl && codec_unfl);

    /* STATUS: Z,S from result; C,O cleared (rule 4040); FU,FO conditional */
    nd500_set_flags_zs_float(cpu, result_bits, is_double);   /* also clears C and O */
    if (fovfl) {
        cpu->ST1 |= ND500_FLAG_FO;
    } else {
        cpu->ST1 &= ~ND500_FLAG_FO;
    }
    if (funfl) {
        cpu->ST1 |= ND500_FLAG_FU;
    } else {
        cpu->ST1 &= ~ND500_FLAG_FU;
    }

    /* TRAP: Floating overflow (FO) / Floating underflow (FU) */
    if (fovfl) {
        trap_floating_overflow(cpu, pc);
    } else if (funfl) {
        trap_floating_underflow(cpu, pc);
    }

    return result_bits;
}

void nd500_set_flags_zsc(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype, bool carry) {
    // Set Z and S flags
    nd500_set_flags_zs(cpu, value, dtype);

    // Set or clear C flag
    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }
}

void nd500_set_flags_zsco(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype, bool carry, bool overflow) {
    // Set Z, S, and C flags
    nd500_set_flags_zsc(cpu, value, dtype, carry);

    // Set or clear O flag
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }
}

void nd500_clear_all_flags(Nd500Cpu* cpu) {
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_C | ND500_FLAG_S |
                  ND500_FLAG_K | ND500_FLAG_O);
}

void nd500_set_flag(Nd500Cpu* cpu, uint32_t flag_mask) {
    cpu->ST1 |= flag_mask;
}

void nd500_clear_flag(Nd500Cpu* cpu, uint32_t flag_mask) {
    cpu->ST1 &= ~flag_mask;
}

bool nd500_test_flag(Nd500Cpu* cpu, uint32_t flag_mask) {
    return (cpu->ST1 & flag_mask) != 0;
}


/* ============================================================================
 * ARITHMETIC HELPERS
 * ============================================================================
 */

int32_t nd500_sign_extend_6bit(uint8_t value) {
    // Sign extend 6-bit to 32-bit
    // Bit 5 is the sign bit (values 0x20-0x3F are negative: -32 to -1)
    if (value & 0x20) {
        // Negative: extend with 1s
        return (int32_t)(value | 0xFFFFFFC0);
    }
    return (int32_t)(value & 0x3F);
}

int32_t nd500_sign_extend_byte(uint8_t value) {
    // Sign extend 8-bit to 32-bit
    return (int32_t)((int8_t)value);
}

int32_t nd500_sign_extend_halfword(uint16_t value) {
    // Sign extend 16-bit to 32-bit
    return (int32_t)((int16_t)value);
}

int64_t nd500_sign_extend_word(uint32_t value) {
    // Sign extend 32-bit to 64-bit
    return (int64_t)((int32_t)value);
}

uint8_t nd500_mask_to_byte(uint64_t value) {
    return (uint8_t)(value & 0xFF);
}

uint16_t nd500_mask_to_halfword(uint64_t value) {
    return (uint16_t)(value & 0xFFFF);
}

uint32_t nd500_mask_to_word(uint64_t value) {
    return (uint32_t)(value & 0xFFFFFFFF);
}

bool nd500_detect_add_overflow(uint64_t a, uint64_t b, uint64_t result, Nd500DataType dtype) {
    // Overflow occurs when two operands with the same sign produce a result with different sign
    // Check sign bits based on data type

    bool a_sign, b_sign, r_sign;

    switch (dtype) {
        case ND500_DTYPE_BYTE:
            a_sign = (a & 0x80) != 0;
            b_sign = (b & 0x80) != 0;
            r_sign = (result & 0x80) != 0;
            break;
        case ND500_DTYPE_HALFWORD:
            a_sign = (a & 0x8000) != 0;
            b_sign = (b & 0x8000) != 0;
            r_sign = (result & 0x8000) != 0;
            break;
        case ND500_DTYPE_WORD:
            a_sign = (a & 0x80000000) != 0;
            b_sign = (b & 0x80000000) != 0;
            r_sign = (result & 0x80000000) != 0;
            break;
        case ND500_DTYPE_DOUBLEWORD:
            a_sign = (a & 0x8000000000000000ULL) != 0;
            b_sign = (b & 0x8000000000000000ULL) != 0;
            r_sign = (result & 0x8000000000000000ULL) != 0;
            break;
        default:
            return false;
    }

    // Overflow if both operands have same sign but result has different sign
    return (a_sign == b_sign) && (a_sign != r_sign);
}

bool nd500_detect_sub_overflow(uint64_t a, uint64_t b, uint64_t result, Nd500DataType dtype) {
    // Overflow occurs when subtracting operands with different signs produces result with wrong sign
    // result = a - b

    bool a_sign, b_sign, r_sign;

    switch (dtype) {
        case ND500_DTYPE_BYTE:
            a_sign = (a & 0x80) != 0;
            b_sign = (b & 0x80) != 0;
            r_sign = (result & 0x80) != 0;
            break;
        case ND500_DTYPE_HALFWORD:
            a_sign = (a & 0x8000) != 0;
            b_sign = (b & 0x8000) != 0;
            r_sign = (result & 0x8000) != 0;
            break;
        case ND500_DTYPE_WORD:
            a_sign = (a & 0x80000000) != 0;
            b_sign = (b & 0x80000000) != 0;
            r_sign = (result & 0x80000000) != 0;
            break;
        case ND500_DTYPE_DOUBLEWORD:
            a_sign = (a & 0x8000000000000000ULL) != 0;
            b_sign = (b & 0x8000000000000000ULL) != 0;
            r_sign = (result & 0x8000000000000000ULL) != 0;
            break;
        default:
            return false;
    }

    // Overflow if operands have different signs and result has same sign as subtrahend
    return (a_sign != b_sign) && (b_sign == r_sign);
}

bool nd500_is_negative(uint64_t value, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return (value & 0x80) != 0;
        case ND500_DTYPE_HALFWORD:
            return (value & 0x8000) != 0;
        case ND500_DTYPE_WORD:
            return (value & 0x80000000) != 0;
        case ND500_DTYPE_DOUBLEWORD:
            return (value & 0x8000000000000000ULL) != 0;
        default:
            return false;
    }
}

int64_t nd500_sign_extend_by_dtype(uint64_t value, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return nd500_sign_extend_byte((uint8_t)value);
        case ND500_DTYPE_HALFWORD:
            return nd500_sign_extend_halfword((uint16_t)value);
        case ND500_DTYPE_WORD:
            return nd500_sign_extend_word((uint32_t)value);
        case ND500_DTYPE_DOUBLEWORD:
            /* Already 64-bit, just reinterpret as signed */
            return (int64_t)value;
        default:
            return (int64_t)value;
    }
}

bool nd500_detect_carry_add(uint64_t result, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return (result > 0xFF);
        case ND500_DTYPE_HALFWORD:
            return (result > 0xFFFF);
        case ND500_DTYPE_WORD:
            return (result > 0xFFFFFFFF);
        case ND500_DTYPE_DOUBLEWORD:
            /* For 64-bit, overflow cannot be detected this way - need different method */
            return false;  /* TODO: Implement properly if needed */
        default:
            return false;
    }
}

/* ============================================================================
 * C# COMPATIBILITY HELPERS (match RetroCore InstructionHelpers.cs)
 * ============================================================================
 */

uint32_t nd500_mask_to_datatype(uint64_t value, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BIT:
            /* BI (bit): a bit result written to an I register keeps ONLY bit 0, upper bits
             * zero-filled (ND-500: "Bit -> I registers, only LSB significant, zero-fill upper").
             * Was falling through to the unmasked default, so BI AND/OR/XOR wrote the full 32-bit
             * value instead of 0/1 (e.g. BI1 AND /bin/bashx55555555 left 0x55555555 instead of 1). Matches
             * the C# functional MaskToDataType(BI). Comp/Comp2 do NOT rely on this (Comp2 has its own
             * width_mask; Comp takes the BI sign from the raw result, Z from the masked result). */
            return (uint32_t)(value & 0x1);
        case ND500_DTYPE_BYTE:
            return (uint32_t)(value & 0xFF);
        case ND500_DTYPE_HALFWORD:
            return (uint32_t)(value & 0xFFFF);
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_DOUBLEWORD:
            return (uint32_t)value;  /* W, F, D - no masking needed for 32-bit */
        default:
            return (uint32_t)value;
    }
}

uint64_t nd500_read_operand_value(Nd500Cpu* cpu, const Nd500OperandDecoded* op, Nd500DataType dtype) {
    /* Handle CONSTANT_SHORT - value embedded in address code (low 6 bits)
     * The 6-bit value is SIGNED: bit 5 is sign bit
     * Values 0x00-0x1F = 0 to 31 (positive)
     * Values 0x20-0x3F = -32 to -1 (negative, sign-extended) */
    if (op->mode == ND500_ADDR_CONSTANT_SHORT) {
        uint8_t raw_value = op->address_code & 0x3F;
        int32_t signed_value = nd500_sign_extend_6bit(raw_value);
        return (uint64_t)(uint32_t)signed_value;
    }

    /* Handle CONSTANT - value in data array (BIG-ENDIAN per ND-500 spec)
     *
     * Per ND-500 Reference Manual Table 13 "Treatment of constants as operands":
     * Constants must be SIGN-EXTENDED (SX) when constant size < operation type size.
     *
     * | Instruction type | :S (6-bit) | :B (byte) | :H (half) | :W (word) |
     * |------------------|------------|-----------|-----------|-----------|
     * | BY               | SX         | NC        | IOS       | IOS       |
     * | H                | SX         | SX        | NC        | IOS       |
     * | W                | SX         | SX        | SX        | NC        |
     *
     * SX = sign extended, NC = no conversion, IOS = illegal operand specifier
     *
     * Note: "SX - sign extended (unless instruction calls for unsigned)"
     * Even for unsigned operations (UMUL, UDIV), constants are first sign-extended,
     * then the result is treated as unsigned by the instruction.
     */
    if (op->mode == ND500_ADDR_CONSTANT) {
        /* First, read the raw value (big-endian) */
        uint64_t raw_val = 0;
        switch (op->data_len) {
            case 1:
                raw_val = op->data[0];
                break;
            case 2:
                raw_val = (uint16_t)((op->data[0] << 8) | op->data[1]);
                break;
            case 4:
                raw_val = (uint32_t)((op->data[0] << 24) | (op->data[1] << 16) |
                                    (op->data[2] << 8) | op->data[3]);
                break;
            case 8:
                raw_val = ((uint64_t)op->data[0] << 56) | ((uint64_t)op->data[1] << 48) |
                          ((uint64_t)op->data[2] << 40) | ((uint64_t)op->data[3] << 32) |
                          ((uint64_t)op->data[4] << 24) | ((uint64_t)op->data[5] << 16) |
                          ((uint64_t)op->data[6] << 8) | (uint64_t)op->data[7];
                break;
            default:
                return 0;
        }

        /* Apply sign extension per Table 13 when constant size < dtype size */
        switch (dtype) {
            case ND500_DTYPE_DOUBLEWORD:
                /* D operations: byte/half/word constants get 32LZ (zero fill), not SX */
                /* Per Table 13: D with :W or :F = 32LZ (32 least significant bits zero filled) */
                /* For simplicity, just return raw value - D operations are rare with small constants */
                return raw_val;

            case ND500_DTYPE_WORD:
                /* W operations: SX for byte and halfword constants */
                if (op->data_len == 1) {
                    /* Sign-extend byte to word */
                    return (uint64_t)(uint32_t)(int32_t)(int8_t)raw_val;
                }
                if (op->data_len == 2) {
                    /* Sign-extend halfword to word */
                    return (uint64_t)(uint32_t)(int32_t)(int16_t)raw_val;
                }
                return raw_val;  /* NC for word constant */

            case ND500_DTYPE_HALFWORD:
                /* H operations: SX for byte constants */
                if (op->data_len == 1) {
                    /* Sign-extend byte to halfword */
                    return (uint64_t)(uint16_t)(int16_t)(int8_t)raw_val;
                }
                return raw_val;  /* NC for halfword constant */

            case ND500_DTYPE_BYTE:
                /* BY operations: NC for byte constant */
                return raw_val;

            default:
                return raw_val;
        }
    }

    /* Handle registers - dispatch to correct register bank based on data type
     * Per ND-500 Reference Manual:
     * - Integer types (BI, BY, H, W) use I1-I4 registers
     * - Float type (F) uses A1-A4 registers
     * - Double type (D) uses D1-D4 registers (A+E pairs)
     */
    if (op->mode == ND500_ADDR_REGISTER) {
        switch (dtype) {
            case ND500_DTYPE_FLOAT:
                return nd500_read_float_register(cpu, op->reg);
            case ND500_DTYPE_DOUBLEWORD:
                return nd500_read_double_register(cpu, op->reg);
            case ND500_DTYPE_BIT:
                /* BIT type from register: return LSB of integer register */
                return nd500_read_integer_register(cpu, op->reg) & 1;
            default:
                /* BYTE, HALFWORD, WORD -> integer registers */
                return nd500_read_integer_register(cpu, op->reg);
        }
    }

    /* Handle memory operands
     *
     * ALT prefix support: When has_alt_prefix is set, use CAD (Current Alternative
     * Domain) instead of CED (Current Executing Domain) for MMU translation.
     * This enables cross-domain data access.
     *
     * Reference: ND-500 Reference Manual, Chapter 6 (Domain System)
     */
    uint8_t domain = (op->has_alt_prefix && cpu) ? cpu->CAD : (cpu ? cpu->CED : 0);

    switch (dtype) {
        case ND500_DTYPE_BIT: {
            /* BIT type: read single bit from memory
             * Per ND-500 Reference Manual 7.2.1:
             * "The specified bit is the rightmost bit (bit 0, the least
             * significant bit) in the addressed byte."
             *
             * effective_address points to the byte containing the bit
             * bit_position (0-7) indicates which bit within that byte
             */
            uint8_t byte = nd500_read_memory_8_domain(cpu, op->effective_address, domain);
            return (byte >> op->bit_position) & 1;  /* Extract single bit (0 or 1) */
        }
        case ND500_DTYPE_BYTE:
            return nd500_read_memory_8_domain(cpu, op->effective_address, domain);
        case ND500_DTYPE_HALFWORD:
            return nd500_read_memory_16_domain(cpu, op->effective_address, domain);
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_FLOAT:
            return nd500_read_memory_32_domain(cpu, op->effective_address, domain);
        case ND500_DTYPE_DOUBLEWORD:
            return nd500_read_memory_64_domain(cpu, op->effective_address, domain);
        default:
            return 0;
    }
}

void nd500_write_operand_value(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint64_t value, Nd500DataType dtype) {
    /* Constants are not writable */
    if (op->mode == ND500_ADDR_CONSTANT || op->mode == ND500_ADDR_CONSTANT_SHORT) {
        return;
    }

    /* Handle registers - dispatch to correct register bank based on data type
     * Per ND-500 Reference Manual (page 901):
     * - I1-I4: "32-bit general registers for word and partial word operations"
     * - A1-A4: "32-bit floating-point accumulators for real number arithmetic"
     * - D1-D4 (A+E): "64-bit floating point accumulators for double precision"
     *
     * Also: "When using the integer registers for BIt, BYte and Halfword, the
     * unused upper part of the register is always zero-filled rather than
     * sign-extended when data is loaded to the register." */
    if (op->mode == ND500_ADDR_REGISTER) {
        switch (dtype) {
            case ND500_DTYPE_FLOAT:
                /* Float -> A1-A4 registers */
                nd500_write_float_register(cpu, op->reg, (uint32_t)value);
                return;
            case ND500_DTYPE_DOUBLEWORD:
                /* Double -> D1-D4 registers (A+E pairs) */
                nd500_write_double_register(cpu, op->reg, value);
                return;
            case ND500_DTYPE_BIT:
                /* Bit -> I registers, only LSB is significant, zero-fill upper bits */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)(value & 1));
                return;
            case ND500_DTYPE_BYTE:
                /* Byte -> I registers, zero-fill upper bits */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)(value & 0xFF));
                return;
            case ND500_DTYPE_HALFWORD:
                /* Halfword -> I registers, zero-fill upper bits */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)(value & 0xFFFF));
                return;
            default:
                /* Word -> I registers */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)value);
                return;
        }
    }

    /* Handle memory operands
     *
     * ALT prefix support: When has_alt_prefix is set, use CAD (Current Alternative
     * Domain) instead of CED (Current Executing Domain) for MMU translation.
     * This enables cross-domain data access.
     *
     * Reference: ND-500 Reference Manual, Chapter 6 (Domain System)
     */
    uint8_t domain = (op->has_alt_prefix && cpu) ? cpu->CAD : (cpu ? cpu->CED : 0);

    switch (dtype) {
        case ND500_DTYPE_BIT: {
            /* BIT type: write single bit to memory (read-modify-write)
             * Per ND-500 Reference Manual 7.2.1:
             * "The specified bit is the rightmost bit (bit 0, the least
             * significant bit) in the addressed byte."
             *
             * effective_address points to the byte containing the bit
             * bit_position (0-7) indicates which bit within that byte
             * value is treated as 0 or non-zero to clear/set the bit
             */
            uint8_t byte = nd500_read_memory_8_domain(cpu, op->effective_address, domain);
            if (value & 1) {
                byte |= (1 << op->bit_position);   /* Set bit */
            } else {
                byte &= ~(1 << op->bit_position);  /* Clear bit */
            }
            nd500_write_memory_8_domain(cpu, op->effective_address, byte, domain);
            break;
        }
        case ND500_DTYPE_BYTE:
            nd500_write_memory_8_domain(cpu, op->effective_address, (uint8_t)value, domain);
            break;
        case ND500_DTYPE_HALFWORD:
            nd500_write_memory_16_domain(cpu, op->effective_address, (uint16_t)value, domain);
            break;
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_FLOAT:
            nd500_write_memory_32_domain(cpu, op->effective_address, (uint32_t)value, domain);
            break;
        case ND500_DTYPE_DOUBLEWORD:
            nd500_write_memory_64_domain(cpu, op->effective_address, value, domain);
            break;
        default:
            break;
    }
}


/* ============================================================================
 * STACK OPERATION HELPERS
 * ============================================================================
 */

void nd500_push_word(Nd500Cpu* cpu, uint32_t value) {
    nd500_write_memory_32(cpu, cpu->L, value);
    cpu->L += 4;
}

uint32_t nd500_pop_word(Nd500Cpu* cpu) {
    cpu->L -= 4;
    return nd500_read_memory_32(cpu, cpu->L);
}

void nd500_push_doubleword(Nd500Cpu* cpu, uint64_t value) {
    nd500_write_memory_64(cpu, cpu->L, value);
    cpu->L += 8;
}

uint64_t nd500_pop_doubleword(Nd500Cpu* cpu) {
    cpu->L -= 8;
    return nd500_read_memory_64(cpu, cpu->L);
}


/* ============================================================================
 * BITFIELD OPERATION HELPERS
 * ============================================================================
 */

uint32_t nd500_extract_bitfield(uint32_t value, uint8_t offset, uint8_t width) {
    if (width == 0 || width > 32 || offset > 31) {
        return 0;
    }

    /* Create mask with 'width' bits set */
    uint32_t mask = (width == 32) ? 0xFFFFFFFF : ((1u << width) - 1);

    /* Shift value right by offset and mask */
    return (value >> offset) & mask;
}

uint32_t nd500_insert_bitfield(uint32_t dest, uint32_t src, uint8_t offset, uint8_t width) {
    if (width == 0 || width > 32 || offset > 31) {
        return dest;
    }

    /* Create mask with 'width' bits set */
    uint32_t mask = (width == 32) ? 0xFFFFFFFF : ((1u << width) - 1);

    /* Clear destination bits, mask source, shift and OR */
    dest &= ~(mask << offset);      /* Clear dest bits */
    src &= mask;                     /* Mask source */
    dest |= (src << offset);         /* Insert */

    return dest;
}

void nd500_set_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset) {
    uint8_t byte_val = nd500_read_memory_8(cpu, addr);
    byte_val |= (1u << bit_offset);
    nd500_write_memory_8(cpu, addr, byte_val);
}

void nd500_clear_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset) {
    uint8_t byte_val = nd500_read_memory_8(cpu, addr);
    byte_val &= ~(1u << bit_offset);
    nd500_write_memory_8(cpu, addr, byte_val);
}

bool nd500_test_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset) {
    uint8_t byte_val = nd500_read_memory_8(cpu, addr);
    return (byte_val & (1u << bit_offset)) != 0;
}


/* ============================================================================
 * LOOP OPERATION HELPERS
 * ============================================================================
 */

bool nd500_loop_decrement_and_test(uint32_t* counter) {
    (*counter)--;
    return (*counter != 0);
}


/* ============================================================================
 * PACKED ARITHMETIC HELPERS
 * ============================================================================
 */

uint32_t nd500_packed_add(uint32_t a, uint32_t b, Nd500DataType element_type) {
    uint32_t result = 0;

    switch (element_type) {
        case ND500_DTYPE_BYTE: {
            /* 4 bytes per word - add each byte independently */
            for (int i = 0; i < 4; i++) {
                uint8_t a_byte = (a >> (i * 8)) & 0xFF;
                uint8_t b_byte = (b >> (i * 8)) & 0xFF;
                uint8_t sum = a_byte + b_byte;
                result |= ((uint32_t)sum << (i * 8));
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            /* 2 halfwords per word - add each halfword independently */
            uint16_t a_lo = a & 0xFFFF;
            uint16_t a_hi = (a >> 16) & 0xFFFF;
            uint16_t b_lo = b & 0xFFFF;
            uint16_t b_hi = (b >> 16) & 0xFFFF;

            result = ((uint32_t)(a_lo + b_lo) & 0xFFFF) |
                    (((uint32_t)(a_hi + b_hi) & 0xFFFF) << 16);
            break;
        }
        default:
            result = a + b;  /* Fallback to normal add */
            break;
    }

    return result;
}

uint32_t nd500_packed_sub(uint32_t a, uint32_t b, Nd500DataType element_type) {
    uint32_t result = 0;

    switch (element_type) {
        case ND500_DTYPE_BYTE: {
            /* 4 bytes per word - subtract each byte independently */
            for (int i = 0; i < 4; i++) {
                uint8_t a_byte = (a >> (i * 8)) & 0xFF;
                uint8_t b_byte = (b >> (i * 8)) & 0xFF;
                uint8_t diff = a_byte - b_byte;
                result |= ((uint32_t)diff << (i * 8));
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            /* 2 halfwords per word - subtract each halfword independently */
            uint16_t a_lo = a & 0xFFFF;
            uint16_t a_hi = (a >> 16) & 0xFFFF;
            uint16_t b_lo = b & 0xFFFF;
            uint16_t b_hi = (b >> 16) & 0xFFFF;

            result = ((uint32_t)(a_lo - b_lo) & 0xFFFF) |
                    (((uint32_t)(a_hi - b_hi) & 0xFFFF) << 16);
            break;
        }
        default:
            result = a - b;  /* Fallback to normal subtract */
            break;
    }

    return result;
}

uint32_t nd500_packed_mul(uint32_t a, uint32_t b, Nd500DataType element_type) {
    uint32_t result = 0;

    switch (element_type) {
        case ND500_DTYPE_BYTE: {
            /* 4 bytes per word - multiply each byte independently */
            for (int i = 0; i < 4; i++) {
                uint8_t a_byte = (a >> (i * 8)) & 0xFF;
                uint8_t b_byte = (b >> (i * 8)) & 0xFF;
                uint8_t prod = a_byte * b_byte;  /* Lower 8 bits of product */
                result |= ((uint32_t)prod << (i * 8));
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            /* 2 halfwords per word - multiply each halfword independently */
            uint16_t a_lo = a & 0xFFFF;
            uint16_t a_hi = (a >> 16) & 0xFFFF;
            uint16_t b_lo = b & 0xFFFF;
            uint16_t b_hi = (b >> 16) & 0xFFFF;

            result = ((uint32_t)(a_lo * b_lo) & 0xFFFF) |
                    (((uint32_t)(a_hi * b_hi) & 0xFFFF) << 16);
            break;
        }
        default:
            result = a * b;  /* Fallback to normal multiply */
            break;
    }

    return result;
}

/* ============================================================================
 * STATUS REGISTER AND PRIVILEGE CHECKING
 * ============================================================================
 */

/**
 * Check if CPU is in privileged mode (PIA bit set in status register)
 */
bool nd500_is_privileged(Nd500Cpu* cpu) {
    if (!cpu) return false;

    /* PIA is bit 1 in status register (ST1, bits 0-31) */
    return (cpu->ST1 & (1U << ND500_ST_BIT_PIA)) != 0;
}

/**
 * Get status register bit value
 * Reads from 64-bit status register (ST1 = bits 0-31, ST2 = bits 32-63)
 */
bool nd500_get_status_bit(Nd500Cpu* cpu, uint8_t bit_number) {
    if (!cpu || bit_number >= 64) return false;

    if (bit_number < 32) {
        /* Bits 0-31 from ST1 - reuse existing flag function */
        return nd500_test_flag(cpu, 1U << bit_number);
    } else {
        /* Bits 32-63 from ST2 */
        return (cpu->ST2 & (1U << (bit_number - 32))) != 0;
    }
}

/**
 * Set status register bit
 */
void nd500_set_status_bit(Nd500Cpu* cpu, uint8_t bit_number) {
    if (!cpu || bit_number >= 64) return;

    if (bit_number < 32) {
        /* Bits 0-31 in ST1 - reuse existing flag function */
        nd500_set_flag(cpu, 1U << bit_number);
    } else {
        /* Bits 32-63 in ST2 */
        cpu->ST2 |= (1U << (bit_number - 32));
    }
}

/**
 * Clear status register bit
 */
void nd500_clear_status_bit(Nd500Cpu* cpu, uint8_t bit_number) {
    if (!cpu || bit_number >= 64) return;

    if (bit_number < 32) {
        /* Bits 0-31 in ST1 - reuse existing flag function */
        nd500_clear_flag(cpu, 1U << bit_number);
    } else {
        /* Bits 32-63 in ST2 */
        cpu->ST2 &= ~(1U << (bit_number - 32));
    }
}

/**
 * Check privilege and trap if not privileged
 * Returns true if privileged (safe to continue), false if trapped
 */
bool nd500_require_privilege(Nd500Cpu* cpu, uint32_t pc) {
    if (!cpu) return false;
    if (!nd500_is_privileged(cpu) && nd500_settings()->privdbg)
        fprintf(stderr, "[PRIVDBG] require_privilege FAIL @PC=0x%08X ST1=0x%08X CED=%u CAD=%u inH=%d\n",
                pc, cpu->ST1, cpu->CED, cpu->CAD, cpu->in_trap_handler);

    /* Check PIA bit (bit 1 in ST1) */
    if (!nd500_is_privileged(cpu)) {
        /* Not privileged - raise IIC (Illegal Instruction Code) trap */
        trap_illegal_instruction(cpu, pc, 0);
        return false;  /* Trapped - instruction should not continue */
    }

    return true;  /* Privileged - safe to continue */
}

/* ============================================================================
 * VALIDATION HELPERS (reduce code duplication)
 * ============================================================================
 */

const char* nd500_instr_name_str(Nd500InstrName instr) {
    switch (instr) {
        case INSTR_ADD:      return "ADD";
        case INSTR_SUBTRACT: return "SUBTRACT";
        case INSTR_MULTIPLY: return "MULTIPLY";
        case INSTR_DIVIDE:   return "DIVIDE";
        case INSTR_UDIV:     return "UDIV";
        case INSTR_CLR:      return "CLR";
        case INSTR_NEG:      return "NEG";
        case INSTR_REM:      return "REM";
        case INSTR_AXI:      return "AXI";
        case INSTR_MUL4:     return "MUL4";
        default:             return "UNKNOWN";
    }
}

bool nd500_validate_operand_count(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                   uint8_t expected_count, Nd500InstrName instr) {
    if (fi->operand_count != expected_count) {
        printf("[ERROR] %s at PC=0x%08X: Expected %u operand%s, got %u\n",
               nd500_instr_name_str(instr), fi->address, expected_count,
               expected_count == 1 ? "" : "s", fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return false;
    }
    return true;
}

bool nd500_validate_target_register(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                     Nd500InstrName instr) {
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] %s at PC=0x%08X: Invalid target register %u\n",
               nd500_instr_name_str(instr), fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return false;
    }
    return true;
}


/* ============================================================================
 * STRING DESCRIPTOR IMPLEMENTATION
 * ============================================================================
 */

/**
 * Helper: Read operand value at specific address
 * (Avoids duplication with nd500_read_operand_value which uses Nd500OperandDecoded)
 */
static uint64_t nd500_read_value_at_address(Nd500Cpu* cpu, uint32_t addr, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return nd500_read_memory_8(cpu, addr);
        case ND500_DTYPE_HALFWORD:
            return nd500_read_memory_16(cpu, addr);
        case ND500_DTYPE_WORD:
            return nd500_read_memory_32(cpu, addr);
        case ND500_DTYPE_DOUBLEWORD:
            return nd500_read_memory_64(cpu, addr);
        default:
            return 0;
    }
}

/**
 * Load string descriptor from memory (based on StringDescriptor.LoadFromMemory)
 */
bool nd500_load_string_descriptor(Nd500Cpu* cpu, uint32_t desc_addr,
                                   bool is_bcd_packed, bool is_ascii,
                                   Nd500StringDescriptor* desc) {
    if (!desc) return false;

    // Initialize descriptor
    desc->is_bcd_packed = is_bcd_packed;
    desc->is_ascii = is_ascii;

    // Read 8-byte descriptor from memory (big-endian)
    uint32_t word0 = nd500_read_memory_32(cpu, desc_addr);
    uint32_t word1 = nd500_read_memory_32(cpu, desc_addr + 4);
    desc->element_count = word0;
    desc->base_address = word1;

    // For BCD/ASCII operations, parse additional fields from element count word
    if (is_bcd_packed || is_ascii) {
        uint32_t flags = desc->element_count;

        // Extract sign representation (bits 26-24)
        desc->sign_repr = (Nd500SignRepresentation)((flags >> 24) & 0x07);

        // Extract scaling factor (bits 23-18) - 6-bit signed value
        int scaling = (int)((flags >> 18) & 0x3F);
        if (scaling >= 32) scaling -= 64;  // Convert to signed (-32..+31)
        desc->scaling_factor = (int8_t)scaling;

        // Extract field width (bits 17-13) - 5-bit value
        desc->field_width = (uint8_t)((flags >> 13) & 0x1F);

        // Actual element count is in bits 12-0 (13 bits)
        desc->element_count = flags & 0x1FFF;
    }

    return true;
}

/**
 * Get element address from descriptor (based on StringDescriptor.GetElementAddress)
 */
uint32_t nd500_string_get_element_address(const Nd500StringDescriptor* desc, uint32_t index) {
    if (!desc) return 0;
    if (index >= desc->element_count) return 0;  // Out of range

    if (desc->is_bcd_packed) {
        // For BCD packed, calculate address based on field width (nibbles)
        uint32_t bytes_per_element = (desc->field_width + 1) / 2;  // Round up
        return desc->base_address + (index * bytes_per_element);
    } else if (desc->is_ascii) {
        // For ASCII, each element is typically 1 byte
        return desc->base_address + index;
    } else {
        // Standard descriptor addressing (byte-indexed)
        return desc->base_address + index;
    }
}

/**
 * Read element value from string (based on ReadElementValue)
 * Uses proper element size-based addressing for data type
 */
uint64_t nd500_string_read_element(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                    uint32_t index, Nd500DataType dtype) {
    if (!desc || !cpu) return 0;
    if (index >= desc->element_count) return 0;

    /* BI (bit) arrays are addressed by BIT, not by byte. Per ND-05.009.4 EN
     * (worked example, "Load bit register 2 ... from the bit array BITA"):
     *
     *     byte = base + INT(index/8)
     *     bit  = 7 - REM(index/8)      "Post indexing always counts the data
     *                                   elements from the left"
     *
     * so element i lives in byte base+i/8 at bit 7-(i%8), MSB first. Treating a
     * BI element as a whole byte makes an N-element bit array claim N BYTES
     * instead of N/8 - an 8x overrun. */
    if (dtype == ND500_DTYPE_BIT) {
        uint32_t byte_addr = desc->base_address + (index >> 3);
        uint8_t  bit_no    = (uint8_t)(7 - (index & 7));
        uint8_t  byte_val  = nd500_read_memory_8(cpu, byte_addr);
        return (uint64_t)((byte_val >> bit_no) & 1u);
    }

    /* Calculate address using element size for proper data type handling */
    uint32_t element_size = nd500_get_element_size(dtype);
    uint32_t addr = desc->base_address + (index * element_size);

    return nd500_read_value_at_address(cpu, addr, dtype);
}

/**
 * Get element size in bytes for data type
 */
uint32_t nd500_get_element_size(Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:      return 1;
        /* NOTE: ND500_DTYPE_BIT has NO entry here on purpose. A bit is not a
         * byte and cannot be expressed as a whole-byte element size; BI element
         * access is special-cased in nd500_string_read_element/write_element,
         * which address it as byte base+index/8, bit 7-(index%8). The old
         * comment here claimed "BI uses BYTE width", which silently gave every
         * BI array an 8x overrun. */
        case ND500_DTYPE_HALFWORD:  return 2;
        case ND500_DTYPE_WORD:      return 4;
        case ND500_DTYPE_FLOAT:     return 4;
        case ND500_DTYPE_DOUBLEWORD: return 8;
        default:                    return 1;
    }
}

/**
 * Write element value to string (based on WriteElementValue)
 * Handles different data types (byte, halfword, word, etc.)
 */
void nd500_string_write_element(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                uint32_t index, uint64_t value, Nd500DataType dtype) {
    if (!desc || !cpu) return;
    if (index >= desc->element_count) return;

    /* BI (bit) arrays: read-modify-write a single bit. See the read path for the
     * ND-05.009.4 citation - byte = base + index/8, bit = 7 - (index%8).
     *
     * This is what the ND linker's "bi3 sfill b.0x18" needs: a 64-element BI fill
     * must clear 8 BYTES, not 64. Filling 64 bytes overran the caller's stack
     * frame by 56 bytes and zeroed the pointer in b.0x14, so the routine at
     * 0xB00046D4 then did "setbi r.0x0" through a NULL and took a protect
     * violation ("No data capability! domain=1 segment=0 vaddr=0x00000000"). */
    if (dtype == ND500_DTYPE_BIT) {
        uint32_t byte_addr = desc->base_address + (index >> 3);
        uint8_t  bit_no    = (uint8_t)(7 - (index & 7));
        uint8_t  byte_val  = nd500_read_memory_8(cpu, byte_addr);
        if (value & 1u) byte_val |=  (uint8_t)(1u << bit_no);
        else            byte_val &= (uint8_t)~(1u << bit_no);
        nd500_write_memory_8(cpu, byte_addr, byte_val);
        return;
    }

    uint32_t element_size = nd500_get_element_size(dtype);
    uint32_t addr = desc->base_address + (index * element_size);

    /* base_address comes from the string descriptor and is a VIRTUAL address, so
     * these must go through the translating nd500_write_memory_* helpers - the
     * matching read path (nd500_read_value_at_address) already does. Writing
     * straight to the bus sent the untranslated address past physical memory and
     * the store was silently dropped, so every string move was a no-op. */
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            nd500_write_memory_8(cpu, addr, (uint8_t)(value & 0xFF));
            break;
        case ND500_DTYPE_HALFWORD:
            nd500_write_memory_16(cpu, addr, (uint16_t)(value & 0xFFFF));
            break;
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_FLOAT:
            nd500_write_memory_32(cpu, addr, (uint32_t)(value & 0xFFFFFFFF));
            break;
        case ND500_DTYPE_DOUBLEWORD:
            nd500_write_memory_64(cpu, addr, value);
            break;
        default:
            nd500_write_memory_8(cpu, addr, (uint8_t)(value & 0xFF));
            break;
    }
}

/**
 * Validate string descriptor (based on StringDescriptor.IsValid)
 */
bool nd500_string_descriptor_is_valid(const Nd500StringDescriptor* desc) {
    if (!desc) return false;

    // Basic validation
    if (desc->base_address == 0) return false;

    // Field width validation (for BCD/ASCII)
    if (desc->is_bcd_packed || desc->is_ascii) {
        if (desc->field_width > 31) return false;
        if (desc->field_width == 0) return false;  // Empty operands cause descriptor range trap

        // Scaling factor validation
        if (desc->scaling_factor < -32 || desc->scaling_factor > 31) return false;
    }

    return true;
}

/* ============================================================================
 * BCD (Binary Coded Decimal) IMPLEMENTATION
 * ============================================================================
 */

/**
 * Helper: Check if BCD sign nibble represents negative value
 * Based on IsNegativeBcdSign from C#
 */
static bool nd500_is_negative_bcd_sign(uint8_t sign_nibble) {
    return (sign_nibble == 0x0B || sign_nibble == 0x0D);  // 1011 or 1101
}

/**
 * Helper: Get BCD sign nibble for given sign and representation
 * Based on GetBcdSignNibble from C#
 */
static uint8_t nd500_get_bcd_sign_nibble(bool is_negative, Nd500SignRepresentation sign_rep) {
    if (sign_rep == ND500_SIGN_UNSIGNED) {
        return 0x0F;  // Unsigned
    }
    return is_negative ? 0x0D : 0x0C;  // Minus (1101) or Plus (1100)
}

/**
 * Read packed BCD value from memory using descriptor
 * Based on ReadPackedBcdValue from InstructionHelpers.cs
 *
 * Converts packed BCD nibbles to int64_t value:
 * - Each byte contains 2 nibbles (4-bit BCD digits)
 * - Nibbles must be 0-9 (A-F are invalid and cause IVO trap)
 * - Sign nibble handled based on sign representation
 * - Scaling factor applied as: value * 10^(-scaling_factor)
 */
int64_t nd500_read_packed_bcd_value(Nd500Cpu* cpu, const Nd500StringDescriptor* desc) {
    if (!desc || !cpu) return 0;
    if (!desc->is_bcd_packed) {
        printf("[ERROR] nd500_read_packed_bcd_value: Descriptor is not for BCD packed operations\n");
        return 0;
    }

    if (!nd500_string_descriptor_is_valid(desc)) {
        printf("[ERROR] nd500_read_packed_bcd_value: Invalid BCD descriptor\n");
        trap_invalid_operation(cpu, cpu->PC);
        return 0;
    }

    // Calculate number of bytes needed for BCD field
    // For embedded trailing sign (sign_repr=0), total nibbles = field_width + 1 (for sign)
    // bytes_needed = (total_nibbles + 1) / 2 to round up
    int total_nibbles = desc->field_width + 1;  // +1 for sign nibble
    int bytes_needed = (total_nibbles + 1) / 2;  // Round up to next byte
    if (bytes_needed <= 0 || bytes_needed > 16) {
        printf("[ERROR] nd500_read_packed_bcd_value: Invalid field width %d\n", desc->field_width);
        trap_invalid_operation(cpu, cpu->PC);
        return 0;
    }

    // Read BCD data from memory
    uint8_t bcd_data[16];  // Maximum 16 bytes (32 nibbles)
    for (int i = 0; i < bytes_needed; i++) {
        bcd_data[i] = nd500_read_memory_8(cpu, desc->base_address + i);
    }

    // Convert BCD to decimal (process nibbles from right to left)
    int64_t value = 0;
    int64_t multiplier = 1;
    int nibbles_processed = 0;

    // Process bytes from most significant to least significant
    for (int byte_idx = bytes_needed - 1; byte_idx >= 0; byte_idx--) {
        uint8_t bcd_byte = bcd_data[byte_idx];

        // Process both nibbles in the byte (low nibble first, then high nibble)
        for (int nibble_idx = 0; nibble_idx < 2; nibble_idx++) {
            uint8_t nibble = (bcd_byte >> (nibble_idx * 4)) & 0x0F;

            // Check if this is the sign nibble (rightmost nibble in rightmost byte)
            if (byte_idx == bytes_needed - 1 && nibble_idx == 0) {
                // This is the sign nibble - handle separately
                continue;
            }

            // Validate BCD digit (0-9 only)
            if (nibble > 9) {
                printf("[ERROR] nd500_read_packed_bcd_value: Invalid BCD digit 0x%X at byte %d nibble %d\n",
                       nibble, byte_idx, nibble_idx);
                trap_invalid_operation(cpu, cpu->PC);
                return 0;
            }

            value += nibble * multiplier;
            multiplier *= 10;
            nibbles_processed++;

            // Prevent overflow (max ~18 digits for int64_t)
            if (nibbles_processed > 18) {
                printf("[WARN] nd500_read_packed_bcd_value: BCD value too large, truncating\n");
                break;
            }
        }
    }

    // Handle sign nibble (rightmost nibble in rightmost byte)
    uint8_t sign_nibble = bcd_data[bytes_needed - 1] & 0x0F;
    if (nd500_is_negative_bcd_sign(sign_nibble)) {
        value = -value;
    }

    // Apply scaling factor: value * 10^(-scaling_factor)
    if (desc->scaling_factor != 0) {
        double scale_factor = pow(10.0, -(double)desc->scaling_factor);
        value = (int64_t)((double)value * scale_factor);
    }

    return value;
}

/**
 * Write packed BCD value to memory using descriptor
 * Based on WritePackedBcdValue from InstructionHelpers.cs
 *
 * Converts int64_t value to packed BCD nibbles:
 * - Each byte contains 2 nibbles (4-bit BCD digits)
 * - Sign nibble added based on sign representation
 * - Scaling factor applied as: value * 10^(scaling_factor)
 *
 * Returns true if the value fit in the field, false if BCD overflow occurred.
 * On overflow, the K flag is set and the truncated value is still written.
 */
bool nd500_write_packed_bcd_value(Nd500Cpu* cpu, const Nd500StringDescriptor* desc, int64_t value) {
    if (!desc || !cpu) return false;
    if (!desc->is_bcd_packed) {
        printf("[ERROR] nd500_write_packed_bcd_value: Descriptor is not for BCD packed operations\n");
        return false;
    }

    if (!nd500_string_descriptor_is_valid(desc)) {
        printf("[ERROR] nd500_write_packed_bcd_value: Invalid BCD descriptor\n");
        trap_invalid_operation(cpu, cpu->PC);
        return false;
    }

    // Apply scaling factor: value * 10^(scaling_factor)
    if (desc->scaling_factor != 0) {
        double scale_factor = pow(10.0, (double)desc->scaling_factor);
        value = (int64_t)((double)value * scale_factor);
    }

    // Get absolute value and sign
    bool is_negative = (value < 0);
    int64_t abs_value = is_negative ? -value : value;

    // Calculate number of bytes needed
    int bytes_needed = (desc->field_width + 1) / 2;
    if (bytes_needed <= 0 || bytes_needed > 16) {
        printf("[ERROR] nd500_write_packed_bcd_value: Invalid field width %d\n", desc->field_width);
        trap_invalid_operation(cpu, cpu->PC);
        return false;
    }

    // Calculate max digits available (field_width minus 1 for sign nibble)
    int max_digits = desc->field_width - 1;
    if (max_digits < 0) max_digits = 0;

    // Initialize BCD data buffer
    uint8_t bcd_data[16] = {0};  // Zero-initialize

    // Convert to BCD digits (process from least significant)
    int digit_count = 0;
    int64_t remaining = abs_value;
    while (remaining > 0 && digit_count < max_digits) {
        int digit = remaining % 10;
        // Digits go into positions 1..N (position 0 is sign nibble in last byte)
        int position = digit_count + 1;  // Skip sign position
        int byte_idx = position / 2;
        int nibble_idx = position % 2;

        bcd_data[byte_idx] |= (digit << (nibble_idx * 4));

        remaining /= 10;
        digit_count++;
    }

    // Detect BCD overflow: if remaining > 0, value doesn't fit
    bool overflow = (remaining > 0);
    if (overflow) {
        // Set K flag on BCD overflow (BO -> K per ND-500 spec)
        nd500_set_flag(cpu, ND500_FLAG_K);
        printf("[WARN] nd500_write_packed_bcd_value: BCD overflow - value %lld truncated to fit %d digits\n",
               (long long)abs_value, max_digits);
    }

    // Add sign nibble (rightmost nibble in rightmost byte, position 0)
    int sign_byte = bytes_needed - 1;
    uint8_t sign_nibble = nd500_get_bcd_sign_nibble(is_negative, desc->sign_repr);
    bcd_data[sign_byte] = (bcd_data[sign_byte] & 0xF0) | sign_nibble;  // Sign in low nibble

    // Write BCD data to memory (big-endian: MSB first)
    for (int i = 0; i < bytes_needed; i++) {
        nd500_write_memory_8(cpu, desc->base_address + i, bcd_data[bytes_needed - 1 - i]);
    }

    return !overflow;
}

/**
 * Write packed BCD value to memory with rounding
 * Applies rounding when destination scaling factor causes precision loss.
 * Returns true if value fit in field, false if BCD overflow occurred.
 */
bool nd500_write_packed_bcd_value_rounded(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                          int64_t value, int8_t source_scale) {
    if (!desc || !cpu) return false;

    /* Calculate scale difference: how many decimal places we're losing */
    int scale_diff = source_scale - desc->scaling_factor;

    if (scale_diff > 0) {
        /* We need to reduce precision - apply rounding */
        /* Example: value=12345, source_scale=-3 (12.345), dest_scale=-1 (X.X) */
        /* scale_diff = -3 - (-1) = -2, need to divide by 100 with rounding */
        int64_t divisor = 1;
        for (int i = 0; i < scale_diff; i++) {
            divisor *= 10;
        }

        /* Round half away from zero */
        int64_t half = divisor / 2;
        if (value >= 0) {
            value = (value + half) / divisor;
        } else {
            value = (value - half) / divisor;
        }
    } else if (scale_diff < 0) {
        /* We need to increase precision - multiply */
        int64_t multiplier = 1;
        for (int i = 0; i < -scale_diff; i++) {
            multiplier *= 10;
        }
        value *= multiplier;
    }

    /* Now write the scaled value using the standard function */
    /* Note: Pass value directly since we've already applied scaling */
    /* Temporarily set scaling_factor to 0 to avoid double-scaling */
    Nd500StringDescriptor temp_desc = *desc;
    temp_desc.scaling_factor = 0;
    return nd500_write_packed_bcd_value(cpu, &temp_desc, value);
}

/**
 * Clear string operation flags (S, C, O)
 * Common helper to reduce code duplication in string instructions.
 */
void nd500_string_clear_unused_flags(Nd500Cpu* cpu) {
    nd500_clear_flag(cpu, ND500_FLAG_S);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}

/* ============================================================================
 * FLOATING-POINT CONVERSION (ND-500 <-> IEEE 754 <-> Integer)
 * ============================================================================
 */

/* The native float and double layout constants are in instruction_helpers.h. */

// ---------------------------------------------------------------------------
// Full-range ND-500 NATIVE float/double <-> host-double codec (frexp-based).
//
// Ground truth: ND-500 Reference Manual sections 2.5.1.4 (single) / 2.5.3.6 (double):
//   value = S * M * 2^(e_field - 256),  M in [0.5, 1) with a hidden leading 1,
//   e_field is the unsigned 9-bit exponent, e_field == 0 means EXACTLY zero.
//   Single mantissa 22 bits, double mantissa 54 bits (host double keeps 52 -> the
//   low 2 native-double mantissa bits are always zero, an accepted precision limit).
//
// FU/FO per manual (a signed exponent requiring more than 9 bits):
//   overflow  when e_field would be > 511  -> store max magnitude, keep sign
//   underflow when e_field would be < 1    -> store zero (minus-zero if sign set)
//
// These do NOT go through IEEE single/double bit fields, so they cover the whole
// native range and behave identically in C and C# (no denormal special-casing).
// ---------------------------------------------------------------------------

double nd500_native_single_to_double(uint32_t nd500_bits) {
    uint32_t e_field = (nd500_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    if (e_field == 0) return 0.0;   // exponent all zero == exactly zero
    uint32_t mant = nd500_bits & ND500_FLOAT_MANTISSA_MASK;
    // M = 0.5 + mant * 2^-23  (22 mantissa bits after the "0.1" binary prefix)
    double m = 0.5 + (double)mant / 8388608.0;   // 8388608 = 2^23
    double v = ldexp(m, (int)e_field - ND500_FLOAT_EXPONENT_BIAS);
    return (nd500_bits & ND500_FLOAT_SIGN_MASK) ? -v : v;
}

uint32_t nd500_native_single_from_double(double value, bool* out_overflow, bool* out_underflow) {
    if (out_overflow)  *out_overflow  = false;
    if (out_underflow) *out_underflow = false;
    if (value == 0.0) return 0;
    bool sign = signbit(value);
    uint32_t sign_bit = sign ? ND500_FLOAT_SIGN_MASK : 0u;

    /* Infinity and NaN must be caught BEFORE frexp(). The C standard leaves
     * *exp UNSPECIFIED when the input is not finite, so everything computed
     * from it below is whatever the host's libm happened to leave behind:
     * glibc lands on a huge exponent and falls into the overflow branch, while
     * mingw does not - measured as test_float_arithmetic's "infinity
     * saturates" passing on Linux and failing on Windows with the same source.
     * That was luck, not logic.
     *
     * The ND-500 single format has no encoding for either: 9 exponent bits and
     * 23 mantissa bits, all combinations meaning a finite number. So both map
     * to the largest representable magnitude and report overflow - the same
     * answer the overflow branch below gives, now reached deliberately. NaN
     * keeps the sign it arrived with only because there is nothing better to
     * do with it; no ND program can observe the difference. */
    if (isinf(value) || isnan(value)) {
        if (out_overflow) *out_overflow = true;
        return sign_bit | ND500_FLOAT_EXPONENT_MASK | ND500_FLOAT_MANTISSA_MASK;
    }

    int e;
    double m = frexp(fabs(value), &e);   // |value| = m * 2^e, m in [0.5, 1)
    // mant = round((m - 0.5) * 2^23); a round up to 1.0 renormalizes into the next exponent
    long mant = lround((m - 0.5) * 8388608.0);
    if (mant > (long)ND500_FLOAT_MANTISSA_MASK) { mant = 0; e += 1; }
    int e_field = e + ND500_FLOAT_EXPONENT_BIAS;
    if (e_field > 511) {   // floating overflow -> largest magnitude, keep sign
        if (out_overflow) *out_overflow = true;
        return sign_bit | ND500_FLOAT_EXPONENT_MASK | ND500_FLOAT_MANTISSA_MASK;
    }
    if (e_field < 1) {     // floating underflow -> zero (minus-zero if negative)
        if (out_underflow) *out_underflow = true;
        return sign_bit;
    }
    return sign_bit | ((uint32_t)e_field << ND500_FLOAT_EXPONENT_SHIFT) | (uint32_t)mant;
}

double nd500_native_double_to_double(uint64_t nd500_bits) {
    uint32_t e_field = (uint32_t)((nd500_bits & ND500_DOUBLE_EXPONENT_MASK) >> ND500_DOUBLE_EXPONENT_SHIFT);
    if (e_field == 0) return 0.0;
    uint64_t mant = nd500_bits & ND500_DOUBLE_MANTISSA_MASK;
    // M = 0.5 + mant * 2^-55  (54 mantissa bits)
    double m = 0.5 + (double)mant / 36028797018963968.0;   // 36028797018963968 = 2^55
    double v = ldexp(m, (int)e_field - ND500_DOUBLE_EXPONENT_BIAS);
    return (nd500_bits & ND500_DOUBLE_SIGN_MASK) ? -v : v;
}

uint64_t nd500_native_double_from_double(double value, bool* out_overflow, bool* out_underflow) {
    if (out_overflow)  *out_overflow  = false;
    if (out_underflow) *out_underflow = false;
    if (value == 0.0) return 0;
    bool sign = signbit(value);
    uint64_t sign_bit = sign ? ND500_DOUBLE_SIGN_MASK : 0ull;

    /* Same non-finite guard as the single-precision codec above, and for the
     * same reason: frexp() leaves *exp unspecified for infinity and NaN, and
     * the ND-500 double format has no encoding for either. Saturate to the
     * largest magnitude and report overflow. */
    if (isinf(value) || isnan(value)) {
        if (out_overflow) *out_overflow = true;
        return sign_bit | ND500_DOUBLE_EXPONENT_MASK | ND500_DOUBLE_MANTISSA_MASK;
    }

    int e;
    double m = frexp(fabs(value), &e);   // |value| = m * 2^e, m in [0.5, 1)
    // mant = round((m - 0.5) * 2^55); host double keeps 52 bits so the low 2 stay zero
    long long mant = llround((m - 0.5) * 36028797018963968.0);
    if (mant > (long long)ND500_DOUBLE_MANTISSA_MASK) { mant = 0; e += 1; }
    int e_field = e + ND500_DOUBLE_EXPONENT_BIAS;
    if (e_field > 511) {
        if (out_overflow) *out_overflow = true;
        return sign_bit | ND500_DOUBLE_EXPONENT_MASK | ND500_DOUBLE_MANTISSA_MASK;
    }
    if (e_field < 1) {
        if (out_underflow) *out_underflow = true;
        return sign_bit;
    }
    return sign_bit | ((uint64_t)e_field << ND500_DOUBLE_EXPONENT_SHIFT) | (uint64_t)mant;
}

// Helper: Count leading zeros (for normalization)
static inline int count_leading_zeros_32(uint32_t x) {
    if (x == 0) return 32;
    return __builtin_clz(x);
}

static inline int count_leading_zeros_64(uint64_t x) {
    if (x == 0) return 64;
    return __builtin_clzll(x);
}

/**
 * Convert int32 to ND-500 single precision float
 * Based on ND500Float.FromInt32 from C#
 *
 * Per ND-500 Reference Manual 7.2.5:
 * - Format: sign(1) | exponent(9) | mantissa(22)
 * - Value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^23
 * - M range: 0.5 <= M < 1.0 (implicit 0.1 binary prefix)
 * - Since M is in [0.5, 1.0), we need exp - 256 = highestBit + 1
 * - Therefore: exp = highestBit + 256 + 1 = highestBit + 257
 */
uint32_t nd500_float_from_int32(int32_t value) {
    if (value == 0) return 0;

    // Extract sign
    bool sign = (value < 0);
    uint32_t abs_value = (uint32_t)(sign ? -value : value);

    // Find highest set bit position (0-31)
    int highest_bit = 31 - count_leading_zeros_32(abs_value);

    // Calculate exponent (bias 256)
    // For ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^23
    // Since M is in [0.5, 1.0), we need exp - 256 = highestBit + 1
    // Therefore: exp = highestBit + 256 + 1 = highestBit + 257
    int exponent = highest_bit + ND500_FLOAT_EXPONENT_BIAS + 1;

    // Calculate mantissa: extract bits below the implicit leading 1
    // and shift them to fill the 22-bit mantissa field from the top
    // For value with highest bit at position h, the lower h bits (0 to h-1)
    // need to be placed starting at bit 21 (top of mantissa field)
    // Shift amount: 21 - (highestBit - 1) = 22 - highestBit
    uint32_t mantissa;
    if (highest_bit <= ND500_FLOAT_MANTISSA_BITS) {
        // Value fits - shift left to align lower bits with top of mantissa
        mantissa = (abs_value << (ND500_FLOAT_MANTISSA_BITS - highest_bit)) & ND500_FLOAT_MANTISSA_MASK;
    } else {
        // Value larger than mantissa - shift right and lose precision
        mantissa = (abs_value >> (highest_bit - ND500_FLOAT_MANTISSA_BITS)) & ND500_FLOAT_MANTISSA_MASK;
    }

    // Combine sign, exponent, mantissa
    uint32_t result = 0;
    if (sign) result |= ND500_FLOAT_SIGN_MASK;
    result |= (uint32_t)(exponent << ND500_FLOAT_EXPONENT_SHIFT);
    result |= mantissa;

    return result;
}

/**
 * Convert ND-500 single precision float to int32 (truncated toward zero)
 *
 * Per ND-500 Reference Manual 7.2.5:
 * - Format: sign(1) | exponent(9) | mantissa(22)
 * - Value = sign * 2^(exponent - 256) * M
 * - M = 0.1mmmmm... (binary) where mantissa bits follow implicit 0.1
 * - M range: 0.5 <= M < 1.0
 * - If exponent = 0, value is exactly zero
 */
int32_t nd500_float_to_int32(uint32_t nd500_bits) {
    // Check for zero (exponent = 0 means exactly zero)
    uint32_t exponent = (nd500_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    if (exponent == 0) return 0;

    // Extract components
    bool sign = (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
    uint32_t mantissa = nd500_bits & ND500_FLOAT_MANTISSA_MASK;

    // Calculate actual exponent (e = exponent - 256)
    int actual_exp = (int)exponent - ND500_FLOAT_EXPONENT_BIAS;

    // Build significand with implicit bit at position 22
    // M = 0.1mmmmm... = (1 << 22 | mantissa) / 2^23
    // The implicit 1 is at bit position 22, mantissa fills bits 21-0
    uint32_t significand = (1U << ND500_FLOAT_MANTISSA_BITS) | mantissa;

    // For integer conversion:
    // value = 2^actual_exp * M = significand * 2^(actual_exp - 23)
    // We need floor(value) for truncation toward zero
    //
    // Shift amount to move binary point to position 0:
    // Significand is 23 bits (implicit 1 at bit 22 + 22 mantissa bits)
    // So shift = 23 - actual_exp to get integer part
    int shift = (ND500_FLOAT_MANTISSA_BITS + 1) - actual_exp;

    uint32_t int_value;
    if (shift >= 32) {
        // Value < 1, truncates to 0
        return 0;
    } else if (shift > 0) {
        // Normal case: shift right to get integer part
        int_value = significand >> shift;
    } else if (shift > -32) {
        // Large number: shift left
        int_value = significand << (-shift);
    } else {
        // Overflow - value too large for int32
        int_value = 0x7FFFFFFFu;
    }

    return sign ? -(int32_t)int_value : (int32_t)int_value;
}

/**
 * Convert ND-500 float to IEEE 754 single precision
 *
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^23, M in [0.5, 1.0)
 * IEEE:   value = 2^(exp - 127) * M, where M = 1 + mantissa/2^23, M in [1.0, 2.0)
 *
 * Since M_ieee = 2 * M_nd (both normalized), the exponent adjusts by -1:
 *   ieee_exp = nd_exp - 256 + 127 - 1 = nd_exp - 130
 * And mantissa shifts left (22 bits -> 23 bits):
 *   ieee_mantissa = nd_mantissa << 1
 */
float nd500_float_to_ieee754(uint32_t nd500_bits) {
    // Converged onto the full-range native codec (nd500_native_single_to_double).
    // The narrowing (float) cast preserves the historical narrow-range behaviour for
    // callers that only ever see IEEE-single-range values, while removing the old
    // denormal disagreement with the C# side. See docs/SYNC-FLOAT-NATIVE-REBASE.md.
    return (float)nd500_native_single_to_double(nd500_bits);
}

/**
 * Convert IEEE 754 float to ND-500 single precision
 *
 * IEEE:   value = 2^(exp - 127) * M, where M = 1 + mantissa/2^23, M in [1.0, 2.0)
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^23, M in [0.5, 1.0)
 *
 * Since M_nd = M_ieee / 2, the exponent adjusts by +1:
 *   nd_exp = ieee_exp + 256 - 127 + 1 = ieee_exp + 130
 * And mantissa shifts right (23 bits -> 22 bits):
 *   nd_mantissa = ieee_mantissa >> 1
 */
uint32_t nd500_float_from_ieee754(float ieee_value) {
    // Converged onto the full-range native codec (nd500_native_single_from_double),
    // which applies the same overflow-to-max / underflow-to-zero rules but over the
    // whole native range. See docs/SYNC-FLOAT-NATIVE-REBASE.md.
    return nd500_native_single_from_double((double)ieee_value, NULL, NULL);
}

/**
 * Check if ND-500 float is zero
 */
bool nd500_float_is_zero(uint32_t nd500_bits) {
    return (nd500_bits & ND500_FLOAT_EXPONENT_MASK) == 0;
}

/**
 * Check if ND-500 float is negative
 */
bool nd500_float_is_negative(uint32_t nd500_bits) {
    return (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
}

/**
 * Convert int64 to ND-500 double precision float
 * Based on ND500Double.FromInt64 from C#
 *
 * Per ND-500 Reference Manual 7.2.6:
 * - Format: sign(1) | exponent(9) | mantissa(54)
 * - Value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^55
 * - M range: 0.5 <= M < 1.0 (implicit 0.1 binary prefix)
 * - Since M is in [0.5, 1.0), we need exp - 256 = highestBit + 1
 * - Therefore: exp = highestBit + 256 + 1 = highestBit + 257
 */
uint64_t nd500_double_from_int64(int64_t value) {
    if (value == 0) return 0;

    // Extract sign
    bool sign = (value < 0);
    uint64_t abs_value = (uint64_t)(sign ? -value : value);

    // Find highest set bit position (0-63)
    int highest_bit = 63 - count_leading_zeros_64(abs_value);

    // Calculate exponent (bias 256)
    // For ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^55
    // Since M is in [0.5, 1.0), we need exp - 256 = highestBit + 1
    // Therefore: exp = highestBit + 256 + 1 = highestBit + 257
    int exponent = highest_bit + ND500_DOUBLE_EXPONENT_BIAS + 1;

    // Calculate mantissa: extract bits below the implicit leading 1
    // and shift them to fill the 54-bit mantissa field from the top
    // For value with highest bit at position h, the lower h bits (0 to h-1)
    // need to be placed starting at bit 53 (top of mantissa field)
    // Shift amount: 53 - (highestBit - 1) = 54 - highestBit
    uint64_t mantissa;
    if (highest_bit <= ND500_DOUBLE_MANTISSA_BITS) {
        // Value fits - shift left to align lower bits with top of mantissa
        mantissa = (abs_value << (ND500_DOUBLE_MANTISSA_BITS - highest_bit)) & ND500_DOUBLE_MANTISSA_MASK;
    } else {
        // Value larger than mantissa - shift right and lose precision
        mantissa = (abs_value >> (highest_bit - ND500_DOUBLE_MANTISSA_BITS)) & ND500_DOUBLE_MANTISSA_MASK;
    }

    // Combine sign, exponent, mantissa
    uint64_t result = 0;
    if (sign) result |= ND500_DOUBLE_SIGN_MASK;
    result |= ((uint64_t)exponent << ND500_DOUBLE_EXPONENT_SHIFT);
    result |= mantissa;

    return result;
}

/**
 * Convert ND-500 double precision float to int64 (truncated toward zero)
 *
 * Per ND-500 Reference Manual 7.2.6:
 * - Format: sign(1) | exponent(9) | mantissa(54)
 * - Value = sign * 2^(exponent - 256) * M
 * - M = 0.1mmmmm... (binary) where mantissa bits follow implicit 0.1
 * - M range: 0.5 <= M < 1.0
 * - If exponent = 0, value is exactly zero
 */
int64_t nd500_double_to_int64(uint64_t nd500_bits) {
    // Check for zero (exponent = 0 means exactly zero)
    uint32_t exponent = (uint32_t)((nd500_bits & ND500_DOUBLE_EXPONENT_MASK) >> ND500_DOUBLE_EXPONENT_SHIFT);
    if (exponent == 0) return 0;

    // Extract components
    bool sign = (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
    uint64_t mantissa = nd500_bits & ND500_DOUBLE_MANTISSA_MASK;

    // Calculate actual exponent (e = exponent - 256)
    int actual_exp = (int)exponent - ND500_DOUBLE_EXPONENT_BIAS;

    // Build significand with implicit bit at position 54
    // M = 0.1mmmmm... = (1 << 54 | mantissa) / 2^55
    // The implicit 1 is at bit position 54, mantissa fills bits 53-0
    uint64_t significand = (1ULL << ND500_DOUBLE_MANTISSA_BITS) | mantissa;

    // For integer conversion:
    // value = 2^actual_exp * M = significand * 2^(actual_exp - 55)
    // We need floor(value) for truncation toward zero
    //
    // Shift amount to move binary point to position 0:
    // Significand is 55 bits (implicit 1 at bit 54 + 54 mantissa bits)
    // So shift = 55 - actual_exp to get integer part
    int shift = (ND500_DOUBLE_MANTISSA_BITS + 1) - actual_exp;

    uint64_t int_value;
    if (shift >= 64) {
        // Value < 1, truncates to 0
        return 0;
    } else if (shift > 0) {
        // Normal case: shift right to get integer part
        int_value = significand >> shift;
    } else if (shift > -64) {
        // Large number: shift left
        int_value = significand << (-shift);
    } else {
        // Overflow - value too large for int64
        int_value = 0x7FFFFFFFFFFFFFFFull;
    }

    return sign ? -(int64_t)int_value : (int64_t)int_value;
}

/**
 * Convert ND-500 double to IEEE 754 double precision
 *
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^55, M in [0.5, 1.0)
 * IEEE:   value = 2^(exp - 1023) * M, where M = 1 + mantissa/2^52, M in [1.0, 2.0)
 *
 * Since M_ieee = 2 * M_nd (both normalized), the exponent adjusts by -1:
 *   ieee_exp = nd_exp - 256 + 1023 - 1 = nd_exp + 766
 * And mantissa shifts right (54 bits -> 52 bits):
 *   ieee_mantissa = nd_mantissa >> 2
 */
double nd500_double_to_ieee754(uint64_t nd500_bits) {
    // Converged onto the full-range native codec (nd500_native_double_to_double).
    // See docs/SYNC-FLOAT-NATIVE-REBASE.md.
    return nd500_native_double_to_double(nd500_bits);
}

/**
 * Convert IEEE 754 double to ND-500 double precision
 *
 * IEEE:   value = 2^(exp - 1023) * M, where M = 1 + mantissa/2^52, M in [1.0, 2.0)
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^55, M in [0.5, 1.0)
 *
 * Since M_nd = M_ieee / 2, the exponent adjusts by +1:
 *   nd_exp = ieee_exp - 1023 + 256 + 1 = ieee_exp - 766
 * And mantissa shifts left (52 bits -> 54 bits):
 *   nd_mantissa = ieee_mantissa << 2
 */
uint64_t nd500_double_from_ieee754(double ieee_value) {
    // Converged onto the full-range native codec (nd500_native_double_from_double).
    // See docs/SYNC-FLOAT-NATIVE-REBASE.md.
    return nd500_native_double_from_double(ieee_value, NULL, NULL);
}

/**
 * Check if ND-500 double is zero
 */
bool nd500_double_is_zero(uint64_t nd500_bits) {
    return (nd500_bits & ND500_DOUBLE_EXPONENT_MASK) == 0;
}

/**
 * Check if ND-500 double is negative
 */
bool nd500_double_is_negative(uint64_t nd500_bits) {
    return (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
}

/**
 * Convert ND-500 double to single (precision loss)
 *
 * ND single and double share the same sign position (MSB), 9-bit exponent
 * width and bias (256); only the mantissa width differs (54 vs 22 bits).
 * Conversion therefore keeps sign and exponent and truncates the mantissa
 * (54 -> 22 bits), preserving all fractional precision the F format can hold.
 */
uint32_t nd500_double_to_single(uint64_t nd500_double_bits) {
    // Exponent 0 means exactly zero in both formats
    if ((nd500_double_bits & ND500_DOUBLE_EXPONENT_MASK) == 0) {
        return 0;
    }

    uint32_t sign = (nd500_double_bits & ND500_DOUBLE_SIGN_MASK) ? ND500_FLOAT_SIGN_MASK : 0;
    uint32_t exponent = (uint32_t)((nd500_double_bits & ND500_DOUBLE_EXPONENT_MASK)
                                   >> ND500_DOUBLE_EXPONENT_SHIFT);
    uint64_t mantissa = nd500_double_bits & ND500_DOUBLE_MANTISSA_MASK;

    // Truncate mantissa 54 -> 22 bits (drop the low 32 bits)
    uint32_t f_mantissa = (uint32_t)(mantissa >> (ND500_DOUBLE_MANTISSA_BITS - ND500_FLOAT_MANTISSA_BITS));

    return sign | (exponent << ND500_FLOAT_EXPONENT_SHIFT) | f_mantissa;
}

/**
 * Convert ND-500 single to double (precision extension)
 *
 * Exact: sign and exponent carry over, mantissa is widened 22 -> 54 bits
 * with zero fill.
 */
uint64_t nd500_single_to_double(uint32_t nd500_float_bits) {
    // Exponent 0 means exactly zero in both formats
    if ((nd500_float_bits & ND500_FLOAT_EXPONENT_MASK) == 0) {
        return 0;
    }

    uint64_t sign = (nd500_float_bits & ND500_FLOAT_SIGN_MASK) ? ND500_DOUBLE_SIGN_MASK : 0;
    uint64_t exponent = (nd500_float_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    uint64_t mantissa = nd500_float_bits & ND500_FLOAT_MANTISSA_MASK;

    return sign | (exponent << ND500_DOUBLE_EXPONENT_SHIFT)
                | (mantissa << (ND500_DOUBLE_MANTISSA_BITS - ND500_FLOAT_MANTISSA_BITS));
}

/**
 * Read operand value as IEEE-754 double (works for both float and double types)
 */
double nd500_read_operand_as_ieee_float(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, bool is_double) {
    // Read ND-500 NATIVE float/double bits (bias-256, M in [0.5,1), exp==0 => 0)
    // and CONVERT to an IEEE-754 value for arithmetic. This mirrors the native
    // reference path (Add.c/Sub.c: nd500_read_operand_value + nd500_float_to_ieee754).
    // Reference: ND-500 Reference Manual sections 2.5.1.4 / 2.5.3.6.
    if (is_double) {
        uint64_t bits = nd500_read_operand_doubleword(cpu, operand);
        return nd500_native_double_to_double(bits);
    } else {
        uint32_t bits = (uint32_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_FLOAT);
        return nd500_native_single_to_double(bits);
    }
}

/**
 * Write IEEE-754 double value to operand (converts to float if needed)
 */
void nd500_write_operand_from_ieee_float(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, double value, bool is_double) {
    // CONVERT the IEEE-754 result back to ND-500 NATIVE float/double bits
    // (bias-256, M in [0.5,1)) before storing. Mirrors the native reference path
    // (Add.c/Sub.c: nd500_float_from_ieee754 + nd500_write_*).
    // Reference: ND-500 Reference Manual sections 2.5.1.4 / 2.5.3.6.
    if (is_double) {
        uint64_t bits = nd500_native_double_from_double(value, NULL, NULL);
        nd500_write_operand_value(cpu, operand, bits, ND500_DTYPE_DOUBLEWORD);
    } else {
        uint32_t bits = nd500_native_single_from_double(value, NULL, NULL);
        nd500_write_operand_value(cpu, operand, bits, ND500_DTYPE_FLOAT);
    }
}
