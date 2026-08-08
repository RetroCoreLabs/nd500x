/*
 * nd500_settings.c - the settings struct, its defaults, and the environment
 * loader that keeps every existing script working.
 *
 * See nd500_settings.h for what this is and why the library stopped calling
 * getenv() itself.
 */

#include "nd500_settings.h"

#include <stdlib.h>
#include <string.h>

static Nd500Settings s_settings;
static int           s_ready = 0;

void nd500_settings_defaults(Nd500Settings* s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    /* The one field whose "off" is not zero. See the header: the environment
     * variable is ND500X_RAW_BS and it is inverted, so translating backspace
     * is the DEFAULT and setting the variable turns it off. */
    s->translate_bs = 1;
}

/* "set and not 0" - the test every one of the old getenv sites used. */
static int env_flag(const char* name) {
    const char* e = getenv(name);
    return (e && e[0] && e[0] != '0') ? 1 : 0;
}

/* A positive number, or 0 for "unset or unparseable". Deliberately strict:
 * a partly-numeric value is a typo, and silently using the prefix of a typo is
 * how you end up with a kernel that has no page frames. */
static uint32_t env_u32(const char* name) {
    const char* e = getenv(name);
    if (!e || !e[0]) return 0;
    char* end = NULL;
    long v = strtol(e, &end, 0);
    if (!end || *end != '\0' || v <= 0) return 0;
    return (uint32_t)v;
}

void nd500_settings_load_env(void) {
    Nd500Settings* s = &s_settings;

    s->fedbg      = env_flag("ND500X_FEDBG");
    s->inddbg     = env_flag("ND500X_INDDBG");
    s->gatedbg    = env_flag("ND500X_GATEDBG");
    s->inodedbg   = env_flag("ND500X_INODEDBG");
    s->sidbg      = env_flag("ND500X_SIDBG");
    s->ttydbg     = env_flag("ND500X_TTYDBG");
    s->keylog     = env_flag("ND500X_KEYLOG");
    s->tickstat   = env_flag("ND500X_TICKSTAT");
    s->carve_bout = env_flag("ND500X_CARVE_BOUT");

    s->noxmsg       = env_flag("ND500X_NOXMSG");
    s->nofeclock    = env_flag("ND500X_NOFECLOCK");
    s->nouserclock  = env_flag("ND500X_NOUSERCLOCK");
    s->console_8bit = env_flag("ND500X_CONSOLE_8BIT");

    /* Inverted on purpose - ND500X_RAW_BS=1 means "do not translate". */
    s->translate_bs = env_flag("ND500X_RAW_BS") ? 0 : 1;

    s->memtop = env_u32("ND500X_MEMTOP");
    s->sfree  = env_u32("ND500X_SFREE");

    s->tape_path = getenv("ND500X_TAPE");
    s->disk_path = getenv("ND500X_DISK");

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
     * machine anyone can use. Anything that must not be modified should be run
     * with "cow" or "0". */
    {
        const char* e = getenv("ND500X_DISK_RW");
        if (e && (e[0] == 'c' || e[0] == 'C'))            s->disk_mode = 2;
        else if (e && e[0] == '0' && e[1] == '\0')        s->disk_mode = 0;
        else                                              s->disk_mode = 1;
    }
}

Nd500Settings* nd500_settings(void) {
    if (!s_ready) {
        s_ready = 1;
        nd500_settings_defaults(&s_settings);
        /* Read the environment ONCE, here, so that every existing script,
         * run-ndix.sh invocation and test keeps behaving exactly as it did.
         * A host that wants no ambient influence - a browser build, or a test
         * that must be hermetic - calls nd500_settings_defaults() afterwards
         * and then fills in what it wants. Nothing else in the emulator reads
         * the environment. */
        nd500_settings_load_env();
    }
    return &s_settings;
}
