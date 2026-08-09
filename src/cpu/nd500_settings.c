/*
 * nd500_settings.c - the settings struct, its defaults, and the environment
 * loader that keeps every existing script working.
 *
 * Generated shape, hand-maintained: see nd500_settings.h for what this is and
 * why nothing else in src/cpu or src/machine calls getenv any more.
 *
 * The parse helpers below are not interchangeable. They exist because the code
 * this replaced was not consistent, and reproducing each site EXACTLY was worth
 * more than tidying the behaviour on the way past:
 *
 *   env_flag                 set and not "0"          - the common case
 *   env_is_one               only a literal "1"       - the debugger's flags
 *   env_not_zero_default_on  default ON, "0" is off   - ND500X_SHOW_HEX
 *   env_u32 / env_u32_base   a number, 0 when unset or unparseable
 *   env_long_base            a number with an explicit "unset" value
 */

#include "nd500_settings.h"

#include <stdlib.h>
#include <string.h>

static Nd500Settings s_settings;
static int           s_ready = 0;

/* "set and not 0" - the test almost every old getenv site used. */
static int env_flag(const char* name) {
    const char* e = getenv(name);
    return (e && e[0] && e[0] != '0') ? 1 : 0;
}

/* Only a literal "1". The debugger's own flags were written this way, so
 * ND500X_TRACE=yes never enabled tracing - preserved rather than "fixed",
 * because scripts may rely on ND500X_TRACE=0 and ND500X_TRACE=2 being off. */
static int env_is_one(const char* name) {
    const char* e = getenv(name);
    return (e && strcmp(e, "1") == 0) ? 1 : 0;
}

/* Default ON; only an explicit leading '0' turns it off. */
static int env_not_zero_default_on(const char* name) {
    const char* e = getenv(name);
    return (e && e[0] == '0') ? 0 : 1;
}

/* A positive number, or 0 for "unset or unparseable". Deliberately strict: a
 * partly-numeric value is a typo, and silently using the prefix of a typo is
 * how you get a kernel with no page frames. */
static uint32_t env_u32(const char* name) {
    const char* e = getenv(name);
    if (!e || !e[0]) return 0;
    char* end = NULL;
    long v = strtol(e, &end, 0);
    if (!end || *end != '\0' || v <= 0) return 0;
    return (uint32_t)v;
}

/* Same, in an explicit base, and lenient about trailing text - the watchpoint
 * knobs were always parsed with a bare strtoul. */
static uint32_t env_u32_base(const char* name, int base) {
    const char* e = getenv(name);
    if (!e || !e[0]) return 0;
    return (uint32_t)strtoul(e, NULL, base);
}

static long env_long_base(const char* name, int base, long unset) {
    const char* e = getenv(name);
    if (!e || !e[0]) return unset;
    return (long)strtoul(e, NULL, base);
}

void nd500_settings_defaults(Nd500Settings* s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    /* Everything not listed here defaults to 0. These are the fields whose
     * documented default is NOT zero - the four inverted flags, ND500X_SHOW_HEX,
     * and the watchpoints whose "off" is -1. */
    s->translate_bs       = 1;
    s->tlb_enabled        = 1;
    s->demand_segments    = 1;
    s->trap_dispatch      = 1;
    s->ptecatch           = -1;
    s->pguwatch           = -1;
    s->show_hex           = 1;
    s->trap_invalid       = 1;
    s->disk_mode = 1;
}

