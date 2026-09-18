/*
 * nd500_ndix_boot.c - build an ND-500 machine that can run the NDIX kernel.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See nd500_ndix_boot.h for why this is a library file and not a set of
 * debugger commands. The code below was MOVED, not rewritten: every comment
 * that recorded a measurement, a manual reference or a bug that was paid for
 * once has been carried over verbatim, because those are the expensive part.
 *
 * Sources it came from:
 *   src/debugger/commands.c   cmd_mmusetup, cmd_map_kdata, cmd_ndix_uarea and
 *                             the three static sintran_* helpers
 *   src/frontend/nd500x/nd500x_ndix.c
 *                             place_aout_segments and the body of
 *                             nd500x_ndix_autoboot
 */
#include "nd500_ndix_boot.h"
#include "machine_protos.h"
#include "machine_types.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/nd500_mmu.h"
#include "../cpu/nd500_phys_alloc.h"
#include "../cpu/nd500_xmsg.h"      /* the XMSG server behind the two rings */
#include "../cpu/nd500_settings.h"  /* ND500X_ETH_UPLINK                    */
#include "../ndlib/ndlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

/* ------------------------------------------------------------ logging ----- */

static void default_notice(void* ctx, const char* line) {
    (void)ctx;
    printf("%s\n", line);
}

static Nd500NdixLog s_notice     = default_notice;
static void*        s_notice_ctx = NULL;
static Nd500NdixLog s_verbose    = NULL;   /* discarded unless a caller asks */
static void*        s_verbose_ctx = NULL;

/* NULL restores the DEFAULT, it does not silence the channel. A caller that
 * binds a sink living on its own stack (the debugger commands do) has to hand
 * it back before returning, and "hand it back" must mean "put things as they
 * were" - not "leave the boot log mute for everyone after me". */
void nd500_ndix_set_notice_log(Nd500NdixLog fn, void* ctx) {
    s_notice     = fn ? fn : default_notice;
    s_notice_ctx = fn ? ctx : NULL;
}

void nd500_ndix_set_verbose_log(Nd500NdixLog fn, void* ctx) {
    s_verbose     = fn;      /* the default here IS "discard" */
    s_verbose_ctx = fn ? ctx : NULL;
}

/* One formatted line, no trailing newline - the sink adds whatever it needs.
 * 512 bytes is comfortably above the longest line any of these steps emits. */
static void notice(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    if (!s_notice) return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    s_notice(s_notice_ctx, buf);
}

static void verbose(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    if (!s_verbose) return;          /* the common case: cost is one branch */
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    s_verbose(s_verbose_ctx, buf);
}

/* --------------------------------------------- SINTRAN's share of the job -- */

/* Write a big-endian halfword to an ND-500 virtual address through the DATA
 * MMU, mirroring mon_write_halfword_cb (src/cpu/nd500_indirect.c). Passing
 * is_write=1 makes a cap-0 segment demand-allocate exactly as a kernel data
 * write would, so segment 6 gets backed and the bytes are visible to BOTH the
 * kernel and the MON handlers (which translate through the same MMU). */
static void sintran_write_halfword(Nd500Cpu* cpu, uint32_t vaddr, uint16_t val) {
    uint32_t phys = vaddr;
    if (cpu->machine && cpu->machine->mmu_enabled)
        phys = nd500_mmu_translate(cpu, vaddr, 1, 0);  /* is_write, data */
    nd500_bus_write8(cpu->machine, phys,     (uint8_t)(val >> 8));  /* BE hi byte */
    nd500_bus_write8(cpu->machine, phys + 1, (uint8_t)val);         /* BE lo byte */
}

/* SINTRAN shared-memory init: Xmsg ring-buffer descriptors (segment 6).
 *
 * On real hardware SINTRAN (the ND-100 side) sets up the ND-100<->ND-500
 * shared segment before the NDIX kernel runs. The kernel's R_init()
 * (if/xg.c:399) REQUIRES the two ring-buffer headers to be pre-initialized and
 * panics ("Xmsg command/response buffer not initialized") otherwise:
 *
 *   xmsg_cmd_buf  @ 0x30000000 : p=0, k=0, mp=NXMSGCMD  (102)
 *   xmsg_resp_buf @ 0x30000800 : p=0, k=0, mp=NXMSGRESP (113)
 *
 * p (offset 0) and k (offset 2) are already 0 because segment 6 is
 * demand-allocated zeroed, so only the mp field (offset 4, a big-endian short)
 * needs writing. Addresses AND values were verified by disassembling _R_init at
 * 0x3EF42: "h comp2 $0x30000004,#102" and "h comp2 $0x30000804,#113". Note the
 * response struct is UNPADDED on the ND-500 compiler, so
 * NXMSGRESP=(0x800-6)/sizeof(xmsg_resp=18)=113 (NOT 102 - the command struct is
 * 20 bytes -> 102). These match the RetroCore NDSharedMemory reference
 * (XMSG_CMD_BUFFER=0x30000000, XMSG_RESP_BUFFER=0x30000800). */
/* uplink_loop - hand every transmitted frame straight back as a received one.
 * The `ctx` is the CPU, because the receive side has to reach guest memory and
 * raise an interrupt. */
static void ndix_uplink_loop(void* ctx, const uint8_t* frame, uint32_t len) {
    nd500_xmsg_frame_in((Nd500Cpu*)ctx, frame, len);
}

static void sintran_init_xmsg_ringbuffers(Nd500Cpu* cpu) {
    sintran_write_halfword(cpu, 0x30000004u, 102);  /* xmsg_cmd_buf.mp  = NXMSGCMD  */
    sintran_write_halfword(cpu, 0x30000804u, 113);  /* xmsg_resp_buf.mp = NXMSGRESP */
    /* The rings are empty again, so the server behind them must forget the port
     * numbers it handed out on any previous boot in this process. */
    nd500_xmsg_reset();

    /* Where et0's frames go. Nothing by default - they are logged under
     * ND500X_FEDBG and dropped, which is all phases 1-4 needed.
     *
     * "loop" echoes every transmitted frame straight back in. That sounds like
     * a toy and is not: it exercises the ENTIRE receive path - the queue, the
     * ac_head envelope, the parked-receive completion and the interrupt that
     * has to be raised outside a DCTL_KICK - with no host, no privileges and
     * nothing to configure. Measured: after one `ifconfig et0 ... up`,
     *
     *     Name  Mtu   Network     Address      Ipkts Ierrs Opkts Oerrs
     *     et0   1498  223.255.25  223.255.254. 1     0     1     0
     *
     * That Ipkts is if_ipackets++ in etrint() (if_et.c:675), which is only
     * reached once the datagram has been decoded.
     *
     * The frame itself goes no further, and that is CORRECT rather than a
     * limitation: in_arpinput() drops an ARP whose sender hardware address is
     * our own before it looks at anything else - "it's from me, ignore it"
     * (netinet/if_ether.c:286). A loopback cannot produce a conversation; it
     * proves the road. */
    {
        const char* up = nd500_settings()->eth_uplink;
        if (up && strcmp(up, "loop") == 0) {
            nd500_xmsg_set_uplink(ndix_uplink_loop, cpu);
            fprintf(stderr, "[XMSG] uplink: loopback (ND500X_ETH_UPLINK=loop)\n");
        }
    }
}

