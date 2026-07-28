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

static const char* FE_DISK_PATH = "/mnt/e/Dev/Ronny/NDIX-C/rootfs.img";

static FILE*    g_disk = NULL;
static long     g_disk_size = 0;
static uint32_t g_ssize = FE_SECSIZE;   /* sector size, confirmed by FE_OPEN */

static int fedbg(void) {
    static int v = -1;
    if (v < 0) { const char* e = getenv("ND500X_FEDBG"); v = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return v;
}

/* ---- packet field access: logical (FE_INIT) or physical (ND-100 word) ---- */
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

    /* Physical layout (bytes): kernel image + emulator PST(0x84000)/DIT(0x90000)
     * live below sfree; free RAM from 1 MB to the top of memory. */
    uint32_t scont_phys = 0x00000000u;   /* first physical addr NDIX uses */
    uint32_t sfree_phys = 0x00100000u;   /* start of free RAM (past image+tables) */
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
static void fe_idev(uint32_t gen, Pkt* rpk) {
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
    pkt_wr16(rpk, ID_RPK_COMPLETION, 0);
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
     * blocks anyway. secsiz must be 1024 (dist[1].ssize) so devaddr = daddr. */
    pkt_wr16(rpk, OP_RPK_COMPLETION, 0);
    pkt_wr32(rpk, OP_RPK_DEVSIZ, 1);            /* dist[1] = di70 */
    pkt_wr32(rpk, OP_RPK_FRMSIZ, 69530);       /* di70 dsize (1K blocks) */
    pkt_wr16(rpk, OP_RPK_SECSIZ, 1024);
    g_ssize = 1024;                             /* match dist[1].ssize for FE_READ */
    if (fedbg()) fprintf(stderr, "[FECALL] FE_OPEN disk: devsiz=1(di70) secsiz=1024\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FE_WCON / FE_RCON - synchronous console I/O. The physaddr points at ONE byte.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void fe_wcon(Nd500Cpu* cpu, uint32_t physaddr_word, Pkt* rpk) {
    /* The char lives in the global `cout = (long)(*cp)` (basic.c:fewcon): a
     * 32-bit big-endian long whose low byte (phys+3) holds the character. */
    uint32_t phys = physaddr_word * 2u - FE_PRIVATE;
    uint8_t ch = nd500_bus_read8(cpu->machine, phys + 3);
    putchar((int)ch);
    fflush(stdout);
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
    nd500_bus_write32(cpu->machine, cx_phys + 16*4, cpu->ST1);  /* 17 ST1 */
    nd500_bus_write32(cpu->machine, cx_phys + 17*4, cpu->ST2);  /* 18 ST2 */
    nd500_bus_write32(cpu->machine, cx_phys + 22*4, cpu->THA);  /* 23 THA */
    nd500_bus_write32(cpu->machine, cx_phys + 23*4, cpu->CED);  /* 24 CED */
    nd500_bus_write32(cpu->machine, cx_phys + 24*4, cpu->CAD);  /* 25 CAD */

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
static char g_conq[4096];
static int  g_conq_head = 0, g_conq_tail = 0;
static pthread_mutex_t g_conq_mtx = PTHREAD_MUTEX_INITIALIZER;

void nd500_fecall_console_input(const char* buf, int len) {
    int i;
    pthread_mutex_lock(&g_conq_mtx);
    for (i = 0; i < len; i++) {
        int nt = (g_conq_tail + 1) % (int)sizeof(g_conq);
        if (nt == g_conq_head) break;          /* host queue full - drop rest */
        g_conq[g_conq_tail] = buf[i];
        g_conq_tail = nt;
    }
    g_conq_avail = (g_conq_tail != g_conq_head);
    pthread_mutex_unlock(&g_conq_mtx);
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
        uint16_t el = (uint16_t)((FE_CONDEV << 8) | (uint8_t)g_conq[g_conq_head]);
        nd500_bus_write16(m, g_termin_ring + 12u + 2u * bfree, el);
        bfree = nf;
        g_conq_head = (g_conq_head + 1) % (int)sizeof(g_conq);
    }
    nd500_bus_write16(m, g_termin_ring + 8, bfree);
    g_conq_avail = (g_conq_tail != g_conq_head);
    pthread_mutex_unlock(&g_conq_mtx);
}

/* Called every instruction from cpu_step. Delivers, at the kernel base level
 * (spl0, idle/biowait), either a pending disk/dctl completion interrupt or a
 * periodic clock tick (bumping the ND-100 tick count first) so hardclock() runs
 * and drives timekeeping + the scheduler. */
void nd500_fecall_tick(Nd500Cpu* cpu) {
    if (!cpu) return;
    /* Front-end "DMA" of queued console input into the mx_bin ring: pure
     * physical-memory writes, safe at any instruction boundary (the real
     * ND-100 wrote this ring concurrently with ND-500 execution). */
    if (g_conq_avail && g_termin_ring) fe_conq_drain(cpu);
    if (cpu->CED != 0) return;
    /* in_trap_handler alone must NOT block delivery: NDIX sleeps INSIDE trap
     * context (pagein -> biowait -> swtch to idle) and spins at 0x844 with the
     * emulator's in_trap_handler still set - the trap only "returns" (lregbl)
     * after the sleeper is woken BY this very interrupt. The PC==0x844 idle-
     * spin gate below already guarantees the safe boundary; blocking on
     * in_trap_handler starved the exec text-read completion forever. */
    if (cpu->in_trap_handler && cpu->PC != 0x00000844u) return;
    /* Periodic clock can be disabled for isolation (ND500X_NOFECLOCK=1). */
    static int noclk = -1;
    if (noclk < 0) { const char* e = getenv("ND500X_NOFECLOCK"); noclk = (e && e[0] && e[0] != '0') ? 1 : 0; }
    int clock_due = !noclk && cpu->instruction_count && (cpu->instruction_count % FE_CLOCK_PERIOD) == 0;
    if (!cpu->fe_int_pending && !clock_due) return;     /* fast path */

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
        fe_deliver(cpu, iplrec, ip_cur, shseg,
                   cpu->fe_int_gen, cpu->fe_int_sub, cpu->fe_int_rpk, FE_IPL_DK);
        cpu->fe_int_pending = 0;
        return;
    }
    /* Periodic clock: advance the ND-100 tick count, then interrupt hardclock. */
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
        if (!disk_path || !disk_path[0]) disk_path = FE_DISK_PATH;
        g_disk = fopen(disk_path, "rb");
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

    switch (req) {
        case FE_INIT: {
            /* _feinit_fecall passes LOGICAL packet addresses (private unknown). */
            Pkt rpk = pkt_logical(cpu, rpk_arg);
            fe_init(cpu, &rpk);
            break;
        }
        case FE_IDEV: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            fe_idev(gen, &rpk);
            break;
        }
        case FE_OPEN: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            if (gen == GEN_DISK) {
                fe_open_disk(&rpk);      /* disk: return geometry */
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
                for (i = 0; i < nbytes; i++)
                    putchar((int)nd500_bus_read8(cpu->machine, phys + i));
                fflush(stdout);
                Pkt rpk = pkt_word(cpu, rpk_arg);
                pkt_wr16(&rpk, 0, 0);   /* completion = success */
                if (fedbg())
                    fprintf(stderr, "[FECALL] FE_WRIT TERM_OUT nbytes=%u phys=0x%08X -> stdout\n",
                            nbytes, phys);
                cpu->fe_int_pending = 1;
                cpu->fe_int_gen = gen;
                cpu->fe_int_sub = device & 0xFFFF;
                cpu->fe_int_rpk = rpk_arg;
                break;
            }
            /* writ_cpk_disk {nbytes@0, physaddr@4, devaddr@8}; writ_rpk {completion@0,
             * status@2}. Async. NON-DESTRUCTIVE for now: complete successfully (so
             * the mount proceeds) WITHOUT writing rootfs.img. Deliver the completion
             * interrupt on the disk's gen like FE_READ. */
            Pkt cpk = pkt_word(cpu, cpk_arg);
            uint32_t nbytes  = pkt_rd32(&cpk, 0);
            uint32_t devaddr = pkt_rd32(&cpk, 8);
            Pkt rpk = pkt_word(cpu, rpk_arg);
            pkt_wr16(&rpk, 0, 0);   /* completion = success */
            pkt_wr16(&rpk, 2, 0);   /* status */
            if (fedbg())
                fprintf(stderr, "[FECALL] FE_WRIT nbytes=%u devaddr=%u -> success (no-op, image unmodified)\n",
                        nbytes, devaddr);
            cpu->fe_int_pending = 1;
            cpu->fe_int_gen = (device >> 16) & 0xFFFF;
            cpu->fe_int_sub = device & 0xFFFF;
            cpu->fe_int_rpk = rpk_arg;
            break;
        }
        case FE_DCTL: {
            Pkt rpk = pkt_word(cpu, rpk_arg);
            pkt_wr16(&rpk, 0, 0);   /* completion */
            pkt_wr16(&rpk, 2, 0);   /* status */
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