void nd500_settings_load_env(void) {
    Nd500Settings* s = &s_settings;

    s->allocdbg           = env_flag("ND500X_ALLOCDBG");
    s->bpdbg              = env_flag("ND500X_BPDBG");
    s->byconvdbg          = env_flag("ND500X_BYCONVDBG");
    s->caddbg             = env_flag("ND500X_CADDBG");
    s->carve_bout         = env_flag("ND500X_CARVE_BOUT");
    s->chaindbg           = env_flag("ND500X_CHAINDBG");
    s->dcodedbg           = env_flag("ND500X_DCODEDBG");
    s->dit1dbg            = env_flag("ND500X_DIT1DBG");
    s->ditdbg             = env_flag("ND500X_DITDBG");
    s->domdbg             = env_flag("ND500X_DOMDBG");
    s->dropdbg            = env_flag("ND500X_DROPDBG");
    s->exedbg             = env_flag("ND500X_EXEDBG");
    s->fcmpdbg            = env_flag("ND500X_FCMPDBG");
    s->fedbg              = env_flag("ND500X_FEDBG");
    s->framelog           = env_flag("ND500X_FRAMELOG");
    s->gatedbg            = env_flag("ND500X_GATEDBG");
    s->heapdbg            = env_flag("ND500X_HEAPDBG");
    s->inddbg             = env_flag("ND500X_INDDBG");
    s->initlog            = env_flag("ND500X_INITLOG");
    s->inodedbg           = env_flag("ND500X_INODEDBG");
    s->iocdbg             = env_flag("ND500X_IOCDBG");
    s->ipcurdbg           = env_flag("ND500X_IPCURDBG");
    s->keylog             = env_flag("ND500X_KEYLOG");
    s->ksudbg             = env_flag("ND500X_KSUDBG");
    s->l2watch            = env_flag("ND500X_L2WATCH");
    s->nc_wwatch          = env_flag("ND500X_NC_WWATCH");
    s->no_invalid00       = env_flag("ND500X_NO_INVALID00");
    s->pageindbg          = env_flag("ND500X_PAGEINDBG");
    s->pathdbg            = env_flag("ND500X_PATHDBG");
    s->pgfdbg             = env_flag("ND500X_PGFDBG");
    s->pgudbg             = env_flag("ND500X_PGUDBG");
    s->physdbg            = env_flag("ND500X_PHYSDBG");
    s->privdbg            = env_flag("ND500X_PRIVDBG");
    s->prtdbg             = env_flag("ND500X_PRTDBG");
    s->pst47dbg           = env_flag("ND500X_PST47DBG");
    s->ptwdbg             = env_flag("ND500X_PTWDBG");
    s->seg30dbg           = env_flag("ND500X_SEG30DBG");
    s->seg_dump           = env_flag("ND500X_SEG_DUMP");
    s->sidbg              = env_flag("ND500X_SIDBG");
    s->slashdbg           = env_flag("ND500X_SLASHDBG");
    s->slpdbg             = env_flag("ND500X_SLPDBG");
    s->solodbg            = env_flag("ND500X_SOLODBG");
    s->stkdbg             = env_flag("ND500X_STKDBG");
    s->stodbg             = env_flag("ND500X_STODBG");
    s->stopdbg            = env_flag("ND500X_STOPDBG");
    s->syscno             = env_flag("ND500X_SYSCNO");
    s->thadbg             = env_flag("ND500X_THADBG");
    s->tickstat           = env_flag("ND500X_TICKSTAT");
    s->tlbstat            = env_flag("ND500X_TLBSTAT");
    s->traplog            = env_flag("ND500X_TRAPLOG");
    s->tsbdbg             = env_flag("ND500X_TSBDBG");
    s->tsetdbg            = env_flag("ND500X_TSETDBG");
    s->ttydbg             = env_flag("ND500X_TTYDBG");
    s->udatadbg           = env_flag("ND500X_UDATADBG");
    s->umuldbg            = env_flag("ND500X_UMULDBG");
    s->uptwdbg            = env_flag("ND500X_UPTWDBG");
    s->uwatch             = env_flag("ND500X_UWATCH");
    s->nc_typetag_guard   = env_flag("ND500X_NC_TYPETAG_GUARD");
    s->bootdbg            = env_flag("ND500X_BOOTDBG");
    s->d1dbg              = env_flag("ND500X_D1DBG");
    s->fuwdbg             = env_flag("ND500X_FUWDBG");
    s->hdlrtrace          = env_flag("ND500X_HDLRTRACE");
    s->icodedbg           = env_flag("ND500X_ICODEDBG");
    s->initdbg            = env_flag("ND500X_INITDBG");
    s->pcsample           = env_flag("ND500X_PCSAMPLE");
    s->piadbg             = env_flag("ND500X_PIADBG");
    s->swtchdbg           = env_flag("ND500X_SWTCHDBG");
    s->sysdbg             = env_flag("ND500X_SYSDBG");
    s->noxmsg             = env_flag("ND500X_NOXMSG");
    s->eth_uplink         = getenv("ND500X_ETH_UPLINK");
    s->nofeclock          = env_flag("ND500X_NOFECLOCK");
    s->nouserclock        = env_flag("ND500X_NOUSERCLOCK");
    s->console_8bit       = env_flag("ND500X_CONSOLE_8BIT");
    s->mmu_guest_tables   = env_flag("ND500X_MMU_GUEST_TABLES");
    s->translate_bs       = env_flag("ND500X_RAW_BS") ? 0 : 1;
    s->tlb_enabled        = env_flag("ND500X_NOTLB") ? 0 : 1;
    s->demand_segments    = env_flag("ND500X_NO_DEMAND_SEGMENTS") ? 0 : 1;
    s->trap_dispatch      = env_flag("ND500X_NO_TRAP_DISPATCH") ? 0 : 1;
    s->memtop             = env_u32("ND500X_MEMTOP");
    s->sfree              = env_u32("ND500X_SFREE");
    s->tape_path          = getenv("ND500X_TAPE");
    s->disk_path          = getenv("ND500X_DISK");
    s->ptewatch           = env_flag("ND500X_PTEWATCH");
    s->ptewatch_page      = env_u32_base("ND500X_PTEWATCH_PAGE", 0);
    s->ptewatch_page2     = env_u32_base("ND500X_PTEWATCH_PAGE2", 0);
    s->ptewatch_pfn       = env_u32_base("ND500X_PTEWATCH_PFN", 0);
    s->ptecatch           = env_long_base("ND500X_PTECATCH", 0, -1);
    s->framewatch         = env_u32_base("ND500X_FRAMEWATCH", 16);
    s->pguwatch           = env_long_base("ND500X_PGUWATCH", 0, -1);
    s->ptdbg_target       = env_u32_base("ND500X_PTDBG", 16);
    s->show_ea            = env_is_one("ND500X_SHOW_EA");
    s->show_hex           = env_not_zero_default_on("ND500X_SHOW_HEX");
    s->demangle           = env_is_one("ND500X_DEMANGLE");
    s->trace              = env_is_one("ND500X_TRACE");
    s->profile            = env_is_one("ND500X_PROFILE");
    s->trap_invalid       = 1;

    /* ND500X_PWATCH=<hex phys addr>[:<len>] - one variable, two fields. Unlike
     * FRAMEWATCH this does not filter on value, so it catches a partial
     * overwrite of a live word; the default length is 16 bytes. */
    {
        const char* e = getenv("ND500X_PWATCH");
        s->pwatch_base = 0;
        s->pwatch_len  = 0;
        if (e && e[0]) {
            char* end = NULL;
            s->pwatch_base = (uint32_t)strtoul(e, &end, 16);
            s->pwatch_len  = (end && *end == ':') ? (uint32_t)strtoul(end + 1, NULL, 0) : 16u;
        }
    }

    /* ND500X_DISK_RW selects what a guest write does:
     *
     *   unset / "1"  write STRAIGHT THROUGH to the image. Editing a file or
     *                writing to /tmp inside NDIX changes the image on disk and
     *                is still there next boot - which is what a real machine
     *                does. /tmp is an ordinary directory in the root filesystem
     *                (4.3BSD has no tmpfs), so it persists too.
     *   "cow"        copy-on-write session, master image untouched.
     *   "0"          read-only; FE_WRIT is a fake-success no-op.
     *
     * The default used to be "0" with "1" meaning the session copy. It was
     * changed deliberately: an emulator whose disk forgets everything is not a
     * machine anyone can use. */
    {
        const char* e = getenv("ND500X_DISK_RW");
        if (e && (e[0] == 'c' || e[0] == 'C'))      s->disk_mode = 2;
        else if (e && e[0] == '0' && e[1] == '\0') s->disk_mode = 0;
        else                                        s->disk_mode = 1;
    }
}

Nd500Settings* nd500_settings(void) {
    if (!s_ready) {
        s_ready = 1;
        nd500_settings_defaults(&s_settings);
        /* Read the environment ONCE, here, so every existing script,
         * run-ndix.sh invocation and unit test behaves exactly as before.
         * (Checked: nothing in test/ ever calls setenv, so no test depends on
         * changing a variable after the emulator has started.) A host that
         * wants nothing ambient calls nd500_settings_defaults() afterwards. */
        nd500_settings_load_env();
    }
    return &s_settings;
}