/* Write a big-endian 32-bit word to an ND-500 virtual address through the DATA
 * MMU (same demand-alloc path as sintran_write_halfword). */
static void sintran_write_word(Nd500Cpu* cpu, uint32_t vaddr, uint32_t val) {
    sintran_write_halfword(cpu, vaddr,     (uint16_t)(val >> 16));
    sintran_write_halfword(cpu, vaddr + 2, (uint16_t)val);
}

/* SINTRAN shared-memory init: the IPL (Interrupt Priority Level) record,
 * struct ipl_rec, at the fixed shared-segment address _iplrec = 0x30001000
 * (locore.c:116). Layout (icb.h:31, offsets in bytes):
 *   ip_next   @0 (long)  : outstanding-interrupt descriptor, ND-100 word addr;
 *                          -1 (0xFFFFFFFF) means "none pending"
 *   ip_current@4 (short) : current IPL
 *   ip_mask   @6 (short) : IPL mask
 *   ip_lock   @8 (short) : spinlock byte
 *
 * On real hardware SINTRAN owns this record and queues interrupt descriptors
 * into ip_next; when idle it holds -1. The NDIX kernel never initializes it
 * (machdep.c:834 only does `iplp = &iplrec`); _splx and _intvec (locore.c:1413,
 * 791) only READ ip_next, short-circuiting on -1. Segment 6 is demand-allocated
 * ZEROED, so ip_next=0, which _splx treats as a real descriptor pointer:
 *   r3 = ip_next<<1 - shseg + sharebase = 0 - 0x30000800 + 0x30000000 = -0x800
 *   deref [r3+4] = 0xFFFFF804  -> PS_AZI page fault (verified: exact fault addr).
 * ip_current/ip_mask/ip_lock are correctly 0 from the demand-zero, so only
 * ip_next needs the -1 sentinel. (shseg = htob(sharedseg)+NBPG = 0x30000800.) */
static void sintran_init_iplrec(Nd500Cpu* cpu) {
    sintran_write_word(cpu, 0x30001000u, 0xFFFFFFFFu);  /* iplrec.ip_next = -1 (none) */
}

/* Map domain-0 logical segment `seg` to a contiguous physical region
 * [phys_base, phys_base + npages*2048) via a PS_ASI page table at pt_phys, using
 * PST index `psn`, as a writable data segment. Used to make _Pst/_pcbtab reach
 * the physical PST/DIT the emulator MMU reads. Page-table entries and the PST
 * entry are written in the hardware pte.h format (pg_pfnum@[29:0]). */
static void sintran_map_segment_to_phys(Nd500Machine* m, int seg, uint32_t phys_base,
                                        uint32_t npages, uint32_t psn, uint32_t pt_phys) {
    for (uint32_t i = 0; i < npages; i++) {
        uint32_t pfn = (phys_base >> PGSHIFT) + i;      /* prot 0 = read/write */
        uint32_t pte = pfn & 0x3FFFFFFFu;
        uint32_t a = pt_phys + i * 4u;
        nd500_bus_write8(m, a,   (uint8_t)(pte >> 24));
        nd500_bus_write8(m, a+1, (uint8_t)(pte >> 16));
        nd500_bus_write8(m, a+2, (uint8_t)(pte >> 8));
        nd500_bus_write8(m, a+3, (uint8_t)pte);
    }
    nd500_mmu_set_pst_entry(m->cpu, (int)psn, PS_ASI, pt_phys >> PGSHIFT);
    nd500_mmu_set_data_capability(m->cpu, 0, seg, (uint16_t)(psn | DC_WRP));
}

/* ------------------------------------------------------------ mmu setup --- */

