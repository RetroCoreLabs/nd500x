/*
 * nd500_settings.h - every knob the emulator has, as plain fields.
 *
 * WHY THIS EXISTS
 * ---------------
 * These settings used to be 108 getenv() calls scattered across src/cpu and
 * src/machine - cpu.c alone had 27 - each with its own latch and its own bit of
 * string parsing. Three problems with that:
 *
 *   1. A WASM build has no environment, so none of it could be configured in a
 *      browser. The flags were not merely off: they were unreachable.
 *   2. Nothing said what the knobs even ARE. Finding them meant grepping for
 *      getenv across two dozen files.
 *   3. Every read was a string lookup plus a hand-rolled test, and the tests
 *      were not consistent: most were "set and not 0", four were INVERTED
 *      (the variable named the thing being switched off), and five accepted
 *      only a literal "1".
 *
 * So the emulator does not read the environment. It reads THIS STRUCT. A host
 * fills it in: the native frontend from getenv (nd500_settings_load_env, which
 * keeps every variable name and every parsing quirk exactly as it was, so
 * run-ndix.sh and all existing scripts still behave identically), a browser
 * build from JS, and eventually the machine-configuration UI from a form - the
 * fields map onto it one for one.
 *
 * THE INVERTED ONES ARE STATED THE POSITIVE WAY ROUND. ND500X_NOTLB becomes
 * tlb_enabled (default 1), ND500X_RAW_BS becomes translate_bs (default 1), and
 * so on. The inversion lives in the loader and nowhere else, which is the whole
 * point: a host filling this struct should not have to know which historical
 * variable happened to be named after the negative.
 *
 * Sibling: nd500_host.h, which is what the ND-500 needs DONE for it (block
 * storage). This file is what it needs TOLD.
 */

