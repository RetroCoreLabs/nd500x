/*
 * nd500_ndix_boot.h - build an ND-500 machine that can run the NDIX kernel.
 *
 * WHY THIS FILE EXISTS
 *
 * Everything here used to live in two places that a browser build cannot
 * reach: the debugger's command table (mmusetup, map-kdata, ndix-uarea) and
 * the native --ndix frontend, which drove them by writing COMMAND STRINGS and
 * handing them to nd500_cmd_execute(). That is fine for a program with a
 * terminal and a REPL. It is useless to a caller with no main(), no argv and
 * no debugger - which is exactly what the wasm build is.
 *
 * So the work moved down here, into the machine library, as ordinary C
 * functions taking ordinary arguments. The debugger commands and the native
 * frontend are now two callers of ONE implementation, and a third caller (the
 * browser) needs neither of them.
 *
 * WHAT THIS IS NOT
 *
 * It is not a hardware model. Every function here does a job that on real
 * hardware belongs to the ND-100 running SINTRAN: SINTRAN owns the page and
 * domain tables, does the context load, and seeds the shared segment before
 * the ND-500 kernel is ever started. There is no ND-100 here yet, so nd500x
 * does it. When the 3022 bus interface is real (plan milestone M5) these are
 * the functions that get REPLACED by the ND-100 actually doing them - which is
 * the second reason to have them in one named place rather than spread across
 * a command table.
 *
 * PATH DEPENDENCE, DELIBERATE
 *
 * nd500_ndix_boot() reads the kernel from a host path. That is honest for now:
 * the a.out has to exist as a real file because ndlib_symbols_load() takes a
 * path, and losing the kernel symbols would cost every symbolic name in the
 * debugger and the _u lookup the trap vector is derived from. A browser host
 * writes the kernel into its virtual filesystem first. Only the DISK is served
 * through callbacks (Nd500HostOps, nd500_host.h).
 */
#ifndef ND500_NDIX_BOOT_H
#define ND500_NDIX_BOOT_H

#include <stdint.h>

struct Nd500Machine;

/* ---- where the hand-built tables live ------------------------------------
 *
 * These addresses sit in the free gap between the loaded kernel image and the
 * start of kernel free memory (firstaddr, physical 0x100000). They were fixed
 * addresses in the debugger commands' default arguments; naming them here is
 * what lets a caller see the layout instead of inheriting it by accident.
 *
 * The values themselves are unchanged. PSTP/DITBASE are set by
 * nd500_ndix_mmu_setup(); see the comment on it for why PSTP must sit ABOVE
 * the loaded DSEG. */
#define ND500_NDIX_PSTP           0x00084000u  /* guest PST base               */
#define ND500_NDIX_DITBASE        0x00090000u  /* guest DIT base (64 KB)       */
#define ND500_NDIX_KDATA_PSN      802u         /* PSN describing kernel data   */
#define ND500_NDIX_KDATA_PT_PHYS  0x000A2000u  /* its PS_ASI page table        */
#define ND500_NDIX_UAREA_PT_PHYS  0x000A3000u  /* page table for segment 29    */
#define ND500_NDIX_UAREA_PG_PHYS  0x000A3800u  /* first of UPAGES u-area pages */

/* ---- logging -------------------------------------------------------------
 *
 * Two channels, because the old code had two and collapsing them would change
 * what a boot prints:
 *
 *   NOTICE  - the handful of lines that ALWAYS appeared, printed with printf
 *             straight from the command bodies (e.g. "ND-500: proc0 kernel
 *             stack ..."). Default sink: stdout, so nothing changes for a
 *             native run. A browser host replaces it.
 *   VERBOSE - the running commentary the debugger commands sent to output(),
 *             which on the --ndix boot path went NOWHERE because the frontend
 *             passes a bare CmdContext. Default sink: discarded, which
 *             reproduces that exactly. The debugger command wrappers install a
 *             sink that forwards to output().
 *
 * Both take one fully formatted line WITHOUT a trailing newline.
 *
 * Passing NULL RESTORES THE DEFAULT rather than silencing the channel, so a
 * caller that binds a sink pointing at its own stack can safely hand it back
 * when it returns without leaving every later boot mute. */
typedef void (*Nd500NdixLog)(void* ctx, const char* line);

void nd500_ndix_set_notice_log(Nd500NdixLog fn, void* ctx);
void nd500_ndix_set_verbose_log(Nd500NdixLog fn, void* ctx);

/* ---- the steps, individually --------------------------------------------
 *
 * Exposed one by one as well as through nd500_ndix_boot(), because the
 * debugger commands are still real commands people type, and because the
 * <kernel>.init boot route runs some of these and not others. */

/* Domains, PST, DIT, the kernel's own table segments (27/28), the shared
 * segment's Xmsg rings and IPL record - then enables the MMU. Returns 0, or
 * -1 with a notice line when there is no CPU. */
int nd500_ndix_mmu_setup(struct Nd500Machine* m);

/* Describe the kernel's own data segment (domain 0, segment 0) in the GUEST
 * tables so kernacc()/getpte() can see it - without this /dev/kmem is
 * unusable and ps/w/vmstat/netstat all fail. Writes the guest PST and DIT
 * ONLY, never the emulator's shadow table: see the implementation for why
 * that distinction is load-bearing.
 *
 * <phys_base> must be page aligned. Pass 0 for <psn>/<pt_phys> to take
 * ND500_NDIX_KDATA_PSN / ND500_NDIX_KDATA_PT_PHYS. Requires mmu_setup first. */
int nd500_ndix_map_kdata(struct Nd500Machine* m, uint32_t phys_base, uint32_t size,
                         uint32_t psn, uint32_t pt_phys);

/* Build proc0's kernel-stack / u-area segment (segment 29, _u at 0xE8000000).
 * machdep.c derives the well-known kernel segment indices but never assigns
 * Pst[stackindex] - it assumes an ND-100 bootstrap already filled it - and
 * init_main.c then reads that slot to build proc0's p_p0br.
 *
 * Pass 0 for either address to take ND500_NDIX_UAREA_PT_PHYS /
 * ND500_NDIX_UAREA_PG_PHYS. Requires mmu_setup first. */
int nd500_ndix_uarea(struct Nd500Machine* m, uint32_t pt_phys, uint32_t uarea_phys);

/* ---- the whole boot ------------------------------------------------------ */

typedef struct Nd500NdixBoot {
    /* The kernel a.out. Required. */
    const char* kernel_path;
    /* Optional pre-split segment files. When BOTH exist they are loaded as-is;
     * otherwise both the sizes and the placement are derived from the a.out
     * header, which is what lets the kernel come out of a disk image where no
     * segment files exist to sit beside it. NULL means "derive". */
    const char* pseg_path;
    const char* dseg_path;
    /* Include the u-area step. The native frontend does it itself because it
     * must happen on the <kernel>.init route too; a browser host wants it
     * here. Non-zero = do it. */
    int with_uarea;
} Nd500NdixBoot;

/* Load the kernel and build every table it needs, leaving the machine ready to
 * run: load a.out (entry PC + symbols) -> mmu_setup -> place the segments ->
 * map-kdata -> THA/CTE1/CTE2/CAD -> optionally the u-area.
 *
 * Does NOT start the CPU. Running it is the caller's business: a native
 * frontend has a REPL, a browser has a scheduler, and neither wants the other
 * one's loop. Returns 0 on success, -1 with a notice line on failure. */
int nd500_ndix_boot(struct Nd500Machine* m, const Nd500NdixBoot* cfg);

#endif /* ND500_NDIX_BOOT_H */
