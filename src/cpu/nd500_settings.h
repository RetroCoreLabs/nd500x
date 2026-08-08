/*
 * nd500_settings.h - every knob the emulator has, as plain fields.
 *
 * WHY THIS EXISTS
 * ---------------
 * These settings used to be eighteen getenv() calls scattered through
 * nd500_fecall.c, each with its own latch and its own bit of string parsing.
 * That had three problems:
 *
 *   1. A WASM build has no environment variables at all, so the emulator could
 *      not be configured in a browser - and could not run there.
 *   2. There was no single place that said what the knobs even ARE. Finding
 *      them meant grepping for getenv.
 *   3. Every read was a string lookup plus a hand-rolled "set and not 0" test,
 *      with the RAW_BS one quietly inverted relative to all the others.
 *
 * So the library does not read the environment. It reads THIS STRUCT. A host
 * fills it in however it likes: the native frontend from getenv (see
 * nd500_settings_load_env in nd500_host_posix.c, which keeps every variable
 * name exactly as it was, so run-ndix.sh and every existing script still work),
 * a browser build from JS, and eventually the machine-configuration UI straight
 * from a form - the fields below map onto it one for one.
 *
 * The struct is mutable and lives for the life of the process. Change a field
 * whenever you like; nothing caches it. Defaults are what an unset environment
 * used to produce, so a host that fills in nothing gets today's behaviour.
 *
 * Sibling: nd500_host.h, which covers what the ND-500 needs DONE for it (block
 * storage). This file is what it needs TOLD.
 */

#ifndef ND500_SETTINGS_H
#define ND500_SETTINGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nd500Settings {

    /* ---- Diagnostics. All default 0 = quiet. --------------------------- */
    int fedbg;        /* ND500X_FEDBG      trace every fecall in and out     */
    int inddbg;       /* ND500X_INDDBG     cross-domain CALL tracing         */
    int gatedbg;      /* ND500X_GATEDBG    interrupt-gate tracing            */
    int inodedbg;     /* ND500X_INODEDBG   dump root inode on the FS block   */
    int sidbg;        /* ND500X_SIDBG      software-interrupt tracing        */
    int ttydbg;       /* ND500X_TTYDBG     terminal traffic                  */
    int keylog;       /* ND500X_KEYLOG     log every key into the guest      */
    int tickstat;     /* ND500X_TICKSTAT   clock tick accounting             */
    int carve_bout;   /* ND500X_CARVE_BOUT WFILE frame-chain carve (MON path)*/

    /* ---- Behaviour ----------------------------------------------------- */

    /* Fail the XMSG (generic 7) device init, so xgattach gives up instead of
     * half-attaching. Diagnostic for the ethernet work. */
    int noxmsg;       /* ND500X_NOXMSG */

    /* Suppress the front-end 50 Hz clock / the user-visible clock tick. */
    int nofeclock;    /* ND500X_NOFECLOCK   */
    int nouserclock;  /* ND500X_NOUSERCLOCK */

    /* Pass console bytes through with all 8 bits instead of masking to 7. */
    int console_8bit; /* ND500X_CONSOLE_8BIT */

    /* Translate the host's backspace to the guest's erase character.
     *
     * NOTE THE POLARITY. This is the one setting whose environment variable is
     * inverted: ND500X_RAW_BS=1 means "send it RAW", i.e. do NOT translate. As
     * a field it is stated the positive way round, and DEFAULTS TO 1. A host
     * filling this struct from scratch must set it, or backspace stops working
     * - which is exactly the kind of trap that made the getenv version worth
     * replacing. */
    int translate_bs; /* ND500X_RAW_BS, inverted. Default 1. */

    /* ---- Memory layout. 0 means "use the built-in value". -------------- */

    /* Report less memory to NDIX than really exists, to exercise the paging
     * path. Ignored unless it leaves at least one page above sfree, so a
     * mistyped value cannot produce a kernel with zero page frames. */
    uint32_t memtop;  /* ND500X_MEMTOP */

    /* First free ND-500-relative physical byte: where NDIX's own page pool
     * begins. Also becomes the allocator's floor, so the two cannot drift. */
    uint32_t sfree;   /* ND500X_SFREE */

    /* ---- Media --------------------------------------------------------- */

    /* Tape image for the cartridge/mag-tape fecalls. NULL or "" = no tape.
     * Not copied - the pointer must outlive the machine. */
    const char* tape_path;   /* ND500X_TAPE */

    /* Root disk image and how guest writes are treated. Read by the POSIX
     * host when it auto-mounts unit 0; a host that mounts units itself can
     * ignore both. disk_mode: 0 = read-only, 1 = write through to the image,
     * 2 = copy-on-write session (master image untouched). */
    const char* disk_path;   /* ND500X_DISK    */
    int         disk_mode;   /* ND500X_DISK_RW: unset/"1" -> 1, "cow" -> 2, "0" -> 0 */

} Nd500Settings;

/*
 * The settings. Never NULL. On first use the defaults are installed and, on a
 * POSIX build, the environment is read once (nd500_settings_load_env) so that
 * existing scripts keep working with no code change anywhere.
 */
Nd500Settings* nd500_settings(void);

/*
 * Reset to defaults - quiet diagnostics, translate_bs = 1, everything else 0.
 * Does NOT read the environment. Use this from a host that intends to fill the
 * struct itself and wants no ambient influence at all.
 */
void nd500_settings_defaults(Nd500Settings* s);

/*
 * Fill from the environment, using the variable names in the comments above.
 * A no-op where there is no environment. Safe to call more than once.
 */
void nd500_settings_load_env(void);

#ifdef __cplusplus
}
#endif

#endif /* ND500_SETTINGS_H */