int nd500_ndix_mmu_setup(Nd500Machine* m) {
    if (!m || !m->cpu) {
        notice("mmusetup: no cpu linked");
        return -1;
    }

    verbose("Setting up MMU with 3 domains: Kernel (0) + User1 (1) + User2 (2)");
    verbose("Each domain gets 256KB code + 256KB data (128 pages each)");
    verbose("");
    verbose("Physical Memory Layout:");
    verbose("  0x00000000-0x0003FFFF: Domain 0 (Kernel) Code (256KB = 128 pages)");
    verbose("  0x00040000-0x0007FFFF: Domain 0 (Kernel) Data (256KB = 128 pages)");
    verbose("  0x00080000-0x000BFFFF: Domain 1 (User1) Code (256KB = 128 pages)");
    verbose("  0x000C0000-0x000FFFFF: Domain 1 (User1) Data (256KB = 128 pages)");
    verbose("  0x00100000-0x0013FFFF: Domain 2 (User2) Code (256KB = 128 pages)");
    verbose("  0x00140000-0x0017FFFF: Domain 2 (User2) Data (256KB = 128 pages)");
    verbose("  0x00180000-0x00FFFFFF: Available (~14.5 MB)");
    verbose("");

    /* Set the guest MMU-table base registers BEFORE any capability/PST setup so
     * the set_* mirroring lands in the right physical tables. The tables live in
     * the free gap between the kernel image and kernel free memory (firstaddr,
     * phys 0x100000): PST at 0x84000, DIT at 0x90000 (64KB), seg 27/28 page
     * tables at 0xA0000. translate() reads these.
     *
     * PSTP MUST sit ABOVE the loaded DSEG. The kernel image is loaded flat as one
     * contiguous block: PSEG at raw 0 (size 0x41a94) + DSEG at raw 0x41a94 (size
     * 0x3E800), so kernel data actually extends to raw 0x8028C - PAST the old
     * 0x80000 PST base. load-dseg (run AFTER mmusetup) therefore zeroed the low
     * ~163 PST entries, including Pst[FIRST_PHYS_SEG=13]. The kernel reads
     * first_phys_seg = Pst[13].ps_pfnum (machdep.c:181); with it clobbered to 0,
     * every derived index collapsed (physindex 19->6, pstindex 17->4, ...) and
     * the kernel then dereferenced Pst[pstindex].ps_pfnum==0 as Physbase+0 (the
     * "inaccessible" physical page 0), page-faulting at PC 0x3621C. Placing PSTP
     * at 0x84000 (above 0x8028C) keeps the synthetic identity PST entries intact,
     * so first_phys_seg=13 and the SINTRAN-provided entries (dataindex, pstindex,
     * psindex) survive with their valid identity values. Verified via ND500X_PTWDBG. */
    m->cpu->PSTP    = ND500_NDIX_PSTP;
    m->cpu->DITBASE = ND500_NDIX_DITBASE;
    /* NOTE: boot-time live CAD = 1 (the /etc/init domain) is established from
     * vmunix.init via `set CAD 1`, NOT here: the load-pseg/load-dseg steps run
     * after mmusetup and would wipe a value set at this point. See the comment
     * in kernel/MASTER/GENERIC/vmunix.init for the full rationale (it is what
     * lets the /etc/init launch RET at PC=0x29 switch domains, manual 4.2.5.2). */
    /* Tell the physical page allocator that this whole region is spoken for.
     * The layout above is hand-built at fixed addresses and never passes through
     * the allocator, so without this a DOM loaded afterwards would be handed the
     * kernel's own pages. Reserved pages are permanent: no arena pop frees them. */
    if (nd500_phys_reserve(m, 0x00000000, 0x00180000) != 0) {
        notice("warning: could not reserve 0x000000-0x17FFFF - a domain may already hold part of it");
    }

    /* Zero the PST (32KB) and DIT (64KB) so unset segments read capability 0
     * (=> demand-map / identity fallback) instead of stale RAM garbage. */
    for (uint32_t a = 0x00080000; a < 0x000A0000; a++)
        nd500_bus_write8(m, a, 0);

    /* =======================================================
     * PST CONFIGURATION - Create 128 contiguous pages per region
     * Each domain needs 256 PST entries (128 for code + 128 for data)
     * Total: 768 PST entries
     * ======================================================= */
    verbose("=== PST Configuration ===");
    verbose("Creating 768 PST entries (256 per domain)...");

    /* Domain 0 (Kernel) Code: PSN 0-127 -> Physical 0x00000000-0x0003FFFF */
    for (uint32_t i = 0; i < 128; i++) {
        nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);  /* PFN = PSN for direct mapping */
    }
    verbose("PST[0-127]     = Domain 0 kernel code (phys 0x00000000-0x0003FFFF)");

    /* Domain 0 (Kernel) Data: PSN 128-255 -> Physical 0x00040000-0x0007FFFF */
    for (uint32_t i = 128; i < 256; i++) {
        nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
    }
    verbose("PST[128-255]   = Domain 0 kernel data (phys 0x00040000-0x0007FFFF)");

    /* Domain 1 (User1) Code: PSN 256-383 -> Physical 0x00080000-0x000BFFFF */
    for (uint32_t i = 256; i < 384; i++) {
        nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
    }
    verbose("PST[256-383]   = Domain 1 user1 code (phys 0x00080000-0x000BFFFF)");

    /* Domain 1 (User1) Data: PSN 384-511 -> Physical 0x000C0000-0x000FFFFF */
    for (uint32_t i = 384; i < 512; i++) {
        nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
    }
    verbose("PST[384-511]   = Domain 1 user1 data (phys 0x000C0000-0x000FFFFF)");

    /* Domain 2 (User2) Code: PSN 512-639 -> Physical 0x00100000-0x0013FFFF */
    for (uint32_t i = 512; i < 640; i++) {
        nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
    }
    verbose("PST[512-639]   = Domain 2 user2 code (phys 0x00100000-0x0013FFFF)");

    /* Domain 2 (User2) Data: PSN 640-767 -> Physical 0x00140000-0x0017FFFF */
    for (uint32_t i = 640; i < 768; i++) {
        nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
    }
    verbose("PST[640-767]   = Domain 2 user2 data (phys 0x00140000-0x0017FFFF)");

    verbose("");
    verbose("=== PCB Configuration ===");
    verbose("Mapping 128 consecutive segment entries per domain...");
    verbose("");

    /* Domain 0 (Kernel): Virtual segment 0 onwards */
    verbose("Domain 0 (Kernel):");
    /* Code segments 0-127: Each segment i maps to PSN i (phys 0x00000000+) */
    for (uint32_t seg = 0; seg < 128; seg++) {
        /* PC_DIR is 0x0000 - direct is absence of PC_IND flag, not a flag to set */
        nd500_mmu_set_program_capability(m->cpu, 0, seg, seg);
    }
    verbose("  Prog segments [0-127]   -> PSN [0-127]   (virtual 0x00000000-0x3F800000)");

    /* Data segments 0-127: Each segment i maps to PSN 128+i (phys 0x00040000+).
     * Kernel data is READ/WRITE, so grant DC_WRP - without it every kernel data
     * store (including the stack-frame [B+8] SP write the NDIX kernel does in
     * INIT/ENTS) hits "WRITE DENIED! missing DC_WRP flag".
     *
     * Two segments must NOT be pre-mapped with the demo's single 2KB PS_AZI page,
     * because their real extents exceed 2KB (an access past offset 0x7FF faults
     * with "PS_AZI page fault L2!=0"):
     *   - segment 0  = the flat-loaded kernel image (text+data+const, virtual
     *     0x0..< physRAM). Leaving its data capability 0 lets the identity
     *     fallback back it (virtual == physical, writes allowed) - exactly what
     *     the PROGRAM side already does (program cap 0 -> identity), so kernel
     *     globals/consts above 2KB (e.g. vaddr 0x00022924) resolve.
     *   - segment 29 = the u-area / kernel stack (virtual 0xE8000000, 8KB, beyond
     *     physRAM). Leaving its capability 0 lets the segment-demand allocator
     *     back it as a writable, paged PS_ADI segment big enough for the stack. */
    for (uint32_t seg = 0; seg < 128; seg++) {
        /* Leave two segments capability 0 so the correct fallback backs them
         * instead of the demo's too-small 2KB PS_AZI page:
         *   seg 0  = flat-loaded kernel image (text+data+const, virtual 0x0 ..
         *            < physRAM) -> identity fallback (virtual == physical, writes
         *            allowed) - exactly what the PROGRAM side already does, so
         *            kernel globals/consts above 2KB (e.g. vaddr 0x00022924) resolve.
         *   seg 29 = the u-area / kernel stack (virtual 0xE8000000, 8KB, beyond
         *            physRAM) -> segment-demand allocator backs it writable + paged
         *            (PS_ADI) big enough for the whole stack. Fixes the reported
         *            [B+8] SP write dropping and the RET PREVB=0 stack underflow.
         * The OTHER data segments keep the demo mapping: they carry loaded DSEG
         * data the kernel reads early, so we must NOT replace them with zeroed
         * demand pages - just make them writable (DC_WRP). */
        if (seg == 0 || (seg >= 1 && seg <= 30)) {
            continue;  /* seg 0 = identity image; 1..30 = runtime kernel tables demand-backed PS_ADI
                        * (the demo 2KB PS_AZI page is too small for the kernel's real segments). */
        }
        nd500_mmu_set_data_capability(m->cpu, 0, seg, (128 + seg) | DC_WRP);
    }
    verbose("  Data segments   -> RW (seg 0 = identity image, seg 29 = demand-backed u-area)");

    /* Special: Segment 31 for Domain 0 = ND-100 Other Machine (INDIRECT + OMC) */
    /* Bit 15 = 1 (INDIRECT), Bit 14 = 1 (OMC), Domain=0, Segment=1 */
    nd500_mmu_set_program_capability(m->cpu, 0, 31, PC_IND | PC_OMC | (0 << 5) | 1);
    verbose("  Prog segment 31         -> INDIRECT OMC Domain=0 Seg=1 (ND-100)");

    verbose("");
    verbose("Domain 1 (User1):");
    /* Code segments 0-127: Each segment i maps to PSN 256+i (phys 0x00080000+) */
    for (uint32_t seg = 0; seg < 128; seg++) {
        /* PC_DIR is 0x0000 - direct is absence of PC_IND flag */
        nd500_mmu_set_program_capability(m->cpu, 1, seg, (256 + seg));
    }
    verbose("  Prog segments [0-127]   -> PSN [256-383] (virtual 0x00000000-0x3F800000)");

    /* Data segments 0-127: Each segment i maps to PSN 384+i (phys 0x000C0000+) */
    for (uint32_t seg = 0; seg < 128; seg++) {
        nd500_mmu_set_data_capability(m->cpu, 1, seg, (384 + seg) | DC_PAC);
    }
    verbose("  Data segments [0-127]   -> PSN [384-511] (virtual 0x00000000-0x3F800000)");

    /* Special: Segment 31 for Domain 1 = Link to Kernel (INDIRECT, no OMC) */
    /* Bit 15 = 1 (INDIRECT), Bit 14 = 0 (no OMC), Domain=0, Segment=1 */
    nd500_mmu_set_program_capability(m->cpu, 1, 31, PC_IND | (0 << 5) | 1);
    verbose("  Prog segment 31         -> INDIRECT Domain=0 Seg=1 (-> Kernel)");

    verbose("");
    verbose("Domain 2 (User2):");
    /* Code segments 0-127: Each segment i maps to PSN 512+i (phys 0x00100000+) */
    for (uint32_t seg = 0; seg < 128; seg++) {
        /* PC_DIR is 0x0000 - direct is absence of PC_IND flag */
        nd500_mmu_set_program_capability(m->cpu, 2, seg, (512 + seg));
    }
    verbose("  Prog segments [0-127]   -> PSN [512-639] (virtual 0x00100000-0x0013FFFF)");

    /* Data segments 0-127: Each segment i maps to PSN 640+i (phys 0x00140000+) */
    for (uint32_t seg = 0; seg < 128; seg++) {
        nd500_mmu_set_data_capability(m->cpu, 2, seg, (640 + seg) | DC_PAC);
    }
    verbose("  Data segments [0-127]   -> PSN [640-767] (virtual 0x00140000-0x0017FFFF)");

    /* Special: Segment 31 for Domain 2 = Link to Kernel (INDIRECT, no OMC) */
    /* Bit 15 = 1 (INDIRECT), Bit 14 = 0 (no OMC), Domain=0, Segment=1 */
    nd500_mmu_set_program_capability(m->cpu, 2, 31, PC_IND | (0 << 5) | 1);
    verbose("  Prog segment 31         -> INDIRECT Domain=0 Seg=1 (-> Kernel)");

    /* Map the kernel's own table-access segments so _Pst (seg 27, 0xd8000000)
     * reaches physical PSTP and _pcbtab (seg 28, 0xe0000000) reaches physical
     * DITBASE. Without this the kernel's writes to kern_dcap/Pst (kpcbinit,
     * __resume, newproc) land in demand pages the MMU never reads. PS_ASI page
     * tables at 0xA0000 / 0xA1000; high PSNs to avoid the demo's 0-767. */
    sintran_map_segment_to_phys(m, 27, m->cpu->PSTP,    16, 800, 0x000A0000); /* _Pst */
    sintran_map_segment_to_phys(m, 28, m->cpu->DITBASE, 32, 801, 0x000A1000); /* _pcbtab */
    verbose("Mapped seg 27 -> PSTP (0x%X), seg 28 -> DITBASE (0x%X)",
            m->cpu->PSTP, m->cpu->DITBASE);

    verbose("");
    verbose("=== MMU Registers ===");
    m->cpu->CAD = 0;
    m->cpu->CED = 0;
    m->cpu->PS = 0;
    verbose("PSTP    = 0x%08X", m->cpu->PSTP);
    verbose("DITBASE = 0x%08X", m->cpu->DITBASE);
    verbose("CAD     = 0 (Alternative Domain - kernel)");
    verbose("CED     = 0 (Executing Domain - kernel)");
    verbose("PS      = 0 (Process Segment)");

    /* Enable MMU */
    verbose("");
    nd500_machine_enable_mmu(m);
    verbose("MMU ENABLED - Virtual memory now active!");

    /* SINTRAN's job on context load: initialize the Xmsg ring-buffer
     * descriptors in the shared segment so the NDIX kernel's R_init() does
     * not panic. Done after MMU enable so the write translates through the
     * data MMU and demand-backs segment 6. See helper above for the verified
     * addresses/values (_R_init disasm at 0x3EF42). */
    sintran_init_xmsg_ringbuffers(m->cpu);
    verbose("Xmsg ring buffers initialized (cmd.mp=102, resp.mp=113 @ seg 6)");

    /* Also SINTRAN's job: seed the IPL record's ip_next to -1 ("no interrupt
     * pending"). Without it _splx derefs a zeroed ip_next as a descriptor
     * pointer and page-faults at 0xFFFFF804. See helper above. */
    sintran_init_iplrec(m->cpu);
    verbose("IPL record initialized (iplrec.ip_next = -1 @ 0x30001000)");

    verbose("");
    verbose("Virtual Memory Layout (each domain has 256KB code + 256KB data):");
    verbose("  Domain 0 (Kernel): Virtual 0x00000000-0x3F800000 -> Phys 0x00000000-0x0007FFFF");
    verbose("  Domain 1 (User1):  Virtual 0x00000000-0x3F800000 -> Phys 0x00080000-0x000FFFFF");
    verbose("  Domain 2 (User2):  Virtual 0x00000000-0x3F800000 -> Phys 0x00100000-0x0017FFFF");
    verbose("");
    verbose("Configuration complete! You can now:");
    verbose("  - Load PSEG/DSEG files to any virtual address 0x00000000-0x3F800000");
    verbose("  - Switch domains using 'set CAD <domain>' or 'set CED <domain>'");
    verbose("  - Use 'showmmu' to view MMU status");
    verbose("  - Use 'phyladr <vaddr>' to test address translation");
    verbose("  - Use 'listpst' to see all 768 configured PST entries");
    verbose("  - Use 'listpcb' to see domain configurations");

    return 0;
}