#ifndef ND500_SETTINGS_H
#define ND500_SETTINGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nd500Settings {

    /* ---- Diagnostics - all default off ------------------------------------- */
    int allocdbg;          /* ND500X_ALLOCDBG             segment allocator bitmap */
    int bpdbg;             /* ND500X_BPDBG                BP instruction */
    int byconvdbg;         /* ND500X_BYCONVDBG            BYCONV conversions */
    int caddbg;            /* ND500X_CADDBG               CAD register writes */
    int carve_bout;        /* ND500X_CARVE_BOUT           WFILE frame-chain carve (MON path) */
    int chaindbg;          /* ND500X_CHAINDBG             CHAIN instruction */
    int dcodedbg;          /* ND500X_DCODEDBG             data-descriptor decode */
    int dit1dbg;           /* ND500X_DIT1DBG              DIT entry 1 */
    int ditdbg;            /* ND500X_DITDBG               domain information table */
    int domdbg;            /* ND500X_DOMDBG               domain switches */
    int dropdbg;           /* ND500X_DROPDBG              dropped writes */
    int exedbg;            /* ND500X_EXEDBG               exec path */
    int fcmpdbg;           /* ND500X_FCMPDBG              float compare */
    int fedbg;             /* ND500X_FEDBG                every fecall in and out */
    int framelog;          /* ND500X_FRAMELOG             stack frame set-up and tear-down */
    int gatedbg;           /* ND500X_GATEDBG              interrupt gate */
    int heapdbg;           /* ND500X_HEAPDBG              guest heap (ALLOB/FREEB) */
    int inddbg;            /* ND500X_INDDBG               cross-domain CALL */
    int initlog;           /* ND500X_INITLOG              INIT instruction */
    int inodedbg;          /* ND500X_INODEDBG             root inode on the FS block read */
    int iocdbg;            /* ND500X_IOCDBG               I/O control */
    int ipcurdbg;          /* ND500X_IPCURDBG             ip_cur cell at 0x30001004 */
    int keylog;            /* ND500X_KEYLOG               every key into the guest */
    int ksudbg;            /* ND500X_KSUDBG               kernel/user switch */
    int l2watch;           /* ND500X_L2WATCH              level-2 page table */
    int nc_wwatch;         /* ND500X_NC_WWATCH            NC word writes */
    int no_invalid00;      /* ND500X_NO_INVALID00         do not trap opcode 0x00 */
    int pageindbg;         /* ND500X_PAGEINDBG            page-in */
    int pathdbg;           /* ND500X_PATHDBG              0xF0000000 path window */
    int pgfdbg;            /* ND500X_PGFDBG               page faults */
    int pgudbg;            /* ND500X_PGUDBG               PGU/WIP page bitmaps */
    int physdbg;           /* ND500X_PHYSDBG              physical allocator report */
    int privdbg;           /* ND500X_PRIVDBG              privileged-instruction refusals */
    int prtdbg;            /* ND500X_PRTDBG               protection violations */
    int pst47dbg;          /* ND500X_PST47DBG             PST entry 47 */
    int ptwdbg;            /* ND500X_PTWDBG               page-table walk */
    int seg30dbg;          /* ND500X_SEG30DBG             segment 30 accesses */
    int seg_dump;          /* ND500X_SEG_DUMP             segment table dump */
    int sidbg;             /* ND500X_SIDBG                software interrupts */
    int slashdbg;          /* ND500X_SLASHDBG             slash (descriptor) operands */
    int slpdbg;            /* ND500X_SLPDBG               sleep/wakeup */
    int solodbg;           /* ND500X_SOLODBG              SOLO instruction */
    int stkdbg;            /* ND500X_STKDBG               stack frames (ENTS) */
    int stodbg;            /* ND500X_STODBG               stack overflow */
    int stopdbg;           /* ND500X_STOPDBG              PC ring dump when the machine stops */
    int syscno;            /* ND500X_SYSCNO               system call numbers */
    int thadbg;            /* ND500X_THADBG               trap handler address */
    int tickstat;          /* ND500X_TICKSTAT             clock tick accounting */
    int tlbstat;           /* ND500X_TLBSTAT              translation cache hit/miss at exit */
    int traplog;           /* ND500X_TRAPLOG              every trap */
    int tsbdbg;            /* ND500X_TSBDBG               PCTSB/DCTSB */
    int tsetdbg;           /* ND500X_TSETDBG              TSET in the 0x83A..0x852 window */
    int ttydbg;            /* ND500X_TTYDBG               terminal traffic */
    int udatadbg;          /* ND500X_UDATADBG             u-area data accesses */
    int umuldbg;           /* ND500X_UMULDBG              UMUL overflow */
    int uptwdbg;           /* ND500X_UPTWDBG              user page-table walk */
    int uwatch;            /* ND500X_UWATCH               u-area writes */
    int nc_typetag_guard;  /* ND500X_NC_TYPETAG_GUARD     NC type-tag guard */
    int bootdbg;           /* ND500X_BOOTDBG              kernel-domain boot path */
    int d1dbg;             /* ND500X_D1DBG                domain 1, PC <= 0x40 */
    int fuwdbg;            /* ND500X_FUWDBG               fuword/suword window at 0xF0000000 */
    int hdlrtrace;         /* ND500X_HDLRTRACE            inside the guest trap handler */
    int icodedbg;          /* ND500X_ICODEDBG             instruction decode, user domains */
    int initdbg;           /* ND500X_INITDBG              domain 1 init, PC < 0x40 */
    int pcsample;          /* ND500X_PCSAMPLE             PC sampling */
    int piadbg;            /* ND500X_PIADBG               PIA privileged gate */
    int swtchdbg;          /* ND500X_SWTCHDBG             context switch at PC 0x844 */
    int sysdbg;            /* ND500X_SYSDBG               kernel-domain system calls */

    /* ---- Behaviour --------------------------------------------------------- */
    int noxmsg;            /* ND500X_NOXMSG               fail XMSG (generic 7) device init so xgattach gives up cleanly */
    uint32_t sysno;        /* ND500X_SYSNO                ND system number for the high half of the XMSG magic number (XFMST). Properly the ND-100's - the pair share one - so this is an override until there is an ND-100 to ask. Default 500. */
    const char* eth_uplink; /* ND500X_ETH_UPLINK          where et0's frames go. NULL/"none" = dropped; "loop" = echoed back; "listen[:port]" = wait for a peer; "tcp:host[:port]" = dial one */
    int nofeclock;         /* ND500X_NOFECLOCK            suppress the front-end 50 Hz clock */
    int nouserclock;       /* ND500X_NOUSERCLOCK          suppress the user-visible clock tick */
    int console_8bit;      /* ND500X_CONSOLE_8BIT         pass console bytes with all 8 bits */
    int mmu_guest_tables;  /* ND500X_MMU_GUEST_TABLES     translate through the GUEST's own DIT/PST (the NDIX regime) */
    int translate_bs;      /* ND500X_RAW_BS               translate host backspace to the guest erase char. ND500X_RAW_BS=1 turns this OFF. */
    int tlb_enabled;       /* ND500X_NOTLB                use the translation cache. ND500X_NOTLB=1 turns this OFF. */
    int demand_segments;   /* ND500X_NO_DEMAND_SEGMENTS   demand-map kernel-domain segments. ND500X_NO_DEMAND_SEGMENTS=1 turns this OFF. */
    int trap_dispatch;     /* ND500X_NO_TRAP_DISPATCH     dispatch traps to the guest handler. ND500X_NO_TRAP_DISPATCH=1 turns this OFF. */

    /* ---- Memory and layout ------------------------------------------------- */
    uint32_t memtop;            /* ND500X_MEMTOP               report less memory to NDIX than exists, to exercise paging. 0 = report it all. */
    uint32_t sfree;             /* ND500X_SFREE                first free ND-500-relative physical byte; also the allocator's floor. 0 = built-in. */

    /* ---- Media ------------------------------------------------------------- */
    const char* tape_path;         /* ND500X_TAPE                 tape image, NULL = none */
    const char* disk_path;         /* ND500X_DISK                 root disk image */

    /* ---- Watchpoints - 0/absent means unset -------------------------------- */
    int      ptewatch;          /* ND500X_PTEWATCH             log PTE writes */
    uint32_t ptewatch_page;     /* ND500X_PTEWATCH_PAGE        first watched page */
    uint32_t ptewatch_page2;    /* ND500X_PTEWATCH_PAGE2       second watched page */
    uint32_t ptewatch_pfn;      /* ND500X_PTEWATCH_PFN         watched page frame */
    long     ptecatch;          /* ND500X_PTECATCH             physical address to catch, -1 = off */
    uint32_t framewatch;        /* ND500X_FRAMEWATCH           hex phys base of a 2 KB frame window */
    uint32_t pwatch_base;       /* ND500X_PWATCH_BASE          from ND500X_PWATCH=<hex>[:<len>] */
    uint32_t pwatch_len;        /* ND500X_PWATCH_LEN           from ND500X_PWATCH; 0 = off */
    long     pguwatch;          /* ND500X_PGUWATCH             page number to watch, -1 = off */
    uint32_t ptdbg_target;      /* ND500X_PTDBG                hex data address to trace */

    /* ---- Debugger presentation --------------------------------------------- */
    int show_ea;           /* ND500X_SHOW_EA              show effective addresses; only "1" counts */
    int show_hex;          /* ND500X_SHOW_HEX             hex operands; default ON, "0" turns it off */
    int demangle;          /* ND500X_DEMANGLE             strip a leading underscore from symbols */
    int trace;             /* ND500X_TRACE                instruction trace; only "1" counts */
    int profile;           /* ND500X_PROFILE              instruction profiling; only "1" counts */
    int trap_invalid;      /* ND500X_TRAP_INVALID         trap invalid opcodes. ALWAYS 1 - the variable has never had an effect. */

    /* disk_mode: 0 = read-only, 1 = write through to the image, 2 = copy-on-
     * write session (the master image is left untouched). From ND500X_DISK_RW:
     * unset or "1" -> 1, "cow" -> 2, "0" -> 0. */
    int disk_mode;

} Nd500Settings;

/*
 * The settings. Never NULL. On first use the defaults are installed and, where
 * there is an environment, it is read once - so every existing script and test
 * keeps working with no change anywhere.
 */
Nd500Settings* nd500_settings(void);

/* Reset to documented defaults. Does NOT read the environment: this is what a
 * host calls when it means to fill the struct itself and wants nothing ambient
 * leaking in - a browser build, or a test that must be hermetic. */
void nd500_settings_defaults(Nd500Settings* s);

/* Fill from the environment using the variable names in the comments above.
 * Safe to call more than once. A no-op where there is no environment. */
void nd500_settings_load_env(void);

#ifdef __cplusplus
}
#endif

#endif /* ND500_SETTINGS_H */
