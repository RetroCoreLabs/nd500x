/* ═══════════════════════════════════════════════════════════════════════════
 * nd500_fecall.c - NDIX ND-100 front-end call (fecall) = MON 600 (octal, 0x180)
 *
 * The NDIX kernel does ALL device I/O (disk, console, clock, init) by calling
 * the ND-100 front-end processor via `fecall()` = `callg $0xf8000180`. On real
 * hardware the ND-100 monitor services these. Here the emulator services them,
 * backing the disk with a host image file (rootfs.img).
 *
 * Interface fully documented in docs/NDIX_FECALL_MON600_SPEC.md. Summary:
 *   callg $0xf8000180,$4, device, request, rpk, cpk
 *     device  = gen<<16 | subdev   (gen: DISK=1)
 *     request = qualifier<<16 | req (req: FE_INIT=1 .. FE_ERRM=0xe)
 *     rpk/cpk = response/command packet addresses
 *   Packet addresses arrive as ND-100 WORD addresses = (nd500_phys_byte+private)/2
 *   EXCEPT FE_INIT (_feinit_fecall skips the conversion) where they are LOGICAL
 *   kernel addresses (private is not known until FE_INIT returns it).
 *   Invert: nd500_phys_byte = word*2 - private.  private cancels in the DMA math.
 *
 * Sync calls (FE_INIT, FE_IDEV, FE_OPEN, FE_CLOS, FE_RCON, FE_WCON): fill the
 * response packet and return. Async calls (FE_READ, FE_WRIT, FE_DCTL): the
 * kernel blocks in biowait() until a completion INTERRUPT drives diintr()->
 * iodone()->B_DONE. (Interrupt delivery: see nd500_fecall_deliver_interrupt.)
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "cpu_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include "nd500_fecall.h"
#include "nd500_tape.h"
#include "../machine/machine_protos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Once-latched env flag: getenv() on the CPU run path races readline's
 * setenv (environ realloc) on the main thread -> SIGSEGV. Latch once. */