/* ----------------------------------------------------------- map-kdata ---- */

/*
 * Describe the kernel's OWN data segment (domain 0, segment 0) in the GUEST
 * page tables, so the kernel's software checks can see it.
 *
 * Why this is needed: nothing in NDIX ever writes pcbtab[KDOM].pcb_dc[DC_KDATA].
 * On real hardware the ND-100/SINTRAN context load installs the kernel domain's
 * own segment capabilities before the ND-500 kernel starts - the same class of
 * gap as THA/CTE1/CTE2, which vmunix.init already hand-installs. Without it
 * kernacc() (machdep.c) reads capability 0 for segment 0 and refuses every
 * access, so io/mem.c mmrw() minor 1 returns EFAULT and /dev/kmem is unusable:
 * "ps" dies with "error reading nswap from /dev/kmem", and w/vmstat/pstat/
 * netstat fail the same way.
 *
 * This deliberately writes ONLY the guest tables (PST at PSTP and the DIT at
 * DITBASE). It does NOT touch the emulator's shadow capability table, because
 * translate() reads the shadow - not the DIT - for domain 0 segment 0 (that
 * segment is not in the use_guest set). Address translation therefore keeps
 * using the proven segment-0 data fallback (virtual + data_base) and is
 * completely unaffected; only kernacc()/getpte() start seeing the truth.
 *
 * phys_base MUST be page aligned, because a page table can only express a
 * page-granular mapping. vmunix.init loads the DSEG at 0x00042000 for exactly
 * this reason (a_text 0x41a94 is not a multiple of NBPG).
 */
int nd500_ndix_map_kdata(Nd500Machine* m, uint32_t phys_base, uint32_t size,
                         uint32_t psn, uint32_t pt_phys) {
    if (!m || !m->cpu) {
        notice("map-kdata: no cpu linked");
        return -1;
    }
    if (!m->cpu->PSTP || !m->cpu->DITBASE) {
        notice("map-kdata: run mmusetup first (PSTP/DITBASE unset)");
        return -1;
    }

    /* 0 means "the default" for both, which is how the debugger command's
     * optional arguments behaved. */
    if (psn == 0)     psn = ND500_NDIX_KDATA_PSN;
    if (pt_phys == 0) pt_phys = ND500_NDIX_KDATA_PT_PHYS;

    if (phys_base & PGOFSET) {
        notice("map-kdata: phys_base 0x%08X is not page aligned (NBPG=%d)",
               phys_base, 1 << PGSHIFT);
        return -1;
    }
    uint32_t npages = (size + (uint32_t)PGOFSET) >> PGSHIFT;
    if (npages == 0) {
        notice("map-kdata: size must be non-zero");
        return -1;
    }
    /* PS_ASI is a single index level: at most NPTEPG entries (one page of PTEs). */
    if (npages > 512u) {
        notice("map-kdata: %u pages exceeds the PS_ASI limit of 512", npages);
        return -1;
    }

    /* Page table: entry k maps segment page k to phys_base + k*NBPG.
     * struct pte is pg_prot@31, pg_xx@30, pg_pfnum@[29:0]; prot 0 = read/write. */
    for (uint32_t i = 0; i < npages; i++) {
        uint32_t pte = ((phys_base >> PGSHIFT) + i) & 0x3FFFFFFFu;
        uint32_t a = pt_phys + i * 4u;
        nd500_bus_write8(m, a,   (uint8_t)(pte >> 24));
        nd500_bus_write8(m, a+1, (uint8_t)(pte >> 16));
        nd500_bus_write8(m, a+2, (uint8_t)(pte >> 8));
        nd500_bus_write8(m, a+3, (uint8_t)pte);
    }

    nd500_mmu_set_pst_entry(m->cpu, (int)psn, PS_ASI, pt_phys >> PGSHIFT);

    /* Guest DIT ONLY: domain 0, data table (+64), segment 0 (+0), 16-bit BE.
     * Deliberately not nd500_mmu_set_data_capability() - see the note above. */
    uint16_t cap = (uint16_t)((psn & DC_PSN) | DC_WRP);
    uint32_t cap_addr = m->cpu->DITBASE + 0u * 256u + 64u + 0u * 2u;
    nd500_bus_write8(m, cap_addr,     (uint8_t)(cap >> 8));
    nd500_bus_write8(m, cap_addr + 1, (uint8_t)cap);

    verbose("kernel data seg 0: phys 0x%08X..0x%08X (%u pages) -> PSN %u, "
            "PS_ASI page table at 0x%08X, DIT cap 0x%04X",
            phys_base, phys_base + (npages << PGSHIFT) - 1, npages, psn, pt_phys, cap);
    return 0;
}

/* ------------------------------------------------------------- u-area ----- */

/*
 * Build proc0's kernel-stack segment - the u-area at _u = 0xE8000000, which is
 * segment 29 (DC_KSTACK, machine/pcb.h:195) - the way an ND-100 bootstrap would
 * have left it. Same class of gap as THA/CTE1/CTE2 and map-kdata.
 *
 * Why nd500x has to do this at all: machdep.c:181-193 only DERIVES the twelve
 * well-known kernel segment indices from Pst[FIRST_PHYS_SEG].ps_pfnum and never
 * assigns Pst[stackindex]; its comment says outright that "the 100 has loaded
 * the kernel into the start of the 5000 memory". init_main.c:68-74 then READS
 * that slot: p_p0br = ptob(Pst[iseg].ps_pfnum) + Physbase, p_addr = iseg. There
 * is no ND-100 here, so the slot has to be built or nothing valid is in it.
 *
 * The SHAPE is not a guess. NDIX builds the equivalent slot itself for every
 * other process in vm_pt.c:85-89 - slot +0 PS_ASI over Usrptmap[a], slots +1..+3
 * PS_ADI - and that was confirmed by reading the live PST out of physical memory
 * at PSTP during a boot: PST[46]=0x40000910, PST[51]=0x40000901, PST[56], [61],
 * [66] all PS_ASI, each followed by three PS_ADI entries. So slot +0 is a
 * ONE-LEVEL segment whose index page doubles as the process page table, and its
 * first UPAGES entries are the u-area's own pages. That is exactly what
 * baseline/bin/ps.c:1305-1350 reads back: a page of struct pte from p_p0br,
 * expecting arguutl[0..UPAGES-1].pg_pfnum to be non-zero.
 *
 * Only ONE slot is written. Pst[16..25] are the twelve well-known KERNEL
 * segments (pcb.h:171-183: STACKINDEX 3 -> 16, PSTINDEX 4 -> 17, SYSINDEX 5 ->
 * 18, PHYSINDEX 6 -> 19, PSINDEX 7 -> 20). proc0's p_addr names a single one of
 * them, not a five-slot process group - writing a group here would overwrite
 * sysindex and physindex, which machdep.c:236 and :371 own. p_szpt = 4 at
 * init_main.c:73 is nominal; the swapper never has user text, data or stack.
 *
 * Default physical placement sits below sfree (0x00100000, nd500_fecall.c:266),
 * so these pages are outside the pool FE_INIT tells NDIX it owns, and above the
 * seg 27/28 page tables (0xA0000/0xA1000) and map-kdata's page table (0xA2000).
 */

/* Constants the u-area step needs, each one a real NDIX source fact. */
#define NDIX_UPAGES          4u            /* machine/param.h:31, = 8 KB       */
#define NDIX_FIRST_PHYS_SEG  13u           /* machine/pcb.h:164                */
#define NDIX_STACKINDEX      3u            /* machine/pcb.h:154                */
#define NDIX_KSTACK_SEG      29            /* machine/pcb.h:195, _u >> SGSHIFT */