static int nd_env_flag(const char* name, int* latch) {
    if (*latch < 0) { const char* e = getenv(name); *latch = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return *latch;
}
static int g_envf_gatedbg = -1;
static int g_envf_inodedbg = -1;


/* ---- FE request codes (machine/if.h) ---- */
#define FE_INIT 0x1
#define FE_IDEV 0x2
#define FE_OPEN 0x3
#define FE_CLOS 0x4
#define FE_READ 0x5
#define FE_RCON 0x6
#define FE_WRIT 0x7
#define FE_WCON 0x8
#define FE_DCTL 0x9
#define FE_EXIT 0xb
#define FE_ERRM 0xe

/* ---- generic device numbers (machine/if.h) ---- */
#define GEN_DISK   0x1
#define GEN_XMSG   0x7
#define GEN_SIINTR 0x8
#define GEN_TAPE   0x2

/* Debug-logging predicate, defined further down; forward-declared so the
 * order of the handlers below does not depend on where it happens to sit. */
static int fedbg(void);

/* ---- tape (generic 2) ----------------------------------------------------
 * io/mt.c drives this. The interface is RECORD-structured, not block-
 * structured: FE_READ says "give me up to maxbytes, tell me how long this
 * record actually was", and FE_DCTL does space-record / space-file / rewind.
 * A flat image cannot express variable-length records or filemarks, so the
 * backing store is a SIMH .tap:
 *
 *   record : <4-byte LE length N> <N bytes, padded to even> <4-byte LE N>
 *   N == 0          tape mark (filemark)
 *   N == 0xFFFFFFFF end of medium
 *   N == 0xFFFFFFFE erase gap - skipped
 *   bit 31 set      error record; the low 24 bits are still the length
 *
 * The trailing length is what makes backspacing possible: BSR reads the
 * 4 bytes before the current position to learn how far to step back.
 *
 * Read-only, per the tracked scope of the task. FE_WRIT and DCTL_WEOF report
 * an error rather than silently pretending to have written - a tape driver
 * that thinks it wrote a filemark and did not would corrupt whatever the
 * guest builds on top of it.
 * ------------------------------------------------------------------------- */
#define TAPE_MARK      0x00000000u
#define TAPE_EOM       0xFFFFFFFFu
#define TAPE_ERASE_GAP 0xFFFFFFFEu

/* DCTL operations (machine/if.h:324-332) */
#define DCTL_FSF      1   /* forward space file    */
#define DCTL_BSF      2   /* backward space file   */
#define DCTL_FSR      3   /* forward space record  */
#define DCTL_BSR      4   /* backward space record */
#define DCTL_REW      5   /* rewind                */
#define DCTL_REW_UNL  6   /* rewind + unload       */
#define DCTL_STAT     7   /* status                */
#define DCTL_WEOF     8   /* write end-of-file     */

/* ---- init_rpk field byte offsets (machine/if.h, byte-packed) ---- */
#define IR_COMPLETION 0
#define IR_HOWTO      2
#define IR_ROOTDEV    6
#define IR_CONDEV     10
#define IR_SPST       12
#define IR_SCONT      16
#define IR_SNDIX      20
#define IR_STEXT      24
#define IR_SDATA      28
#define IR_SSTACK     32
#define IR_SFREE      36
#define IR_SBUFFER    40
#define IR_SPHYS      44
#define IR_PRIVATE    48
#define IR_CPUTYPE    52
#define IR_S3VERS     54
#define IR_SHAREDSEG  56
#define IR_CONTIGNO   60
#define IR_PAGENO     62
#define IR_BOOTED     64

/* ---- read_cpk_disk / read_rpk_xxxx offsets ---- */
#define RD_CPK_NBYTES   0
#define RD_CPK_PHYSADDR 4
#define RD_CPK_DEVADDR  8
#define RD_RPK_COMPLETION 0
#define RD_RPK_STATUS     2
#define RD_RPK_NBYTES     4

/* ---- open_rpk_disk offsets ---- */
#define OP_RPK_COMPLETION 0
#define OP_RPK_DEVSIZ     2
#define OP_RPK_FRMSIZ     6
#define OP_RPK_SECSIZ     10

/* ---- idev_rpk_xxxx offsets ---- */
#define ID_RPK_COMPLETION 0
#define ID_RPK_SUBDEVC    2

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration returned by FE_INIT and reused by later calls.
 *
 * private: the ND-500<->ND-100 address offset. Must be NONZERO (the kernel's
 * dton() returns 0 when private==0, which would zero all DMA addresses). Its
 * value cancels in the packet/DMA round-trip (kernel adds it, we subtract it),
 * so any nonzero value works as long as we invert with the SAME value we report.
 * ═══════════════════════════════════════════════════════════════════════════ */
#define FE_PRIVATE   0x00002000u   /* nonzero ND-100<->ND-500 offset (bytes) */
#define FE_ROOTDEV   0            /* di0a: makedev(di_major=0, minor 0) */
#define FE_CONDEV    0            /* console minor device */
#define FE_SECSIZE   512u         /* disk sector size (bytes) */

/* No built-in disk path. A root-anchored default belongs to whoever ran the
 * build, not to the repository - the image location comes from --ndix (which
 * exports ND500X_DISK) or from ND500X_DISK directly. */

static FILE*    g_disk = NULL;
static long     g_disk_size = 0;
static int      g_disk_rw = 0;   /* 1 = COW session open r+b, honor FE_WRIT */
static uint32_t g_ssize = FE_SECSIZE;   /* sector size, confirmed by FE_OPEN */

/* ---- tape state (SIMH .tap, read-only) ----
 * The record/framing layer lives in nd500_tape.c so it can be unit-tested on
 * its own. That matters here: the guest has no mt binary and no /dev/mt nodes,
 * so none of this can be reached through a boot yet, and an untested layer
 * that has never run is one nobody should trust. */
static Nd500Tape g_tape;
static int       g_tape_init = 0;

/* Attach the image named by ND500X_TAPE, once. No built-in default: a
 * root-anchored path belongs to whoever ran the build, not to the repo. */
static Nd500Tape* fe_tape_open(void) {
    if (g_tape_init) return g_tape.fp ? &g_tape : NULL;
    g_tape_init = 1;
    const char* p = getenv("ND500X_TAPE");
    if (!p || !p[0]) return NULL;
    if (nd500_tape_attach(&g_tape, p) != 0) {
        fprintf(stderr, "[FECALL] tape: cannot open %s\n", p);
        return NULL;
    }
    fprintf(stderr, "[FECALL] tape image: %s (%ld bytes)\n", p, g_tape.size);
    return &g_tape;
}

typedef struct {
    Nd500Cpu* cpu;
    int       logical;   /* 1: base is a logical kernel addr; 0: base is a physical byte addr */
    uint32_t  base;
} Pkt;

static Pkt pkt_logical(Nd500Cpu* cpu, uint32_t logical_addr) {
    Pkt p; p.cpu = cpu; p.logical = 1; p.base = logical_addr; return p;
}
/* ND-100 word address -> physical byte base */
static Pkt pkt_word(Nd500Cpu* cpu, uint32_t nd100_word) {
    Pkt p; p.cpu = cpu; p.logical = 0; p.base = nd100_word * 2u - FE_PRIVATE; return p;
}

static void pkt_wr16(Pkt* p, uint32_t off, uint16_t v) {
    if (p->logical) nd500_write_memory_16(p->cpu, p->base + off, v);
    else            nd500_bus_write16(p->cpu->machine, p->base + off, v);
}
static void pkt_wr32(Pkt* p, uint32_t off, uint32_t v) {
    if (p->logical) nd500_write_memory_32(p->cpu, p->base + off, v);
    else            nd500_bus_write32(p->cpu->machine, p->base + off, v);
}
static uint16_t pkt_rd16(Pkt* p, uint32_t off) {
    return p->logical ? nd500_read_memory_16(p->cpu, p->base + off)
                      : nd500_bus_read16(p->cpu->machine, p->base + off);
}
static uint32_t pkt_rd32(Pkt* p, uint32_t off) {
    return p->logical ? nd500_read_memory_32(p->cpu, p->base + off)
                      : nd500_bus_read32(p->cpu->machine, p->base + off);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_INIT - system initialization. Return the boot parameter block SINTRAN's
 * ND-100 monitor would have supplied. Memory boundaries are ND-100 WORD
 * addresses = (physical_byte + private)/2; the kernel doubles them back to
 * bytes (htob) and, for firstaddr, subtracts private (so private cancels).
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_init(Nd500Cpu* cpu, Pkt* rpk) {
    uint32_t memtop = cpu->machine ? cpu->machine->memory_size : 0x1000000u;

    /* ND500X_MEMTOP caps the top of memory REPORTED to NDIX, in bytes (a plain
     * or 0x-prefixed number). Real emulator RAM is untouched - the kernel
     * simply believes it has less, which is what puts freemem under lotsfree
     * and starts the pageout daemon (sys/vm_sched.c:416,
     * lotsfree = LOOPPAGES / LOTSFREEFRACT). That is the only way to exercise
     * the swap path, and therefore RPGU/RWIP, on a machine with 16 MB of RAM
     * and a kernel that fits in 1 MB.
     *
     * Ignored unless it leaves at least one page above sfree, so a mistyped
     * value cannot produce a kernel with zero page frames. */
    {
        static long capped = -2;   /* -2 = not yet read, -1 = unset/invalid */
        if (capped == -2) {
            const char* e = getenv("ND500X_MEMTOP");
            capped = -1;
            if (e && e[0]) {
                char* end = NULL;
                long v = strtol(e, &end, 0);
                if (end && *end == '\0' && v > 0) capped = v;
            }
        }
        if (capped > 0 && (uint32_t)capped < memtop &&
            (uint32_t)capped > 0x00100000u + 2048u) {
            fprintf(stderr, "[FECALL] FE_INIT: reporting memtop 0x%lX instead of 0x%X"
                            " (ND500X_MEMTOP)\n", capped, memtop);
            memtop = (uint32_t)capped;
        }
    }

    /* Physical layout (bytes): kernel image + emulator PST(0x84000)/DIT(0x90000)
     * live below sfree; free RAM from 1 MB to the top of memory. */
    uint32_t scont_phys = 0x00000000u;   /* first physical addr NDIX uses */
    uint32_t sfree_phys = 0x00100000u;   /* start of free RAM (past image+tables) */

    /* ND500X_SFREE overrides where NDIX's free pool starts, in bytes.
     *
     * The default 0x00100000 is KNOWN to overlap the emulator's own
     * allocations and is left unchanged only because moving it changes the
     * guest's view of memory. mmusetup reserves just 0x0-0x180000
     * (src/debugger/commands.c:3066), so demand segments land above that -
     * measured at 0x00180000 (seg 6), 0x001A1000 (seg 29), 0x001C2000 (seg 8),
     * with an ND500X_PHYSDBG high-water of 0x001E3000 that does not vary with
     * guest memory size. All of that sits inside the pool NDIX is told it owns,
     * nothing informs NDIX, and under memory pressure NDIX reuses and zeroes
     * those pages - destroying the segment-8 page table and faulting the
     * cxbtab (see the ROOT CAUSE notes on this in git log).
     *
     * Setting ND500X_SFREE=0x280000 puts the pool above the high-water with
     * headroom. Kept as an override rather than a new default so the shipped
     * behaviour is unchanged until the tradeoff (half a megabyte of guest
     * memory) is chosen deliberately. */
    {
        static long sfree_override = -2;
        if (sfree_override == -2) {
            const char* e = getenv("ND500X_SFREE");
            sfree_override = -1;
            if (e && e[0]) {
                char* end = NULL;
                long v = strtol(e, &end, 0);
                if (end && *end == '\0' && v > 0) sfree_override = v;
            }
        }
        if (sfree_override > 0 && (uint32_t)sfree_override < memtop) {
            fprintf(stderr, "[FECALL] FE_INIT: sfree 0x%lX instead of 0x%X"
                            " (ND500X_SFREE)\n", sfree_override, sfree_phys);
            sfree_phys = (uint32_t)sfree_override;
        }
    }

    uint32_t sphys_phys = memtop;        /* top of physical memory */
    uint32_t stext_phys = 0x00000000u;
    uint32_t sdata_phys = 0x00041a94u;   /* = a_text (data base) */
    uint32_t sstack_phys = sfree_phys;
    uint32_t spst_phys  = 0x00084000u;   /* emulator PSTP */

    /* word = (physical + private)/2 */
#define W(x) (((x) + FE_PRIVATE) / 2u)
    pkt_wr16(rpk, IR_COMPLETION, 0);           /* success */
    pkt_wr32(rpk, IR_HOWTO,      0);
    pkt_wr32(rpk, IR_ROOTDEV,    FE_ROOTDEV);
    pkt_wr16(rpk, IR_CONDEV,     FE_CONDEV);
    pkt_wr32(rpk, IR_SPST,       W(spst_phys));
    pkt_wr32(rpk, IR_SCONT,      W(scont_phys));
    pkt_wr32(rpk, IR_SNDIX,      W(stext_phys));
    pkt_wr32(rpk, IR_STEXT,      W(stext_phys));
    pkt_wr32(rpk, IR_SDATA,      W(sdata_phys));
    pkt_wr32(rpk, IR_SSTACK,     W(sstack_phys));
    pkt_wr32(rpk, IR_SFREE,      W(sfree_phys));
    pkt_wr32(rpk, IR_SBUFFER,    W(sfree_phys));
    pkt_wr32(rpk, IR_SPHYS,      W(sphys_phys));
    pkt_wr32(rpk, IR_PRIVATE,    FE_PRIVATE);  /* RAW bytes, not a word address */
    pkt_wr16(rpk, IR_CPUTYPE,    1);
    pkt_wr16(rpk, IR_S3VERS,     1);
    pkt_wr32(rpk, IR_SHAREDSEG,  W(sphys_phys));
    pkt_wr16(rpk, IR_CONTIGNO,   0);
    pkt_wr16(rpk, IR_PAGENO,     0);
    /* booted[256]: "vmunix" */
    { const char* b = "vmunix"; uint32_t i;
      for (i = 0; b[i]; i++) {
          if (rpk->logical) nd500_write_memory_8(cpu, rpk->base + IR_BOOTED + i, (uint8_t)b[i]);
          else              nd500_bus_write8(cpu->machine, rpk->base + IR_BOOTED + i, (uint8_t)b[i]);
      }
    }
#undef W
    if (fedbg())
        fprintf(stderr, "[FECALL] FE_INIT: rootdev=%u private=0x%X sfree_phys=0x%X sphys=0x%X\n",
                FE_ROOTDEV, FE_PRIVATE, sfree_phys, sphys_phys);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_IDEV - initialise a generic device. Return one sub-device.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_idev(Nd500Cpu* cpu, uint32_t gen, Pkt* cpk, Pkt* rpk) {
    /* Record this device's interrupt priority. machine/if.h: every _idev_cpk
     * variant starts with "short ipl", and each driver fills it in before the
     * call - if/si.c:39 does `Idev_cpk(*si_pkt).ipl = IPL_SI` (3). Completion
     * interrupts for the device must then be delivered at THAT level, not at the
     * disk level everything used to get. Range-checked because a driver that
     * leaves the field uninitialised would otherwise index cxbtab[] with
     * garbage; 0 keeps the caller's default. */
    if (gen < (sizeof cpu->fe_dev_ipl / sizeof cpu->fe_dev_ipl[0])) {
        uint16_t ipl = pkt_rd16(cpk, 0);
        cpu->fe_dev_ipl[gen] = (ipl >= 1 && ipl <= 15) ? (uint8_t)ipl : 0;
        if (fedbg())
            fprintf(stderr, "[FECALL] FE_IDEV gen=%u ipl=%u%s\n", gen, ipl,
                    (ipl >= 1 && ipl <= 15) ? "" : " (out of range - using default)");
    }

    /* EXPERIMENT (ND500X_NOXMSG): fail the XMSG (gen 7) device init so xgattach
     * bails early (xg.c:137) and never issues the DCTL_WAIT / sleep(&xwbuf) that
     * currently deadlocks the boot - to see whether the cross-message subsystem
     * is required to reach mountfs / the root disk read. */
    static int noxmsg = -1;
    if (noxmsg < 0) { const char* e = getenv("ND500X_NOXMSG"); noxmsg = (e && e[0] && e[0] != '0') ? 1 : 0; }
    if (noxmsg && gen == GEN_XMSG) {
        pkt_wr16(rpk, ID_RPK_COMPLETION, 1);   /* error -> xgattach returns early */
        if (fedbg()) fprintf(stderr, "[FECALL] FE_IDEV gen=%u -> FAIL (NOXMSG)\n", gen);
        return;
    }
    /* Tape: only present when an image is attached (ND500X_TAPE). With no
     * image, answer SUCCESS with zero sub-devices - that is how the kernel
     * spells "the drive is not there", and it is silent:
     *
     *     io/mt.c:107   if (fep->subdevc = Idev_rpk(*mt_pkt).subdevc) {
     *                           fep->alive++; fep->state = GD_RUN; ...
     *
     * subdevc == 0 skips the whole block, so nothing is marked alive and no
     * mt sub-device is ever built. Reporting a nonzero COMPLETION instead sent
     * mtattach down io/mt.c:103 nderror(), which printed
     *     mtattach(): bad completion code 01 from feidev()
     * on every single boot of a machine that simply has no tape drive. The
     * earlier reasoning here - that success would make mtattach create /dev/mt*
     * that fail on access - was wrong: with subdevc 0 it creates nothing. */
    pkt_wr16(rpk, ID_RPK_COMPLETION, 0);
    if (gen == GEN_TAPE && !fe_tape_open()) {
        pkt_wr16(rpk, ID_RPK_SUBDEVC, 0);
        if (fedbg())
            fprintf(stderr, "[FECALL] FE_IDEV gen=2 (tape) -> subdevc=0 (no ND500X_TAPE image)\n");
        return;
    }

    /* Terminals answer with a DIFFERENT packet shape. machine/if.h:
     *     struct _idev_rpk_xxxx { short completion; short subdevc; }
     *     struct _idev_rpk_term { short completion; long locdevm; short remdevc; }
     * so for a terminal generic there is no subdevc field at all - offset 2 is
     * the TOP HALF of locdevm. Writing the generic subdevc=1 there set
     * locdevm = 0x00010000.
     *
     * io/mx.c miattach() walks i = 0 .. devtab.subdevc (MAXTTY = 256, fixed in
     * GENERIC/ioconf.c) and admits line i when
     *     i == 0, or (i <= MAXLOCALTTY && (locdevm & (1 << (31 - (i-1)/4))))
     * - four minors per mask bit, counting down from bit 31. Bit 16 therefore
     * enabled minors 61-64 and nothing else, which is why /dev/console worked
     * while init reported "/dev/tty01: No such device or address".
     *
     * Enable the first 16 local lines (bits 31-28) so tty01..tty16 exist.
     * remdevc stays 0: remote lines (minor 129+) would need the ND-100 side. */
    if (gen == 3 /* TERM_IN */ || gen == 4 /* TERM_OUT */) {
        pkt_wr32(rpk, 2, 0xF0000000u);      /* locdevm: minors 1-16 */
        pkt_wr16(rpk, 6, 0);                /* remdevc: no remote lines */
        if (fedbg())
            fprintf(stderr, "[FECALL] FE_IDEV gen=%u (terminal) -> locdevm=0xF0000000 "
                            "(local minors 1-16), remdevc=0\n", gen);
        return;
    }

    pkt_wr16(rpk, ID_RPK_SUBDEVC, 1);       /* one sub-device */
    if (fedbg()) fprintf(stderr, "[FECALL] FE_IDEV gen=%u -> subdevc=1\n", gen);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_OPEN - open a sub-device. Return disk geometry.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_open_disk(Pkt* rpk) {
    /* devsiz is a DISK-TYPE code = index into the kernel's dist[] table
     * (io/disizes.c), NOT a sector count. rootfs.img was built as a di70 disk
     * (mkndixfs: "newfs /dev/di0a dio70"), which is dist[1]: nspc=90, ssize=1024,
     * dsize=69530 1K-blocks. Returning devsiz=1 makes diopen use di70 geometry
     * and set blkzero=dist[1].nspc=90 - matching the FFS placed at raw sector 90.
     * frmsiz is only consulted for scsi (type>=128); set it to the total 1K
     * blocks anyway. secsiz must be 1024 (dist[1].ssize) so devaddr = daddr.
     *
     * Bit 6 (BSD_PART = 64, machine/disizes.h) selects WHICH of the two
     * partition tables dist[1] carries:
     *     struct dist { ... struct size *sizes[2]; ... }   sizes[0] = logical
     *                                                      sizes[1] = berkeley
     *     io/di.c:203  partition[unit] = ((devsiz & BSD_PART) >> 6);
     *     io/di.c:418  daddr = b_blkno + blkzero[unit]
     *                        + st->sizes[ptype][partno].cyloff * st->nspc;
     * Sending 1|64 = 65 therefore picks di70_sizes (Berkeley), which is what an
     * NDIX /etc/fstab naming /dev/di0a and /dev/di0e describes. For dist[1] the
     * two tables are IDENTICAL for partitions a,b,c,d,e (a=7942@0, b=16720@89,
     * e=27968@364) and differ only in f,g,h - so this bit is not load-bearing
     * for the shipped root+/usr layout, but it states the intent correctly.
     *
     * The EMULATOR does no partition arithmetic at all: the kernel hands us an
     * absolute sector number and we read/write that offset in one flat image. */
    pkt_wr16(rpk, OP_RPK_COMPLETION, 0);
    pkt_wr32(rpk, OP_RPK_DEVSIZ, 1 | 64);       /* dist[1] = di70, BSD_PART */
    pkt_wr32(rpk, OP_RPK_FRMSIZ, 69530);       /* di70 dsize (1K blocks) */
    pkt_wr16(rpk, OP_RPK_SECSIZ, 1024);
    g_ssize = 1024;                             /* match dist[1].ssize for FE_READ */
    if (fedbg()) fprintf(stderr, "[FECALL] FE_OPEN disk: devsiz=65(di70,BSD) secsiz=1024\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_WCON / FE_RCON - synchronous console I/O. The physaddr points at ONE byte.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_tty_out(int unit, const unsigned char* buf, int len);

static void fe_wcon(Nd500Cpu* cpu, uint32_t physaddr_word, Pkt* rpk) {
    /* The char lives in the global `cout = (long)(*cp)` (basic.c:fewcon): a
     * 32-bit big-endian long whose low byte (phys+3) holds the character. */
    uint32_t phys = physaddr_word * 2u - FE_PRIVATE;
    uint8_t ch = nd500_bus_read8(cpu->machine, phys + 3);
    /* Synchronous console write: same emit point as the async TERM_OUT path,
     * so it reaches a telnet terminal too. It is unit 0 by definition (the
     * FE_WCON packet carries no unit). */
    fe_tty_out(FE_CONDEV, &ch, 1);
    pkt_wr16(rpk, 0, 0);  /* completion */
}

static void fe_rcon(Nd500Cpu* cpu, uint32_t physaddr_word, Pkt* rpk) {
    /* Kernel reads (char)cin (basic.c:fercon) - the low byte of the long cin. */
    uint32_t phys = physaddr_word * 2u - FE_PRIVATE;
    int c = getchar();
    if (c == EOF) { pkt_wr16(rpk, 0, 1); return; }  /* completion != 0 = no data */
    nd500_bus_write8(cpu->machine, phys + 3, (uint8_t)c);
    pkt_wr16(rpk, 0, 0);
}

static int fedbg(void) {
    static int v = -1;
    if (v < 0) { const char* e = getenv("ND500X_FEDBG"); v = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return v;
}

/* ---- packet field access: logical (FE_INIT) or physical (ND-100 word) ---- */
/* ═══════════════════════════════════════════════════════════════════════════
 * FE_READ, generic 2 - read ONE tape record into ND-500 memory.
 * cpk _read_cpk_tape (if.h:223): maxbytes@0 (long), physaddr@4 (naddr_t).
 * rpk _read_rpk_xxxx (if.h:236): completion@0, status@2, nbytes@4 (long).
 * The ACTUAL record length goes back in nbytes, which is how the driver
 * learns a short record; a tape mark reads as zero bytes (EOF to the guest).
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_read_tape(Nd500Cpu* cpu, uint32_t cpk_word, Pkt* rpk) {
    Pkt cpk = pkt_word(cpu, cpk_word);
    uint32_t maxbytes = pkt_rd32(&cpk, 0);
    uint32_t physaddr = pkt_rd32(&cpk, 4);          /* ND-100 word address */
    uint32_t dst_phys = physaddr * 2u - FE_PRIVATE;

    uint16_t completion = 0, status = 0;
    uint32_t got = 0;

    if (!fe_tape_open()) {
        completion = 1;                              /* no medium loaded */
    } else {
        static uint8_t buf[65536];
        uint32_t actual = 0, reclen = 0;
        int is_mark = 0, err_flag = 0;
        uint32_t want = (maxbytes > sizeof(buf)) ? (uint32_t)sizeof(buf) : maxbytes;
        if (!nd500_tape_read(&g_tape, buf, want, &actual, &reclen, &is_mark, &err_flag)) {
            completion = 1;                          /* end of medium */
            status = 1;
        } else {
            for (uint32_t i = 0; i < actual; i++)
                nd500_bus_write8(cpu->machine, dst_phys + i, buf[i]);
            got = actual;
            if (err_flag) status = 1;
            /* reclen may exceed actual - a short read. The driver detects that
             * by comparing the nbytes it gets back against what it asked for. */
            (void)reclen; (void)is_mark;
        }
    }

    if (fedbg())
        fprintf(stderr, "[FECALL] FE_READ tape: maxbytes=%u -> %u bytes, pos=%ld, compl=%u\n",
                maxbytes, got, g_tape.pos, completion);

    pkt_wr16(rpk, 0, completion);
    pkt_wr16(rpk, 2, status);
    pkt_wr32(rpk, 4, got);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_DCTL, generic 2 - tape motion.
 * cpk _dctl_cpk_tape (if.h:323): operation@0, parameter@2 (both short).
 * rpk _dctl_rpk_tape (if.h:386): completion@0, status@2, ops@4.
 * mt.c:434-435 fills operation from b_command and parameter from b_repcnt,
 * so the space operations repeat `parameter` times.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_dctl_tape(Nd500Cpu* cpu, uint32_t cpk_word, Pkt* rpk) {
    Pkt cpk = pkt_word(cpu, cpk_word);
    uint16_t op    = pkt_rd16(&cpk, 0);
    uint16_t count = pkt_rd16(&cpk, 2);
    uint16_t completion = 0, status = 0;
    uint32_t n = (count == 0) ? 1u : (uint32_t)count;
    uint32_t i;
    uint32_t h;

    if (!fe_tape_open()) {
        pkt_wr16(rpk, 0, 1);
        pkt_wr16(rpk, 2, 0);
        pkt_wr16(rpk, 4, 0);
        return;
    }

    switch (op) {
        case DCTL_REW:
        case DCTL_REW_UNL:
            nd500_tape_rewind(&g_tape);
            break;

        case DCTL_FSR:                                   /* forward space record */
            for (i = 0; i < n; i++)
                if (!nd500_tape_fwd(&g_tape, &h)) { completion = 1; break; }
            break;

        case DCTL_BSR:                                   /* backward space record */
            for (i = 0; i < n; i++)
                if (!nd500_tape_back(&g_tape, &h)) { completion = 1; break; }
            break;

        case DCTL_FSF:                                   /* forward space file */
            for (i = 0; i < n; i++) {
                for (;;) {
                    if (!nd500_tape_fwd(&g_tape, &h)) { completion = 1; break; }
                    if (h == ND500_TAPE_MARK) break;           /* the mark is consumed */
                }
                if (completion) break;
            }
            break;

        case DCTL_BSF:                                   /* backward space file */
            for (i = 0; i < n; i++) {
                for (;;) {
                    if (!nd500_tape_back(&g_tape, &h)) { completion = 1; break; }
                    if (h == ND500_TAPE_MARK) break;
                }
                if (completion) break;
            }
            break;

        case DCTL_STAT:
            /* Report position, not motion. status bit 0 = at BOT. */
            status = (g_tape.pos == 0) ? 1 : 0;
            break;

        case DCTL_WEOF:
            /* Read-only backing store. Reporting success here would tell the
             * driver a filemark exists where none does. */
            completion = 1;
            if (fedbg())
                fprintf(stderr, "[FECALL] FE_DCTL tape: WEOF refused (image is read-only)\n");
            break;

        default:
            completion = 1;
            break;
    }

    if (fedbg())
        fprintf(stderr, "[FECALL] FE_DCTL tape: op=%u count=%u -> pos=%ld compl=%u status=%u\n",
                op, count, g_tape.pos, completion, status);

    pkt_wr16(rpk, 0, completion);
    pkt_wr16(rpk, 2, status);
    pkt_wr16(rpk, 4, (uint16_t)n);      /* ops actually attempted */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_READ - read sectors from the disk image and DMA into ND-500 memory.
 * cpk: nbytes@0, physaddr@4 (ND-100 word), devaddr@8 (sector index).
 * DMA target physical byte = physaddr*2 - private. Image offset = devaddr*ssize.
 * ASYNC: after the DMA + rpk fill, the completion must be delivered via an
 * interrupt (TODO nd500_fecall_deliver_interrupt) - a bare return leaves the
 * kernel asleep in biowait.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_read_disk(Nd500Cpu* cpu, uint32_t device, uint32_t cpk_word, uint32_t rpk_word, Pkt* rpk) {
    Pkt cpk = pkt_word(cpu, cpk_word);
    if (fedbg()) {
        fprintf(stderr, "[FECALL] FE_READ raw cpk@word=0x%08X phys=0x%08X:", cpk_word, cpk.base);
        for (uint32_t i = 0; i < 20; i += 4)
            fprintf(stderr, " [%u]=0x%08X", i, nd500_bus_read32(cpu->machine, cpk.base + i));
        fprintf(stderr, "\n");
    }
    uint32_t nbytes   = pkt_rd32(&cpk, RD_CPK_NBYTES);
    uint32_t physaddr = pkt_rd32(&cpk, RD_CPK_PHYSADDR);   /* ND-100 word */
    uint32_t devaddr  = pkt_rd32(&cpk, RD_CPK_DEVADDR);    /* sector index */

    uint32_t dst_phys = physaddr * 2u - FE_PRIVATE;
    long     img_off  = (long)devaddr * (long)g_ssize;

    if (fedbg())
        fprintf(stderr, "[FECALL] FE_READ nbytes=%u devaddr=%u img_off=0x%lX dst_phys=0x%08X\n",
                nbytes, devaddr, img_off, dst_phys);

    uint32_t done = 0;
    if (g_disk && fseek(g_disk, img_off, SEEK_SET) == 0) {
        uint8_t buf[2048];
        while (done < nbytes) {
            uint32_t chunk = nbytes - done;
            if (chunk > sizeof(buf)) chunk = sizeof(buf);
            size_t got = fread(buf, 1, chunk, g_disk);
            if (got == 0) break;
            for (size_t i = 0; i < got; i++)
                nd500_bus_write8(cpu->machine, dst_phys + done + (uint32_t)i, buf[i]);
            done += (uint32_t)got;
        }
    }

    /* Diagnostic: when the inode block (devaddr 122 = fs_iblkno) is DMA'd, dump
     * root inode 2's i_db[0] (dinode offset 40, inode 2 at block offset 2*128)
     * as the kernel will read it, to check the root-dir block pointer. */
    if (nd_env_flag("ND500X_INODEDBG", &g_envf_inodedbg) && devaddr == 122) {
        uint32_t o = dst_phys + 2u*128u + 40u;
        uint32_t v = ((uint32_t)nd500_bus_read8(cpu->machine,o)<<24)
                   | ((uint32_t)nd500_bus_read8(cpu->machine,o+1)<<16)
                   | ((uint32_t)nd500_bus_read8(cpu->machine,o+2)<<8)
                   |  (uint32_t)nd500_bus_read8(cpu->machine,o+3);
        uint32_t o3 = dst_phys + 3u*128u + 40u;
        uint32_t v3 = ((uint32_t)nd500_bus_read8(cpu->machine,o3)<<24)
                    | ((uint32_t)nd500_bus_read8(cpu->machine,o3+1)<<16)
                    | ((uint32_t)nd500_bus_read8(cpu->machine,o3+2)<<8)
                    |  (uint32_t)nd500_bus_read8(cpu->machine,o3+3);
        fprintf(stderr, "[INODEDBG] inode-block DMA'd to 0x%08X: root(2).i_db[0]=%u  lostfound(3).i_db[0]=%u\n", dst_phys, v, v3);
    }

    pkt_wr16(rpk, RD_RPK_COMPLETION, 0);          /* success */
    pkt_wr16(rpk, RD_RPK_STATUS, 0);
    pkt_wr32(rpk, RD_RPK_NBYTES, done);

    /* async: queue a completion interrupt (delivered by cpu_step at the next
     * safe boundary) so diintr()->iodone() wakes the biowait() sleeper. */
    /* Interrupt on the disk's ACTUAL generic device (the root disk is gen 7 in
     * this config, not the nominal DISK=1) so dispatch calls its diintr. */
    cpu->fe_int_pending = 1;
    cpu->fe_int_gen = (device >> 16) & 0xFFFF;
    cpu->fe_int_sub = device & 0xFFFF;
    cpu->fe_int_rpk = rpk_word;
    (void)rpk;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Deliver a pending ND-100 completion interrupt: replicate the ND-500 hardware
 * interrupt entry - save the live (interrupted) context into cxbtab[ip_current],
 * queue an int_descr on the IPL record, and vector to _intvec. The kernel's
 * _intvec -> dispatch -> diintr -> iodone -> B_DONE -> wakeup completes the I/O,
 * then lcntxt restores cxbtab[ip_current] and resumes the interrupted code.
 * Called from cpu_step; only fires once the CPU has dropped below IPL_DK.
 * ═══════════════════════════════════════════════════════════════════════════ */
#define K_IPLP      0x0001cb20u   /* BSS: holds ptr to ipl_rec */
#define K_SHSEG     0x0001cb24u   /* BSS: shseg (ND-100 byte base of shared seg) */
#define K_SHAREBASE 0x30000000u   /* KVA of shared seg start */
#define K_CXBTAB    0x40000008u   /* KVA of context block table; cxbtab[ipl]=+ipl*256 */
#define K_INTVEC    0x000004edu   /* interrupt vector */
#define FE_IPL_DK   4
#define IP_NEXT_OFF 0
#define IP_CURR_OFF 4
#define ID_NEXT_OFF     0
#define ID_IPL_OFF      4
#define ID_GEN_DEV_OFF  10
#define ID_SUB_DEV_OFF  12
#define ID_RESP_PKT_OFF 18
/* Our int_descr + terminator live near the end of iplrec's shared-seg page
 * (0x30001000-0x30001800, mapped), past the kernel's shared-seg globals
 * (iplrec@0x1000, clockrec@0x1040 ... sub_dev_descrip@0x1080). MUST NOT collide
 * with _clockrec (0x30001040) or the ND-100 tick count is corrupted. */
#define INTDESC_OFF 0x00001780u
#define GEN_CLOCK   5            /* CLOCK generic device (machine/if.h) */
#define IPL_CL      3            /* clock interrupt priority (machine/icb.h) */
#define K_CLOCKREC  0x30001040u  /* clock_rec.cr_ticks (shared seg) */
#define FE_CLOCK_PERIOD 50000ull /* emulated instructions per ~40ms clock tick */

/* Build the int_descr (+ terminator), save the live context into cxbtab[ip_cur],
 * queue the descriptor on the IPL record and vector to _intvec, for one interrupt
 * at level `ipl` from generic device `gen` (sub `sub`, response-packet word `rpk`).
 * Caller has read iplrec/ip_cur/shseg and confirmed the base level. */
static void fe_deliver(Nd500Cpu* cpu, uint32_t iplrec, uint16_t ip_cur, uint32_t shseg,
                       uint32_t gen, uint32_t sub, uint32_t rpk, uint32_t ipl) {
    /* An ND-100 interrupt reaches the ND-500 whatever domain is executing, and
     * the kernel is written for both cases: _intvec's first act (machine/
     * locore.c:768 "Findstk") is to read cxbtab[ip_cur].cx_ced and branch to
     * New_Kstack - a fresh _Kstack - when the interrupted context was a user
     * domain, versus Old_Kstack when it was the kernel itself.
     *
     * Everything written below - the shared-segment descriptors, iplrec,
     * cxbtab - is a KERNEL address that a user domain holds no capability for,
     * so the switch has to happen before the first write, not after. The
     * interrupted CED/CAD/ST1 are captured first and stored into the context
     * block, and _intvec's closing `lcntxt $CNTXMASK` restores all three
     * (CNTXMASK bits 23-25 = THA/CED/CAD, and bit 17 = ST1 carries the
     * interrupted domain's PiA back), so the user domain resumes unchanged. */
    uint32_t int_ced = cpu->CED, int_cad = cpu->CAD, int_st1 = cpu->ST1;
    if (int_ced != 0) {
        cpu->CED = 0;
        /* CAD == CED so that a `ret` with prev_b == 0 anywhere in the handler
         * cannot be mistaken for a domain return (Ret.c gates on CAD != CED). */
        cpu->CAD = 0;
        nd500_apply_domain_pia(cpu, 0);
    }
    uint32_t idp_kva  = K_SHAREBASE + INTDESC_OFF;
    uint32_t term_kva = K_SHAREBASE + INTDESC_OFF + 0x40u;   /* terminator descriptor */

    /* dispatch()'s queue scan (trap.c:696) computes idtmp = htob(ip_next)-shseg
     * +sharebase and reads idtmp->id_ipl BEFORE the emptiness check fully guards
     * it. On real hardware that speculative read lands in mapped shared memory;
     * here it would fault if ip_next were 0 (idtmp below sharebase). So the last
     * descriptor's id_next points at a ZEROED terminator (id_ipl=0, self-linked)
     * living in the mapped shared-seg page - dispatch reads id_ipl=0 (< prev),
     * stops cleanly, and never dereferences an unmapped address. */
    uint32_t term_word = ((INTDESC_OFF + 0x40u) + shseg) >> 1;   /* ND-100 word of term */
    nd500_write_memory_32(cpu, term_kva + ID_NEXT_OFF, term_word);   /* self-link */
    nd500_write_memory_16(cpu, term_kva + ID_IPL_OFF, 0);
    nd500_write_memory_16(cpu, term_kva + ID_GEN_DEV_OFF, 0);
    nd500_write_memory_16(cpu, term_kva + ID_SUB_DEV_OFF, 0);
    nd500_write_memory_32(cpu, term_kva + ID_RESP_PKT_OFF, 0);

    nd500_write_memory_32(cpu, idp_kva + ID_NEXT_OFF, term_word);
    nd500_write_memory_16(cpu, idp_kva + ID_IPL_OFF, (uint16_t)ipl);
    nd500_write_memory_16(cpu, idp_kva + ID_GEN_DEV_OFF, (uint16_t)gen);
    nd500_write_memory_16(cpu, idp_kva + ID_SUB_DEV_OFF, (uint16_t)sub);
    nd500_write_memory_32(cpu, idp_kva + ID_RESP_PKT_OFF, rpk);

    /* iplp->ip_next = ND-100 word addr of int_descr.
     * _intvec: idp_kva = htob(ip_next) - shseg + sharebase
     * => ip_next = (idp_kva - sharebase + shseg) >> 1 = (INTDESC_OFF + shseg) >> 1 */
    uint32_t ip_next_word = (INTDESC_OFF + shseg) >> 1;
    nd500_write_memory_32(cpu, iplrec + IP_NEXT_OFF, ip_next_word);

    /* Save live context into cxbtab[ip_cur] (PHYSICAL, fixed (reg-1)*4 slots -
     * matching the kernel CX_ offsets and the lcntxt restore at _intvec exit). */
    uint32_t cx_phys = nd500_mmu_translate(cpu, K_CXBTAB + (uint32_t)ip_cur * 256u, 1, 0);
    nd500_bus_write32(cpu->machine, cx_phys +  0*4, cpu->PC);   /* 1  P  */
    nd500_bus_write32(cpu->machine, cx_phys +  1*4, cpu->L);    /* 2  L  */
    nd500_bus_write32(cpu->machine, cx_phys +  2*4, cpu->B);    /* 3  B  */
    nd500_bus_write32(cpu->machine, cx_phys +  3*4, cpu->R);    /* 4  R  */
    nd500_bus_write32(cpu->machine, cx_phys +  4*4, cpu->I[0]); /* 5  I1 */
    nd500_bus_write32(cpu->machine, cx_phys +  5*4, cpu->I[1]);
    nd500_bus_write32(cpu->machine, cx_phys +  6*4, cpu->I[2]);
    nd500_bus_write32(cpu->machine, cx_phys +  7*4, cpu->I[3]);
    nd500_bus_write32(cpu->machine, cx_phys +  8*4, cpu->A[0]); /* 9  A1 */
    nd500_bus_write32(cpu->machine, cx_phys +  9*4, cpu->A[1]);
    nd500_bus_write32(cpu->machine, cx_phys + 10*4, cpu->A[2]);
    nd500_bus_write32(cpu->machine, cx_phys + 11*4, cpu->A[3]);
    nd500_bus_write32(cpu->machine, cx_phys + 12*4, cpu->E[0]); /* 13 E1 */
    nd500_bus_write32(cpu->machine, cx_phys + 13*4, cpu->E[1]);
    nd500_bus_write32(cpu->machine, cx_phys + 14*4, cpu->E[2]);
    nd500_bus_write32(cpu->machine, cx_phys + 15*4, cpu->E[3]);
    nd500_bus_write32(cpu->machine, cx_phys + 16*4, int_st1);   /* 17 ST1 */
    nd500_bus_write32(cpu->machine, cx_phys + 17*4, cpu->ST2);  /* 18 ST2 */
    nd500_bus_write32(cpu->machine, cx_phys + 22*4, cpu->THA);  /* 23 THA */
    nd500_bus_write32(cpu->machine, cx_phys + 23*4, int_ced);   /* 24 CED */
    nd500_bus_write32(cpu->machine, cx_phys + 24*4, int_cad);   /* 25 CAD */

    if (fedbg())
        fprintf(stderr, "[FECALL] deliver INT gen=%u sub=%u rpk=0x%08X ipl=%u ip_cur=%u -> _intvec (P was 0x%08X)\n",
                gen, sub, rpk, ipl, ip_cur, cpu->PC);

    cpu->PC = K_INTVEC;          /* vector to the interrupt handler */
}

/* ---- console INPUT: host stdin -> guest mx_bin ring ------------------------
 * io/mx.c struct b_ex_h (all 16-bit fields): bx_sem@0 bx_status@2 bx_control@4
 * bx_head@6 bx_free@8 bx_max@10 bx_buf@12 (BEX_TTY=1000 shorts). Element =
 * (sub<<8)|char. The front-end (us) is the PRODUCER: deposit at bx_buf[bx_free]
 * and advance bx_free mod bx_max; mxpoll() (run off every clock tick) is the
 * consumer draining bx_head -> bx_free into the tty line discipline. mx.c does
 * no overflow test ("in fact no test is done about overflow"), so the producer
 * must stop at head == (free+1) mod max. The ring's location arrives in the
 * held TERM_IN FE_READ cpk: physaddr@4 = dton(&mx_bin) (ND-100 word addr).
 * No completion interrupt is needed for data - the FE_READ rpk is only for
 * carrier/error events (mxrintr SD_READ), and consumption rides the clock.
 *
 * Host side: the debugger REPL thread pushes bytes with
 * nd500_fecall_console_input(); the CPU thread drains them into the guest ring
 * from nd500_fecall_tick(). The queue is mutex-protected; g_conq_avail is a
 * lock-free fast-path hint so the per-instruction tick stays cheap. */
static uint32_t g_termin_ring = 0;   /* physical byte addr of mx_bin (0 = none) */
static volatile int g_conq_avail = 0;

/* One host queue serves every unit, because the GUEST ring is also shared:
 * mx_bin elements are (unit << 8) | char, so the unit has to travel with the
 * byte instead of living in a per-unit queue that would then have to be
 * re-interleaved. Keeping one FIFO also preserves the typing order between
 * lines that arrive on different terminals. */
#define FE_CONQ_SIZE 4096
typedef struct { uint8_t unit; char ch; } FeConqEntry;
static FeConqEntry g_conq[FE_CONQ_SIZE];
static int  g_conq_head = 0, g_conq_tail = 0;
static pthread_mutex_t g_conq_mtx = PTHREAD_MUTEX_INITIALIZER;

/* The guest's erase character is DEL (0177). h/ttychars.h:36 sets CERASE that
 * way, and the guest confirms it at runtime - "stty everything" on the console
 * reports "erase ^?" with the line in cooked mode (-raw -cbreak).
 *
 * Measured: sending 0x7F erases (echo abcZ<DEL>Y printed "abcY"), sending 0x08
 * does not (echo defZ<BS>W printed "defZ\bW" - the byte went through as data).
 * So on any terminal whose Backspace key emits BS rather than DEL, backspace
 * silently does nothing. Translating here is what a real terminal server did,
 * and it is the only place we can do it: the NDIX side is frozen source.
 *
 * ND500X_RAW_BS=1 turns the translation off, for a program that genuinely wants
 * ^H as data - vi bound to ^H for cursor-left being the obvious one. */
static int fe_translate_bs(void) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("ND500X_RAW_BS"); on = (e && e[0] && e[0] != '0') ? 0 : 1; }
    return on;
}

void nd500_fecall_tty_input(int unit, const char* buf, int len) {
    int i;
    int xlat = fe_translate_bs();
    if (unit < 0 || unit > 255) return;        /* element field is 8 bits */
    pthread_mutex_lock(&g_conq_mtx);
    for (i = 0; i < len; i++) {
        int nt = (g_conq_tail + 1) % FE_CONQ_SIZE;
        if (nt == g_conq_head) break;          /* host queue full - drop rest */
        g_conq[g_conq_tail].unit = (uint8_t)unit;
        g_conq[g_conq_tail].ch   = (xlat && buf[i] == 0x08) ? 0x7F : buf[i];
        g_conq_tail = nt;
    }
    g_conq_avail = (g_conq_tail != g_conq_head);
    pthread_mutex_unlock(&g_conq_mtx);
}

void nd500_fecall_console_input(const char* buf, int len) {
    nd500_fecall_tty_input(FE_CONDEV, buf, len);
}

/* ---- console OUTPUT: guest -> host sinks ---------------------------------
 * Guest tty output arrives one FE_WRIT chunk at a time on a known unit. The
 * local stdout copy is unconditional for unit 0 (so a boot log keeps working
 * even with a telnet client attached) and suppressed for other units once a
 * sink owns them. */
static Nd500TtyOutFunc g_tty_out[ND500_TTY_MAX_UNITS];
static void*           g_tty_out_ctx[ND500_TTY_MAX_UNITS];

void nd500_fecall_set_tty_output(int unit, Nd500TtyOutFunc fn, void* ctx) {
    if (unit < 0 || unit >= ND500_TTY_MAX_UNITS) return;
    g_tty_out[unit] = fn;
    g_tty_out_ctx[unit] = ctx;
}

/* Emit one already-masked chunk of guest output for <unit>. */
static void fe_tty_out(int unit, const unsigned char* buf, int len) {
    Nd500TtyOutFunc fn = NULL;
    void* ctx = NULL;
    if (unit >= 0 && unit < ND500_TTY_MAX_UNITS) {
        fn = g_tty_out[unit];
        ctx = g_tty_out_ctx[unit];
    }
    if (fn) fn(unit, buf, len, ctx);
    if (unit == FE_CONDEV || !fn) {
        int i;
        for (i = 0; i < len; i++) putchar((int)buf[i]);
        fflush(stdout);
    }
}

/* The emulated console is a 7-bit ASCII terminal, so bit 7 of an outgoing byte
 * is a parity bit, not data - mask it off.
 *
 * This is a POLICY choice about what is on the other end of the line, not a
 * fact derived from the guest, so here is the evidence behind it.
 *
 * 4.3BSD getty generates parity IN SOFTWARE and UNCONDITIONALLY -
 * baseline/etc/getty/main.c putchr():
 *     c |= partab[c&0177] & 0200;
 *     if (OP) c ^= 0200;
 * Nothing suppresses it: setflags() has cases for ap/op/ep but none for np, and
 * putchr does not consult the tty mode at all. Captured from the wire, getty's
 * banner is
 *     8D 0A 8D 0A 4E 44 C9 D8 2D C3 A0 28 6C A9 ... 6C 6F E7 69 EE 3A A0
 * = "\r\n\r\nNDIX-C (l)\r\n\r\r\n\rlogin: " with bit 7 set on exactly those
 * characters whose low 7 bits have odd population. login and the shell were
 * clean because they do not do this.
 *
 * Keying the mask on the line configuration does NOT work. io/mx.c mxparam()
 * reports the mode via FE_DCTL DCTL_CHG_FLGS (DCTL_PAR_EVEN/ODD/RAW), but getty
 * sets the line RAW before printing its prompt, so the very output that carries
 * software parity arrives on a line advertised as 8-bit clean. That was measured,
 * not assumed: the kernel's own "NDIX startup: /etc/rc running" went out on a
 * cooked line and getty's banner on a raw one, in the same boot.
 *
 * Masking is safe for everything else on this path: cooked output is already
 * 7-bit by construction (sys/tty.c ttyoutput() starts with "c &= 0177"), and the
 * console is an ASCII terminal with no 8-bit traffic. Set ND500X_CONSOLE_8BIT=1
 * to pass all eight bits through if you ever need binary console output.
 */
static int fe_console_8bit(void) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("ND500X_CONSOLE_8BIT"); on = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return on;
}

static void fe_conq_drain(Nd500Cpu* cpu) {
    Nd500Machine* m = cpu->machine;
    if (!m) return;
    /* Diagnostic (FEDBG): the ring address was captured from dton(&mx_bin) at
     * FE_READ time. _mx_bin lives at kernel VA 0x3801A480 (locore.s PARTS
     * block, segment 7) - verify the CURRENT guest translation still points
     * at the captured physical page; if not, the cached address is stale and
     * ring writes would corrupt whatever now owns that page. */
    if (fedbg()) {
        uint32_t now = nd500_mmu_translate_domain(cpu, 0x3801A800u, 0, 0, 0);
        static uint32_t last_warn = 0;
        if (now != g_termin_ring && now != last_warn) {
            last_warn = now;
            fprintf(stderr, "[FECALL] mx_bin phys MOVED: cached=0x%08X live=0x%08X\n",
                    g_termin_ring, now);
        }
    }
    pthread_mutex_lock(&g_conq_mtx);
    uint16_t head = nd500_bus_read16(m, g_termin_ring + 6);
    uint16_t bfree = nd500_bus_read16(m, g_termin_ring + 8);
    uint16_t max  = nd500_bus_read16(m, g_termin_ring + 10);
    if (max == 0 || max > 1000 || head >= max || bfree >= max) {
        pthread_mutex_unlock(&g_conq_mtx);    /* ring not initialised yet */
        return;
    }
    while (g_conq_head != g_conq_tail) {
        uint16_t nf = (uint16_t)((bfree + 1) % max);
        if (nf == head) break;                 /* guest ring full - retry later */
        uint16_t el = (uint16_t)((g_conq[g_conq_head].unit << 8) |
                                 (uint8_t)g_conq[g_conq_head].ch);
        nd500_bus_write16(m, g_termin_ring + 12u + 2u * bfree, el);
        bfree = nf;
        g_conq_head = (g_conq_head + 1) % FE_CONQ_SIZE;
    }
    nd500_bus_write16(m, g_termin_ring + 8, bfree);
    g_conq_avail = (g_conq_tail != g_conq_head);
    pthread_mutex_unlock(&g_conq_mtx);
}

/* Called every instruction from cpu_step. Delivers, at the kernel base level
 * (spl0, idle/biowait), either a pending disk/dctl completion interrupt or a
 * periodic clock tick (bumping the ND-100 tick count first) so hardclock() runs
 * and drives timekeeping + the scheduler. */
unsigned long long g_tick_gated = 0, g_tick_seen = 0;
unsigned long long g_tick_due_any = 0, g_tick_user = 0;
unsigned long long g_tick_at844 = 0, g_tick_ced0_not844 = 0;
unsigned long long g_tick_userdeliv = 0;   /* clock ticks delivered from a user domain */
unsigned long long g_tick_latched = 0;     /* clock ticks latched for later delivery */
/* Ticks that came due while no boundary was eligible. See the long note at the
 * latch site in nd500_fecall_tick(). */
#define FE_CLOCK_MAX_PENDING 4u
static unsigned g_clock_pending = 0;
static void tick_report(void) {
    const char* e = getenv("ND500X_TICKSTAT");
    if (!e || !e[0] || e[0]=='0') return;
    fprintf(stderr, "[TICKSTAT] clock ticks due=%llu suppressed_by_trap_gate=%llu (%.1f%%)\n",
            g_tick_seen, g_tick_gated,
            g_tick_seen ? 100.0*(double)g_tick_gated/(double)g_tick_seen : 0.0);
}
void nd500_fecall_tick(Nd500Cpu* cpu) {
    { static int reg = 0; if (!reg) { reg = 1; atexit(tick_report); } }
    if (!cpu) return;
    /* Front-end "DMA" of queued console input into the mx_bin ring: pure
     * physical-memory writes, safe at any instruction boundary (the real
     * ND-100 wrote this ring concurrently with ND-500 execution). */
    if (g_conq_avail && g_termin_ring) fe_conq_drain(cpu);
    /* Counted ABOVE the CED gate on purpose: the counter below it can only ever
     * agree with the gate, so it cannot tell us whether ticks are DUE while a
     * user process runs. This one can. */
    {
        extern unsigned long long g_tick_due_any, g_tick_user;
        static int st2 = -1;
        if (st2 < 0) { const char* e = getenv("ND500X_TICKSTAT"); st2 = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (st2 && cpu->instruction_count && (cpu->instruction_count % FE_CLOCK_PERIOD) == 0) {
            g_tick_due_any++;
            if (cpu->CED != 0) g_tick_user++;
            { extern unsigned long long g_tick_at844, g_tick_ced0_not844;
              if (cpu->PC == 0x00000844u) g_tick_at844++;
              else if (cpu->CED == 0)     g_tick_ced0_not844++; }
            if ((g_tick_due_any % 200ull) == 0)
                { extern unsigned long long g_tick_at844, g_tick_ced0_not844;
                  fprintf(stderr, "[TICKALL] due=%llu user_domain=%llu at_0x844=%llu kernel_but_not_844=%llu latched=%llu userdeliv=%llu pend=%u\n",
                        g_tick_due_any, g_tick_user, g_tick_at844, g_tick_ced0_not844,
                        g_tick_latched, g_tick_userdeliv, g_clock_pending); }
        }
    }
    /* Periodic clock can be disabled for isolation (ND500X_NOFECLOCK=1). */
    static int noclk = -1;
    if (noclk < 0) { const char* e = getenv("ND500X_NOFECLOCK"); noclk = (e && e[0] && e[0] != '0') ? 1 : 0; }
    int clock_due = !noclk && cpu->instruction_count && (cpu->instruction_count % FE_CLOCK_PERIOD) == 0;

    /* A tick is a one-shot test on ONE instruction out of FE_CLOCK_PERIOD, but
     * every gate below rejects most instructions - so testing for the tick and
     * gating it in the same breath DISCARDED the tick whenever the two did not
     * coincide, rather than deferring it. Measured at an idle login prompt with
     * ND500X_TICKSTAT: 740 of 8800 due ticks (8.4%) found the kernel at 0x844,
     * so ~92% of the clock was being thrown away even with the machine doing
     * nothing. Latch it here and deliver at the next eligible boundary instead.
     * Capped, so a long ineligible stretch cannot then fire a burst of catch-up
     * interrupts at the kernel the moment it becomes eligible. */
    if (clock_due && g_clock_pending < FE_CLOCK_MAX_PENDING) {
        g_clock_pending++;
        g_tick_latched++;
    }

    /* ── a USER domain is executing ──────────────────────────────────────────
     * Everything below this block is written for CED == 0 and gates delivery on
     * the kernel being parked in swtch()'s idle spin at PC 0x844. That gate
     * exists to stop a DEVICE COMPLETION landing between a driver's async
     * fecall and its following `flag++; sleep(&flag)`, which would run the
     * handler while flag == 0 and leave the driver asleep forever.
     *
     * That hazard is about kernel code being half way through a sequence. When
     * CED != 0 the kernel is not executing at all, so there is no such sequence
     * to land inside - this is the safest point in the whole run to interrupt.
     * Without delivery here nothing ever preempts a compute-bound process:
     * hardclock() is what drives roundrobin/setpri, and measured with
     * ND500X_TICKSTAT a spinning `yes` drove at_0x844 to a standstill while 44%
     * of due ticks were being dropped by the old `if (CED != 0) return`.
     *
     * The periodic clock ONLY. Device completions keep the 0x844 rule, because
     * for them the hazard above is real whatever domain happens to be running.
     * ND500X_NOUSERCLOCK=1 restores the old behaviour. */
    if (cpu->CED != 0) {
        static int nouclk = -1;
        if (nouclk < 0) { const char* e = getenv("ND500X_NOUSERCLOCK"); nouclk = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (nouclk || !g_clock_pending) return;
        /* Same CALL/ENT* interlock as the kernel path below. */
        if (cpu->pending_call_return_address != 0) return;
        /* A trap handler running in a user domain is mid-dispatch; leave it. */
        if (cpu->in_trap_handler) return;

        /* The IPL record, cxbtab and the shared segment are kernel addresses,
         * so read them through the kernel domain, and put the user domain back
         * if we end up not delivering. fe_deliver() performs the real switch
         * itself and saves the interrupted CED/CAD/ST1 into the context block. */
        uint32_t u_ced = cpu->CED, u_cad = cpu->CAD;
        cpu->CED = 0; cpu->CAD = 0;
        uint32_t iplrec = nd500_read_memory_32(cpu, K_IPLP);
        uint16_t ip_cur = iplrec ? nd500_read_memory_16(cpu, iplrec + IP_CURR_OFF) : 1;
        if (iplrec == 0 || ip_cur != 0) {          /* not at the kernel base level */
            cpu->CED = u_ced; cpu->CAD = u_cad;
            return;
        }
        uint32_t shseg = nd500_read_memory_32(cpu, K_SHSEG);
        uint32_t t = nd500_read_memory_32(cpu, K_CLOCKREC);
        nd500_write_memory_32(cpu, K_CLOCKREC, t + 2);   /* ~40ms = 2 x 20ms ticks */
        cpu->CED = u_ced; cpu->CAD = u_cad;   /* fe_deliver saves these, then switches */
        g_clock_pending--;
        fe_deliver(cpu, iplrec, ip_cur, shseg, GEN_CLOCK, 0, 0, IPL_CL);
        g_tick_userdeliv++;
        return;
    }
    /* in_trap_handler alone must NOT block delivery: NDIX sleeps INSIDE trap
     * context (pagein -> biowait -> swtch to idle) and spins at 0x844 with the
     * emulator's in_trap_handler still set - the trap only "returns" (lregbl)
     * after the sleeper is woken BY this very interrupt. The PC==0x844 idle-
     * spin gate below already guarantees the safe boundary; blocking on
     * in_trap_handler starved the exec text-read completion forever. */
    /* ND500X_TICKSTAT=1: count how often the clock tick is SUPPRESSED by this
     * gate versus delivered. The clock drives hardclock() and therefore
     * preemption, so if a CPU-bound user process keeps the machine in trap
     * context this gate can starve the scheduler - which is exactly what a
     * spinning `yes` looks like from the console. Measure it, do not assume. */
    {
        extern unsigned long long g_tick_gated, g_tick_seen;
        static int st = -1;
        if (st < 0) { const char* e = getenv("ND500X_TICKSTAT"); st = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (st && cpu->instruction_count && (cpu->instruction_count % FE_CLOCK_PERIOD) == 0) {
            g_tick_seen++;
            if (cpu->in_trap_handler && cpu->PC != 0x00000844u) g_tick_gated++;
            if ((g_tick_seen % 200ull) == 0)
                fprintf(stderr, "[TICKSTAT] due=%llu suppressed=%llu (%.1f%%)\n",
                        g_tick_seen, g_tick_gated,
                        100.0*(double)g_tick_gated/(double)g_tick_seen);
        }
    }
    if (cpu->in_trap_handler && cpu->PC != 0x00000844u) return;
    if (!cpu->fe_int_pending && !g_clock_pending) return;   /* fast path */

    /* Never interrupt between a CALL/CALLG and its ENT* - the emulator's
     * pending-call state is not part of the saved context, so an interrupt
     * there would leave the ENT* with "no preceding CALL" (ISE) on return. On
     * the ND-500 a CALL and its entry are a coupled pair, so this is faithful. */
    if (cpu->pending_call_return_address != 0) return;

    /* Deliver ONLY when the kernel is genuinely idle in swtch()'s spin (PC at
     * the idle-loop splx, 0x844), not merely at spl0 mid-instruction-stream. A
     * completion delivered between a driver's async fecall and its subsequent
     * `flag++; sleep(&flag)` would run the handler while flag==0 (no wakeup),
     * then the code sets flag=1 and sleeps forever. Waiting for the swtch idle
     * spin guarantees the sleeper has parked and its wait-flag is set. */
    if (cpu->PC != 0x00000844u) return;

    uint32_t iplrec = nd500_read_memory_32(cpu, K_IPLP);
    if (iplrec == 0) return;
    uint16_t ip_cur = nd500_read_memory_16(cpu, iplrec + IP_CURR_OFF);
    if (nd_env_flag("ND500X_GATEDBG", &g_envf_gatedbg)) {
        static uint64_t gn = 0;
        if (gn++ < 25) {
            uint32_t rpaddr = nd500_mmu_translate(cpu, iplrec + IP_CURR_OFF, 0, 0);
            fprintf(stderr, "[GATEDBG] @0x844 iplrec=0x%08X ip_cur=%u rd_paddr=0x%08X CED=%d gen=%d pcall=0x%08X fe_pend=%d clk_due=%d icnt=%llu\n",
                    iplrec, ip_cur, rpaddr, cpu->CED, (int)cpu->fe_int_gen, cpu->pending_call_return_address,
                    (int)cpu->fe_int_pending, clock_due, (unsigned long long)cpu->instruction_count);
        }
    }
    if (ip_cur != 0) {
        if (cpu->fe_int_pending && fedbg()) {
            static uint64_t last = 0;
            if (cpu->instruction_count - last > 300000) {
                last = cpu->instruction_count;
                uint32_t iplock = nd500_read_memory_32(cpu, iplrec + 8);
                uint32_t ipnext = nd500_read_memory_32(cpu, iplrec + IP_NEXT_OFF);
                fprintf(stderr, "[FECALL] INT blocked: ip_cur=%u IP_LOCK=0x%08X ip_next=0x%08X B=0x%08X (pending gen=%u)\n",
                        ip_cur, iplock, ipnext, cpu->B, cpu->fe_int_gen);
                uint32_t b = cpu->B;
                for (int lvl = 0; lvl < 10 && b >= 0xE8000000u && b < 0xE8100000u; lvl++) {
                    uint32_t prevb = nd500_read_memory_32(cpu, b + 0);
                    uint32_t reta  = nd500_read_memory_32(cpu, b + 4);
                    fprintf(stderr, "[FECALL]   frame L%d B=0x%08X RETA=0x%08X\n", lvl, b, reta);
                    if (prevb == b || prevb == 0) break;
                    b = prevb;
                }
            }
        }
        return;   /* only at the base level - retry next instruction */
    }
    uint32_t shseg = nd500_read_memory_32(cpu, K_SHSEG);

    /* Pending disk/dctl completion takes priority over the periodic clock. */
    if (cpu->fe_int_pending) {
        /* Deliver at the priority the DEVICE asked for at connect time, not at
         * the disk level. Every completion used to go out at FE_IPL_DK (4),
         * while SIINTR's own priority is IPL_SI = 3 (machine/icb.h:87) - the
         * value if/si.c:39 puts in Idev_cpk.ipl before calling FE_IDEV. The
         * clock has always passed its IPL_CL explicitly (below); this makes
         * every other device do the same. Devices that never went through
         * FE_IDEV, or whose ipl field was out of range, keep the old default. */
        uint32_t ipl = FE_IPL_DK;
        if (cpu->fe_int_gen < (sizeof cpu->fe_dev_ipl / sizeof cpu->fe_dev_ipl[0])
            && cpu->fe_dev_ipl[cpu->fe_int_gen] != 0)
            ipl = cpu->fe_dev_ipl[cpu->fe_int_gen];

        /* SIINTR delivery trace (env ND500X_SIDBG). */
        if (cpu->fe_int_gen == GEN_SIINTR) {
            static int sidbg = -1;
            if (sidbg < 0) { const char* e = getenv("ND500X_SIDBG"); sidbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (sidbg)
                fprintf(stderr, "[SIDBG] deliver interrupt gen=8 sub=%u rpk=0x%08X ipl=%u (IPL_SI=3) PC=0x%08X CED=%u\n",
                        cpu->fe_int_sub, cpu->fe_int_rpk, ipl, cpu->PC, cpu->CED);
        }
        fe_deliver(cpu, iplrec, ip_cur, shseg,
                   cpu->fe_int_gen, cpu->fe_int_sub, cpu->fe_int_rpk, ipl);
        cpu->fe_int_pending = 0;
        return;
    }
    /* Periodic clock: advance the ND-100 tick count, then interrupt hardclock. */
    g_clock_pending--;
    uint32_t t = nd500_read_memory_32(cpu, K_CLOCKREC);
    nd500_write_memory_32(cpu, K_CLOCKREC, t + 2);   /* ~40ms = 2 x 20ms ticks */
    fe_deliver(cpu, iplrec, ip_cur, shseg, GEN_CLOCK, 0, 0, IPL_CL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Entry point: called from nd500_indirect.c for MON 600 (offset 0x180).
 * arg_addresses[0..3] are the effective addresses of device/request/rpk/cpk.
 * Returns 0 (handled).
 * ═══════════════════════════════════════════════════════════════════════════ */
int nd500_fecall(Nd500Cpu* cpu, uint32_t arg_count, const uint32_t* arg_addresses) {
    if (!cpu || arg_count < 4) return -1;

    if (!g_disk) {
        const char* disk_path = getenv("ND500X_DISK");
        if (!disk_path || !disk_path[0]) {
            fprintf(stderr, "[FECALL] no root disk image: pass --ndix <image> "
                            "or set ND500X_DISK\n");
            return -1;
        }
        /* ND500X_DISK_RW=1: copy-on-write session. The MASTER image is copied
         * to <image>.session (overwriting any previous session) and all reads
         * AND writes go to the session copy - the master stays pristine. A
         * good session can be promoted by copying it over the master by hand.
         * Without the env var the master opens read-only and FE_WRIT is a
         * fake-success no-op (historic behavior). */
        static int rw = -1;
        if (rw < 0) { const char* e = getenv("ND500X_DISK_RW"); rw = (e && e[0] && e[0] != '0') ? 1 : 0; }
        g_disk_rw = rw;
        if (rw) {
            static char spath[1100];
            snprintf(spath, sizeof(spath), "%s.session", disk_path);
            FILE* src = fopen(disk_path, "rb");
            FILE* dst = src ? fopen(spath, "wb") : NULL;
            if (src && dst) {
                char buf[65536]; size_t n;
                while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
            }
            if (src) fclose(src);
            if (dst) fclose(dst);
            g_disk = dst ? fopen(spath, "r+b") : NULL;
            if (fedbg() || g_disk)
                fprintf(stderr, "[FECALL] COW session: %s -> %s (%s)\n",
                        disk_path, spath, g_disk ? "writable" : "FAILED - no disk");
        } else {
            g_disk = fopen(disk_path, "rb");
        }
        if (g_disk) { fseek(g_disk, 0, SEEK_END); g_disk_size = ftell(g_disk); fseek(g_disk, 0, SEEK_SET); }
        if (fedbg())
            fprintf(stderr, "[FECALL] disk image: %s (%ld bytes)\n", disk_path, g_disk ? g_disk_size : -1L);
    }

    uint32_t device  = nd500_read_memory_32(cpu, arg_addresses[0]);
    uint32_t request = nd500_read_memory_32(cpu, arg_addresses[1]);
    uint32_t rpk_arg = nd500_read_memory_32(cpu, arg_addresses[2]);
    uint32_t cpk_arg = nd500_read_memory_32(cpu, arg_addresses[3]);

    uint32_t gen = (device >> 16) & 0xFFFF;
    uint32_t req = request & 0xFFFF;

    if (fedbg())
        fprintf(stderr, "[FECALL] device=0x%08X (gen=%u) request=0x%08X (req=%u) rpk=0x%08X cpk=0x%08X\n",
                device, gen, request, req, rpk_arg, cpk_arg);

    /* SIINTR tracing (env ND500X_SIDBG) - generic 8 only, so it can be left on
     * without drowning in disk and console traffic.
     *
     * SIINTR is NOT related to SINTRAN despite the name: it is the 4.3BSD
     * "software interrupt" (netisr) channel. if/si.c is headed "software
     * interrupt handler", and siintr() clears NETISR_RAW / NETISR_IP / NETISR_NS
     * and dispatches rawintr() / ipintr() / nsintr(). It is how the kernel defers
     * network protocol processing out of the hardware interrupt, by asking the
     * ND-100 front end to interrupt it back at a lower priority.
     *
     * Only two requests ever arrive here:
     *   FE_IDEV, QF_SYNC  - siattach (if/si.c:133), command packet carries
     *                       ipl = IPL_SI (3, machine/icb.h:87)
     *   FE_DCTL, QF_ASYNC - setsoftnet/schednetisr (machine/basic.c:38-51),
     *                       command packet is _dctl_cpk_si = { int dummy; },
     *                       i.e. no payload - the request IS the signal
     * Anything else on generic 8 is unexpected and worth seeing. */
    if (gen == GEN_SIINTR) {
        static int sidbg = -1;
        if (sidbg < 0) { const char* e = getenv("ND500X_SIDBG"); sidbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (sidbg) {
            uint32_t qual = (request >> 16) & 0xFFFF;
            const char* rname = (req == FE_IDEV) ? "FE_IDEV" :
                                (req == FE_DCTL) ? "FE_DCTL" : "OTHER";
            Pkt cpk = pkt_word(cpu, cpk_arg);
            fprintf(stderr,
                "[SIDBG] gen=8 SIINTR %s(%u) qual=%u(%s) sub=%u cpk=0x%08X cpk[0..3]=%04X %04X "
                "rpk=0x%08X PC=0x%08X CED=%u instr=%llu\n",
                rname, req, qual, (qual == 1) ? "QF_SYNC" : "QF_ASYNC",
                device & 0xFFFF, cpk_arg, pkt_rd16(&cpk, 0), pkt_rd16(&cpk, 2),
                rpk_arg, cpu->PC, cpu->CED,
                (unsigned long long)cpu->instruction_count);
        }
    }

    switch (req) {
        case FE_INIT: {
            /* _feinit_fecall passes LOGICAL packet addresses (private unknown). */
            Pkt rpk = pkt_logical(cpu, rpk_arg);
            fe_init(cpu, &rpk);
            break;
        }
        case FE_IDEV: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            Pkt cpk = pkt_word(cpu, cpk_arg);
            fe_idev(cpu, gen, &cpk, &rpk);
            break;
        }
        case FE_OPEN: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            if (gen == GEN_DISK) {
                fe_open_disk(&rpk);      /* disk: return geometry */
            } else if (gen == GEN_TAPE) {
                /* _open_rpk_tape (if.h:191): completion@0, status@2 - no
                 * geometry, a tape has none. Deliberately does NOT rewind:
                 * BSD distinguishes rewind from no-rewind tape devices by
                 * minor number, and the driver issues DCTL_REW explicitly when
                 * it wants BOT. A fresh attach already starts at BOT. */
                int ok = (fe_tape_open() != NULL);
                pkt_wr16(&rpk, 0, ok ? 0 : 1);
                pkt_wr16(&rpk, 2, 0);
                if (fedbg())
                    fprintf(stderr, "[FECALL] FE_OPEN tape -> %s (pos=%ld)\n",
                            ok ? "success" : "no medium", g_tape.pos);
            } else {
                pkt_wr16(&rpk, 0, 0);    /* other devices (e.g. XMSG): success */
                if (fedbg()) fprintf(stderr, "[FECALL] FE_OPEN gen=%u -> success (non-disk)\n", gen);
            }
            break;
        }
        case FE_CLOS: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            pkt_wr16(&rpk, 0, 0);
            break;
        }
        case FE_WCON: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            /* wcon_cpk: physaddr@0 */
            Pkt cpk = pkt_word(cpu, cpk_arg);
            uint32_t physaddr = pkt_rd32(&cpk, 0);
            fe_wcon(cpu, physaddr, &rpk);
            break;
        }
        case FE_RCON: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            Pkt cpk = pkt_word(cpu, cpk_arg);
            uint32_t physaddr = pkt_rd32(&cpk, 0);
            fe_rcon(cpu, physaddr, &rpk);
            break;
        }
        case FE_READ: {
            if (gen == 3 /* TERM_IN */) {
                /* Console INPUT poll (io/mx.c mxopen tail: the one outstanding
                 * async FE_READ; cpk = read_cpk_term {dummy@0, physaddr@4 =
                 * ND-100 word addr of the mx_bin input ring}). It completes
                 * only when console input ARRIVES - there is none yet, so
                 * leave the request outstanding: no rpk write, no interrupt.
                 * Treating this as a DISK read DMA'd disk sectors into the tty
                 * ring and caused a completion-interrupt storm.
                 * Record where mx_bin lives so fe_conq_drain() can deposit
                 * host stdin bytes (see console INPUT block above). */
                Pkt cpk = pkt_word(cpu, cpk_arg);
                uint32_t waddr = pkt_rd32(&cpk, 4);
                g_termin_ring = waddr * 2u - FE_PRIVATE;
                if (fedbg())
                    fprintf(stderr, "[FECALL] FE_READ TERM_IN sub=%u -> held pending; mx_bin ring @phys 0x%08X\n",
                            device & 0xFFFF, g_termin_ring);
                break;
            }
            Pkt rpk = pkt_word(cpu, rpk_arg);
            if (gen == GEN_TAPE) {
                fe_read_tape(cpu, cpk_arg, &rpk);
                cpu->fe_int_pending = 1;      /* async, like the disk path */
                cpu->fe_int_gen = gen;
                cpu->fe_int_sub = device & 0xFFFF;
                cpu->fe_int_rpk = rpk_arg;
                break;
            }
            fe_read_disk(cpu, device, cpk_arg, rpk_arg, &rpk);
            break;
        }
        case FE_WRIT: {
            if (gen == 4 /* TERM_OUT */) {
                /* Console OUTPUT (io/mx.c mxstart): writ_cpk_term {nbytes@0,
                 * physaddr@4 = ND-100 word addr of the output bytes};
                 * writ_rpk_term {completion@0}. Copy the bytes to stdout and
                 * deliver the async completion interrupt (gen 4 -> mxxintr
                 * wakes the writer / sends the next chunk). */
                Pkt cpk = pkt_word(cpu, cpk_arg);
                uint32_t nbytes = pkt_rd32(&cpk, 0);
                uint32_t waddr  = pkt_rd32(&cpk, 4);
                uint32_t phys   = waddr * 2u - FE_PRIVATE;
                uint32_t i;
                if (nbytes > 4096u) nbytes = 4096u;   /* sanity clamp */
                int strip = !fe_console_8bit();
                unsigned char obuf[4096];
                for (i = 0; i < nbytes; i++) {
                    uint8_t ch = nd500_bus_read8(cpu->machine, phys + i);
                    if (strip) ch &= 0x7F;   /* the UART consumes the parity bit */
                    obuf[i] = ch;
                }
                /* One emit point for stdout AND any host sink, so a telnet
                 * terminal sees exactly the masked bytes stdout sees. */
                fe_tty_out((int)(device & 0xFFFF), obuf, (int)nbytes);
                Pkt rpk = pkt_word(cpu, rpk_arg);
                pkt_wr16(&rpk, 0, 0);   /* completion = success */
                if (fedbg())
                {
                    char hex[3*48+1]; unsigned hn = 0, k;
                    for (k = 0; k < nbytes && k < 48; k++)
                        hn += (unsigned)snprintf(hex+hn, sizeof(hex)-hn, "%02X ",
                                                 nd500_bus_read8(cpu->machine, phys + k));
                    hex[hn] = 0;
                    fprintf(stderr, "[FECALL] FE_WRIT TERM_OUT unit=%u strip=%d nbytes=%u phys=0x%08X raw=[%s]\n",
                            device & 0xFFFF, strip, nbytes, phys, hex);
                }
                cpu->fe_int_pending = 1;
                cpu->fe_int_gen = gen;
                cpu->fe_int_sub = device & 0xFFFF;
                cpu->fe_int_rpk = rpk_arg;
                break;
            }
            if (gen == GEN_TAPE) {
                /* Read-only backing store (tracked scope). Report the failure
                 * rather than fake success - see fe_dctl_tape's WEOF. */
                Pkt rpk = pkt_word(cpu, rpk_arg);
                pkt_wr16(&rpk, 0, 1);   /* completion = error */
                pkt_wr16(&rpk, 2, 0);
                if (fedbg())
                    fprintf(stderr, "[FECALL] FE_WRIT tape refused (image is read-only)\n");
                cpu->fe_int_pending = 1;
                cpu->fe_int_gen = gen;
                cpu->fe_int_sub = device & 0xFFFF;
                cpu->fe_int_rpk = rpk_arg;
                break;
            }
            /* writ_cpk_disk {nbytes@0, physaddr@4, devaddr@8}; writ_rpk {completion@0,
             * status@2}. Async. With ND500X_DISK_RW (COW session, opened r+b) the
             * sector data is DMA'd from ND-500 memory into the session image -
             * the mirror of fe_read_disk. Without it: fake success, image
             * untouched (historic behavior). Deliver the completion interrupt on
             * the disk's gen like FE_READ. */
            Pkt cpk = pkt_word(cpu, cpk_arg);
            uint32_t nbytes   = pkt_rd32(&cpk, 0);
            uint32_t physaddr = pkt_rd32(&cpk, 4);   /* ND-100 word addr */
            uint32_t devaddr  = pkt_rd32(&cpk, 8);   /* sector index */
            Pkt rpk = pkt_word(cpu, rpk_arg);
            uint16_t completion = 0;
            if (g_disk_rw && g_disk) {
                uint32_t src_phys = physaddr * 2u - FE_PRIVATE;
                long img_off = (long)devaddr * (long)g_ssize;
                if (img_off < 0 || img_off + (long)nbytes > g_disk_size) {
                    completion = 1;   /* out of range */
                } else {
                    uint32_t i;
                    fseek(g_disk, img_off, SEEK_SET);
                    for (i = 0; i < nbytes; i++)
                        fputc((int)nd500_bus_read8(cpu->machine, src_phys + i), g_disk);
                    fflush(g_disk);
                }
                if (fedbg())
                    fprintf(stderr, "[FECALL] FE_WRIT nbytes=%u devaddr=%u img_off=0x%lX src_phys=0x%08X -> session (compl=%u)\n",
                            nbytes, devaddr, (long)devaddr * (long)g_ssize, physaddr * 2u - FE_PRIVATE, completion);
            } else if (fedbg()) {
                fprintf(stderr, "[FECALL] FE_WRIT nbytes=%u devaddr=%u -> success (no-op, image unmodified)\n",
                        nbytes, devaddr);
            }
            pkt_wr16(&rpk, 0, completion);
            pkt_wr16(&rpk, 2, 0);   /* status */
            cpu->fe_int_pending = 1;
            cpu->fe_int_gen = (device >> 16) & 0xFFFF;
            cpu->fe_int_sub = device & 0xFFFF;
            cpu->fe_int_rpk = rpk_arg;
            break;
        }
        case FE_DCTL: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            if (gen == GEN_TAPE) {
                fe_dctl_tape(cpu, cpk_arg, &rpk);
                cpu->fe_int_pending = 1;
                cpu->fe_int_gen = gen;
                cpu->fe_int_sub = device & 0xFFFF;
                cpu->fe_int_rpk = rpk_arg;
                break;
            }
            pkt_wr16(&rpk, 0, 0);   /* completion */
            pkt_wr16(&rpk, 2, 0);   /* status */
            /* Terminal DCTL_CHG_FLGS reports the line mode (machine/if.h
             * dctl_cpk_term: operation@0, parameter@2, both BE16). Logged only -
             * console output masking is NOT driven from it, see fe_console_8bit(). */
            if (fedbg() && (gen == 3 /* TERM_IN */ || gen == 4 /* TERM_OUT */)) {
                Pkt dcpk = pkt_word(cpu, cpk_arg);
                uint16_t operation = pkt_rd16(&dcpk, 0);
                uint16_t parameter = pkt_rd16(&dcpk, 2);
                if (operation == 3 /* DCTL_CHG_FLGS */)
                    fprintf(stderr, "[FECALL] FE_DCTL CHG_FLGS gen=%u unit=%u param=0x%04X\n",
                            gen, device & 0xFFFF, parameter);
            }
            /* SYNC calls (qualifier 1, e.g. mxparam's terminal DCTL) must NOT
             * get a completion interrupt - the kernel does not sleep on them.
             * Async DCTL queues a completion interrupt like FE_READ. */
            if (((request >> 16) & 0xFFFF) != 1 /* !QF_SYNC */) {
                cpu->fe_int_pending = 1;
                cpu->fe_int_gen = gen;
                cpu->fe_int_sub = device & 0xFFFF;
                cpu->fe_int_rpk = rpk_arg;
            }
            break;
        }
        case FE_EXIT: {
            if (fedbg()) fprintf(stderr, "[FECALL] FE_EXIT (shutdown)\n");
            break;
        }
        case FE_ERRM: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            pkt_wr16(&rpk, 0, 0);
            break;
        }
        default:
            if (fedbg()) fprintf(stderr, "[FECALL] unhandled req=%u\n", req);
            break;
    }
    return 0;
}