int nd500_ndix_uarea(Nd500Machine* m, uint32_t pt_phys, uint32_t uarea_phys) {
    if (!m || !m->cpu) {
        notice("ndix-uarea: no cpu linked");
        return -1;
    }
    if (!m->cpu->PSTP || !m->cpu->DITBASE) {
        notice("ndix-uarea: run mmusetup first (PSTP/DITBASE unset)");
        return -1;
    }

    if (pt_phys == 0)    pt_phys = ND500_NDIX_UAREA_PT_PHYS;
    if (uarea_phys == 0) uarea_phys = ND500_NDIX_UAREA_PG_PHYS;

    if ((pt_phys & PGOFSET) || (uarea_phys & PGOFSET)) {
        notice("ndix-uarea: addresses must be page aligned (NBPG=%d)", 1 << PGSHIFT);
        return -1;
    }

    /* first_phys_seg is read from the PST exactly as machdep.c:181 reads it,
     * rather than assumed to be 13, so a different PST layout still lands in
     * the slot the kernel will actually look at. */
    uint32_t fps = nd500_bus_read32(m, m->cpu->PSTP + NDIX_FIRST_PHYS_SEG * 4u)
                 & 0x3FFFFFFFu;
    uint32_t psn = fps + NDIX_STACKINDEX;
    if (psn == 0 || psn >= MAX_PST) {
        notice("ndix-uarea: derived PSN %u out of range (Pst[%u].ps_pfnum = %u)",
               psn, NDIX_FIRST_PHYS_SEG, fps);
        return -1;
    }

    /* Zero the whole index page first: entries past UPAGES must read 0 so the
     * MMU treats them as absent (nd500_mmu_read_pte: valid = pfnum != 0) and so
     * ps sees a clean end to the table rather than stale RAM. */
    for (uint32_t i = 0; i < (1u << PGSHIFT); i++)
        nd500_bus_write8(m, pt_phys + i, 0);

    /* Entry k maps u-area page k. struct pte is pg_prot@31, pg_xx@30,
     * pg_pfnum@[29:0] (machine/pte.h:27-31); prot 0 = read/write, which the
     * kernel stack needs. */
    for (uint32_t i = 0; i < NDIX_UPAGES; i++) {
        uint32_t pte = ((uarea_phys >> PGSHIFT) + i) & 0x3FFFFFFFu;
        uint32_t a = pt_phys + i * 4u;
        nd500_bus_write8(m, a,     (uint8_t)(pte >> 24));
        nd500_bus_write8(m, a + 1, (uint8_t)(pte >> 16));
        nd500_bus_write8(m, a + 2, (uint8_t)(pte >> 8));
        nd500_bus_write8(m, a + 3, (uint8_t)pte);
    }

    /* Zero the u-area pages themselves. locore.c:200 does the same thing on the
     * way in ("a virgin u area"), but it only covers _u.._Kstack; this makes the
     * whole 8 KB deterministic rather than whatever the allocator left. */
    for (uint32_t i = 0; i < NDIX_UPAGES << PGSHIFT; i++)
        nd500_bus_write8(m, uarea_phys + i, 0);

    nd500_mmu_set_pst_entry(m->cpu, (int)psn, PS_ASI, pt_phys >> PGSHIFT);

    /* Segment 29 is in the guest-table set (nd500_mmu.c use_guest), so the
     * capability must reach the DIT - set_data_capability writes the shadow and
     * mirrors it there. __resume (locore.c:924-932) overwrites this on every
     * context switch with p_addr|DC_WRP; the point of setting it here is that
     * the FIRST touch of 0xE8000000, long before any __resume, finds a real
     * mapping instead of falling into the demand allocator. */
    uint16_t cap = (uint16_t)((psn & DC_PSN) | DC_WRP);
    nd500_mmu_set_data_capability(m->cpu, 0, NDIX_KSTACK_SEG, cap);

    /* The NOTICE channel, not the verbose one: this line was a bare printf in
     * the debugger command precisely so it would appear on the --ndix boot
     * path, where the command context swallows output(). It replaces the
     * "kernel u-area published to PST[16]" line the demand-map fallback used to
     * print, and matches those ND-500: lines. */
    notice("ND-500: proc0 kernel stack (seg %d) -> PST[%u] PS_ASI, page table "
           "0x%08X, %u u-area pages at 0x%08X..0x%08X, DIT cap 0x%04X",
           NDIX_KSTACK_SEG, psn, pt_phys, NDIX_UPAGES, uarea_phys,
           uarea_phys + (NDIX_UPAGES << PGSHIFT) - 1, cap);
    return 0;
}

/* ------------------------------------------------------- segment placing -- */

/* Read the three sizes out of an a.out header. Big-endian, per pcc-nd500
 * src/include/nd500/a.out.h: a_magic@0, a_text@4, a_data@8, a_bss@12. */
static int read_aout_sizes(const char* path, uint32_t* a_text, uint32_t* a_data,
                           uint32_t* a_bss) {
    unsigned char hdr[32];
    FILE* f = fopen(path, "rb");
    if (!f || fread(hdr, 1, sizeof hdr, f) != sizeof hdr) {
        if (f) fclose(f);
        return -1;
    }
    fclose(f);
    *a_text = ((uint32_t)hdr[4]  << 24) | ((uint32_t)hdr[5]  << 16)
            | ((uint32_t)hdr[6]  << 8)  |  (uint32_t)hdr[7];
    *a_data = ((uint32_t)hdr[8]  << 24) | ((uint32_t)hdr[9]  << 16)
            | ((uint32_t)hdr[10] << 8)  |  (uint32_t)hdr[11];
    *a_bss  = ((uint32_t)hdr[12] << 24) | ((uint32_t)hdr[13] << 16)
            | ((uint32_t)hdr[14] << 8)  |  (uint32_t)hdr[15];
    return 0;
}

/* Mark a hand-loaded image's pages as permanently in use. The file's size is
 * the extent actually written; a failure here only means some page is already
 * held by a live domain, which is worth saying out loud rather than ignoring. */
static void reserve_loaded_image(Nd500Machine* m, uint32_t base_addr,
                                 const char* filepath, const char* what) {
    FILE* f = fopen(filepath, "rb");
    if (!f) return;
    long sz = -1;
    if (fseek(f, 0, SEEK_END) == 0) sz = ftell(f);
    fclose(f);
    if (sz <= 0) return;

    if (nd500_phys_reserve(m, base_addr, (uint32_t)sz) != 0) {
        notice("warning: %s at 0x%08X overlaps memory a loaded domain owns",
               what, base_addr);
    }
}

/* Put the kernel a.out into physical memory in the layout the PSEG/DSEG files
 * would have produced: text at 0, data at <dseg_load>, bss zeroed after it.
 *
 * The debugger's own `load` is NOT enough here. It places data immediately
 * after text at a_text (0x41A8C for the shipped kernel), while load-dseg places
 * it at the next 2 KB page (0x42000) - and 0x42000 is the address map-kdata is
 * given and the address the kernel's own data references were linked against.
 * The two differ by 1396 bytes, so relying on `load` alone would shift every
 * kernel datum. `load` is still issued before this, for the entry PC and the
 * symbols; this then overwrites what it put down with the right placement. */
static int place_aout_segments(Nd500Machine* m, const char* path,
                               unsigned long dseg_load,
                               unsigned long pseg_size, unsigned long dseg_size) {
    unsigned char hdr[32];
    uint32_t a_text, a_data, a_bss;
    unsigned char* buf;
    FILE* f;
    unsigned long i;

    f = fopen(path, "rb");
    if (!f || fread(hdr, 1, sizeof hdr, f) != sizeof hdr) {
        if (f) fclose(f);
        notice("error: cannot read %s", path);
        return -1;
    }
    a_text = ((uint32_t)hdr[4]  << 24) | ((uint32_t)hdr[5]  << 16)
           | ((uint32_t)hdr[6]  << 8)  |  (uint32_t)hdr[7];
    a_data = ((uint32_t)hdr[8]  << 24) | ((uint32_t)hdr[9]  << 16)
           | ((uint32_t)hdr[10] << 8)  |  (uint32_t)hdr[11];
    a_bss  = ((uint32_t)hdr[12] << 24) | ((uint32_t)hdr[13] << 16)
           | ((uint32_t)hdr[14] << 8)  |  (uint32_t)hdr[15];
    (void)a_bss;

    /* IMAGIC/OMAGIC keep text immediately after the 32-byte header
     * (pcc-nd500 src/include/nd500/a.out.h, N_TXTOFF). */
    buf = (unsigned char*)malloc((size_t)a_text + a_data);
    if (!buf) { fclose(f); notice("error: out of memory reading %s", path); return -1; }
    if (fread(buf, 1, (size_t)a_text + a_data, f) != (size_t)a_text + a_data) {
        fclose(f); free(buf);
        notice("error: %s is shorter than its header claims", path);
        return -1;
    }
    fclose(f);

    /* Check against the PADDED size, since that is what actually gets written. */
    if (dseg_load + dseg_size > m->memory_size) {
        free(buf);
        notice("error: kernel needs 0x%lX bytes, machine has 0x%X",
               dseg_load + dseg_size, m->memory_size);
        return -1;
    }

    for (i = 0; i < a_text; i++)
        nd500_bus_write8(m, (uint32_t)i, buf[i]);
    /* splitseg pads .pseg with ZEROS from a_text up to the 2 KB page boundary,
     * and the load-pseg path therefore writes those zeros too. We must match it,
     * because the debugger's `load` ran a moment ago and placed the DATA image at
     * a_text (0x41A8C) instead of at the next page (0x42000) - see the comment
     * above. That leaves 1396 bytes of stray data bytes sitting in the text tail
     * which the .pseg path has as zeros.
     *
     * Not cosmetic: those bytes were being read as if they were text, and the
     * boot died dereferencing 0x2025640A - the ASCII of the format string " %d\n"
     * - before reaching login. Zeroing the tail is what makes the from-image boot
     * behave identically to the .pseg/.dseg boot. */
    for (i = a_text; i < pseg_size; i++)
        nd500_bus_write8(m, (uint32_t)i, 0);
    for (i = 0; i < a_data; i++)
        nd500_bus_write8(m, (uint32_t)(dseg_load + i), buf[a_text + i]);
    /* bss must be zero: guest RAM is only cleared at power-on, and the kernel
     * assumes a zeroed bss the way every C program does. The loop runs to
     * dseg_size, not a_data+a_bss, for the same reason as the text tail above:
     * .dseg is padded to a page and map-kdata is handed the padded size. */
    for (i = a_data; i < dseg_size; i++)
        nd500_bus_write8(m, (uint32_t)(dseg_load + i), 0);

    free(buf);

    /* THE separate-I&D data base. This is what load-dseg does after writing the
     * file (commands.c, "Separate I&D de-aliasing for the PSEG/DSEG load path")
     * and it is NOT optional: a.out magic 0411 means the kernel's data lives in
     * D-space at [0, a_data+a_bss), so a segment-0 data read of virtual V must
     * resolve to data_base + V. The debugger's `load`, issued just before us,
     * sets this base to a_text (0x41A8C) because that is where IT put the data.
     * We move the data to the next page (0x42000), so the base has to move with
     * it - otherwise every kernel data read is 1396 bytes low.
     *
     * Measured before this call existed: at _feinit+0x08 the kernel executed
     * `w1 := $114728` and got 0x2025640A (the ASCII of " %d\n" at 0x5DAB4)
     * instead of 0x0003DF00 at 0x5E028 - exactly 1396 bytes adrift - and the
     * boot then died on a page fault at PC=0x3613E. The bytes in memory were
     * correct all along; only this base was wrong. */
    ndlib_aout_set_data_base((uint32_t)dseg_load);

    /* Claim both extents, exactly as the load-pseg / load-dseg commands do via
     * reserve_loaded_image() (src/debugger/commands.c). Without this the page
     * allocator does not know the kernel image is there and can hand the same
     * frames to a domain demand-mapped later. This was the ONLY thing the
     * load-pseg/load-dseg path did that placing the a.out by hand did not, and
     * leaving it out is what made the boot-from-image path die where the
     * .pseg/.dseg path booted. */
    if (nd500_phys_reserve(m, 0, (uint32_t)pseg_size) != 0)
        notice("warning: kernel text at 0x0 overlaps memory a loaded domain owns");
    if (nd500_phys_reserve(m, (uint32_t)dseg_load, (uint32_t)dseg_size) != 0)
        notice("warning: kernel data at 0x%08lX overlaps memory a loaded domain owns",
               dseg_load);

    notice("[ndix] placed a.out: text 0x0..0x%X (zero-padded to 0x%08lX), "
           "data 0x%08lX..0x%08lX, bss+pad zeroed to 0x%08lX",
           a_text, pseg_size, dseg_load, dseg_load + a_data,
           dseg_load + dseg_size);
    return 0;
}

/* --------------------------------------------------------------- boot ----- */

/* Boot the kernel.
 *
 * Everything the old vmunix.init hand-wrote is derived from the files here, so a
 * rebuilt kernel cannot silently desync:
 *   - the .pseg / .dseg paths come from the kernel path
 *   - the .dseg load address is the .pseg size rounded to a 2 KB page
 *   - map-kdata gets that same address and the real .dseg size
 * (verified against the shipped kernel: pseg 0x42000, dseg 0x3E800, matching the
 * 0x42000 / 0x3E800 constants the init file carried by hand).
 *
 * THA/CTE1/CTE2/CAD stand in for what SINTRAN's context load would have set. */
int nd500_ndix_boot(Nd500Machine* m, const Nd500NdixBoot* cfg) {
    unsigned long pseg_size, dseg_load, dseg_size;
    int have_seg_files;
    struct stat sp, sd;
    uint32_t entry = 0, pc = 0;

    if (!m || !m->cpu) {
        notice("ndix-boot: no cpu linked");
        return -1;
    }
    if (!cfg || !cfg->kernel_path || !cfg->kernel_path[0]) {
        notice("ndix-boot: no kernel path given");
        return -1;
    }

    have_seg_files = (cfg->pseg_path && cfg->dseg_path &&
                      stat(cfg->pseg_path, &sp) == 0 &&
                      stat(cfg->dseg_path, &sd) == 0);

    if (have_seg_files) {
        pseg_size = (unsigned long)sp.st_size;
        dseg_size = (unsigned long)sd.st_size;
    } else {
        /* No .pseg/.dseg beside the kernel - derive both from the a.out itself.
         * splitseg produces nothing the header does not already say: pseg is
         * a_text rounded up to a 2 KB page, dseg is a_data + a_bss rounded the
         * same way. Checked against the shipped kernel, whose header reads
         * a_text=0x41A8C a_data=0x1CB20 a_bss=0x21540: that gives 270336 and
         * 256000, byte-for-byte the sizes of vmunix.pseg and vmunix.dseg.
         *
         * This is what lets the kernel come out of the disk image, where only
         * the a.out exists and there are no segment files to sit beside it. */
        uint32_t a_text, a_data, a_bss;
        if (read_aout_sizes(cfg->kernel_path, &a_text, &a_data, &a_bss) != 0) {
            notice("error: cannot read the a.out header of %s", cfg->kernel_path);
            return -1;
        }
        if (a_text == 0) {
            notice("error: %s has no text segment - not an NDIX kernel",
                   cfg->kernel_path);
            return -1;
        }
        pseg_size = ((unsigned long)a_text + 0x7FFUL) & ~0x7FFUL;
        dseg_size = (((unsigned long)a_data + a_bss) + 0x7FFUL) & ~0x7FFUL;
        notice("[ndix] segments derived from the a.out: text=%u data=%u bss=%u",
               a_text, a_data, a_bss);
    }

    /* .dseg follows .pseg, page-aligned (NBPG = 2048). */
    dseg_load = (pseg_size + 0x7FFUL) & ~0x7FFUL;

    notice("[ndix] auto-boot: pseg=%lu dseg=%lu -> dseg@0x%08lX kdata 0x%08lX+0x%lX",
           pseg_size, dseg_size, dseg_load, dseg_load, dseg_size);

    /* The a.out load first: it reads the file and SETS THE ENTRY PC from the
     * header, and it is what puts the kernel's symbols where
     * ndlib_symbols_lookup() can find them - which the THA derivation below
     * depends on. Without this the machine starts at PC=0 and immediately stops
     * on an invalid instruction.
     *
     * This used to be the debugger's `load` command. The command does exactly
     * this plus auto-sourcing <name>.init, and this path exists precisely
     * BECAUSE there is no .init - so nothing is lost by calling the library
     * function the command itself calls. */
    if (ndlib_load_aout_with_debug(m, cfg->kernel_path, 1, &entry, &pc) != 0) {
        notice("error: failed to load kernel %s", cfg->kernel_path);
        return -1;
    }
    notice("[ndix] loaded %s (entry 0x%08X, PC 0x%08X)", cfg->kernel_path, entry, pc);

    /* Naming each step in the boot log is not decoration - when a boot dies
     * the last line printed is how you know which step it died in. These used
     * to appear because the frontend echoed every command string it sent
     * ("[ndix] mmusetup"); now the step says its own name. */
    notice("[ndix] mmusetup");
    if (nd500_ndix_mmu_setup(m) != 0) return -1;

    if (have_seg_files) {
        /* Pre-split segment files: load them exactly as load-pseg/load-dseg do,
         * including the reservation and the separate-I&D data base that the
         * dseg command sets. */
        if (nd500_load_pseg_file(m, cfg->pseg_path, 0x00000000) != 0) {
            notice("error: failed to load PSEG %s", cfg->pseg_path);
            return -1;
        }
        reserve_loaded_image(m, 0x00000000, cfg->pseg_path, "PSEG");
        notice("[ndix] loaded PSEG %s at 0x00000000", cfg->pseg_path);

        if (nd500_load_dseg_file(m, cfg->dseg_path, (uint32_t)dseg_load) != 0) {
            notice("error: failed to load DSEG %s", cfg->dseg_path);
            return -1;
        }
        /* Separate I&D de-aliasing - see the long note in place_aout_segments;
         * cmd_load_dseg does the same thing right after writing the file. */
        ndlib_aout_set_data_base((uint32_t)dseg_load);
        reserve_loaded_image(m, (uint32_t)dseg_load, cfg->dseg_path, "DSEG");
        notice("[ndix] loaded DSEG %s at 0x%08lX", cfg->dseg_path, dseg_load);
    } else if (place_aout_segments(m, cfg->kernel_path, dseg_load,
                                   pseg_size, dseg_size) != 0) {
        return -1;
    }

    notice("[ndix] map-kdata 0x%08lX 0x%08lX", dseg_load, dseg_size);
    if (nd500_ndix_map_kdata(m, (uint32_t)dseg_load, (uint32_t)dseg_size, 0, 0) != 0)
        return -1;

    /* THA = _u + U_CXB0, NOT _u + _Ktrap.
     *
     * The runtime trap vector is set LIVE by the kernel's __resume
     * (machine/locore.c:964-966): tha := _u + U_CXB0 + traplev*496, and
     * U_CXB0 = 1852 = 0x73C (machine/locore.h:66). At traplev 0 that is
     * 0xE800073C. The static _Ktrap = _u+0x736 (locore.c:183), which
     * kpcbinit stores in pcb_tha, is 6 bytes LOWER and is not 4-byte aligned;
     * cpu.c:942-949 already documents that it yields a misaligned vector.
     *
     * Measured: with 0xE8000736 a trap 38 reads its slot at 0xE80007CE, so the
     * big-endian word straddles THA[36]=0x00000365 and THA[37]=0x00000373 and
     * returns 0x03650000 - a garbage handler address. With 0xE800073C the same
     * slot is 0xE80007D4 and holds the real page-fault handler 0x00000381.
     *
     * This bootstrap value only matters for a trap raised in domain 0 before
     * the first __resume/domain switch, since a switch reloads THA. A healthy
     * boot never traps that early, which is why the wrong value went unnoticed;
     * a memory-starved boot does, and died here.
     *
     * _u is looked up in the kernel's own symbol table rather than assumed, so a
     * kernel that moves its u-area still gets a correct vector. The load above
     * has already read the symbols. U_CXB0 stays a literal because it is a
     * compile-time struct offset (machine/locore.h:66), not a linker symbol -
     * there is nothing in the a.out to read it from. If the lookup fails we fall
     * back to the measured value rather than booting with THA unset. */
    {
        uint32_t u_addr = 0;
        unsigned long tha = 0xE800073CUL;
        if (ndlib_symbols_lookup("_u", &u_addr, NULL) == 0 && u_addr != 0) {
            tha = (unsigned long)u_addr + 0x73CUL;   /* U_CXB0 = 1852 */
            if (tha != 0xE800073CUL)
                notice("[ndix] THA derived from _u=0x%08X -> 0x%08lX", u_addr, tha);
        } else {
            notice("[ndix] warning: symbol _u not found, using THA 0x%08lX", tha);
        }
        notice("[ndix] set THA 0x%08lX", tha);
        nd500_dbg_reg_set_by_name(m->cpu, "THA", (uint32_t)tha);
    }
    /* nd500_dbg_reg_set_by_name(), not a field assignment: it is the SAME
     * function the debugger's `set` command calls, so these four writes cannot
     * drift from what typing them by hand does. Today it is a plain write
     * through an offset table (debug_api.c:493); if it ever grows a side
     * effect, this path gets it too. */
    notice("[ndix] set CTE1 0xF413D800");
    nd500_dbg_reg_set_by_name(m->cpu, "CTE1", 0xF413D800u);
    notice("[ndix] set CTE2 0x0000005F");
    nd500_dbg_reg_set_by_name(m->cpu, "CTE2", 0x0000005Fu);
    notice("[ndix] set CAD 1");
    nd500_dbg_reg_set_by_name(m->cpu, "CAD", 1u);

    if (cfg->with_uarea && nd500_ndix_uarea(m, 0, 0) != 0)
        return -1;

    return 0;
}
