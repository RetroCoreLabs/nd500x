/*
 * nd500x_shell.c - SINTRAN-flavoured interactive shell (--monitor mode).
 *
 * Phase 1: local terminal. Command surface sourced from the ND manuals; see
 * docs/SINTRAN-SHELL-SPEC.md for every citation.
 *
 * Faithful bits reproduced here:
 *  - "@" command prompt (ND-60.128.5 line 158).
 *  - Run a program by typing its name / RECOVER-DOMAIN <name>, resolving in the
 *    current user's directory then user SYSTEM's (ND-60.050.06 lines 1720-1724,
 *    already how src/libmon/mon_path.c resolves names).
 *  - Command abbreviation to a unique prefix per hyphen-word: LIST-FILES may be
 *    L-FIL or LI-FI, LI-F is AMBIGUOUS COMMAND (ND-60.132.03 lines 1044-1052).
 *  - Plain-text error messages (NO SUCH FILE NAME, AMBIGUOUS COMMAND,
 *    NO SUCH COMMAND OR DOMAIN) - ND-60.132.03 lines 1204-1258.
 *  - SET-/GET-TERMINAL-TYPE with the sourced 16-bit type numbers
 *    (ND-60.128.5 lines 9309-9334, 17632-17689). 6 = DEC-VT100,
 *    53 = TANDBERG TDV-2200/9-ND NOTIS, 93 = TDV-2200/9S.
 *
 * nd500x conveniences, explicitly NOT authentic (flagged in the spec):
 *  - "login <user>" is a simplified stand-in for the real ESC-driven "User Name"
 *    + hidden-password flow (the byte-proven password fold is deferred).
 */

#include "nd500x_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <ndmon/mon.h>
#include <ndmon/mon_log.h>
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_config.h>
#include <ndmon/mon_terminal_state.h>
#include "../../ndlib/ndlib.h"
#include "nd500_dom.h"
#include "nd500x_telnet.h"

/* Segment-allocator C-side state snapshot (nd500_segment_alloc.c) - used to make
 * a nested 317B UECOM program run transparent to its caller. */
void* nd500_segment_alloc_state_save(void* machine_ptr);
void  nd500_segment_alloc_state_restore(void* blob);

/* ND500X_STODBG helper (nd500_mmu.h is not pulled in by cpu_protos.h). */
uint32_t nd500_mmu_peek(Nd500Cpu* cpu, uint32_t virtual_addr);

/* ND500X_LOADDBG=1: forward DOM-loader log lines to stderr, so a silent
 * "DOM configuration failed" can be diagnosed (the loader reports its
 * specific failure only through this callback). */
static void loaddbg_cb(void* ctx, const char* fmt, ...) {
    (void)ctx;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* Post-run cleanup helper (nd500_domain.c - same reason). */
void nd500_domain_free(Nd500Cpu* cpu, uint8_t domain);

/* MMU-table snapshot (nd500_mmu.c) - the nested UECOM DOM load overwrites
 * PST entries / capabilities the caller's domain still references; without
 * restoring these the caller resumes on a WRONG virtual-to-physical mapping
 * (its heap vars, THA vector and stack limits all read from another domain's
 * pages -> the NC exit "No trap handler at THA[27]" stack-overflow crash).
 * See docs/HANDOFF-NC-HEAP-CRASH-2026-07-27.md section 2c. */
void* nd500_mmu_state_save(void);
void  nd500_mmu_state_restore(void* blob);

/* Transport: shell output/input goes to the local console or a telnet client.
 * All shell text below uses printf, which is routed via shell_printf(). */
static int g_use_telnet = 0;

static void sh_puts(const char* s) {
    if (g_use_telnet) nd500x_telnet_write(s);
    else { fputs(s, stdout); fflush(stdout); }
}
static void shell_printf(const char* fmt, ...) {
    char buf[2048];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sh_puts(buf);
}
#define printf shell_printf

/* ------------------------------------------------------------------ state */

static Nd500Machine* g_machine = NULL;
static Nd500Cpu*     g_cpu     = NULL;
static int           g_logged_in = 0;
static int           g_running = 1;          /* shell REPL keeps going */

/* Logical device for "your own terminal" (SINTRAN convention: device 1). The
 * terminal type is kept in the emulator's per-terminal state, not a shell-local
 * variable, so MON 16B/17B and a running DOM share the exact same value. */
#define SHELL_TERM_DEVICE 1
static struct termios g_cooked_termios;      /* saved on start for restore */
static int           g_have_cooked = 0;

/* Generous safety cap so a runaway DOM cannot wedge the shell forever. */
#define SHELL_MAX_STEPS 2000000000ULL

/* ---- MODE (script) execution -------------------------------------------
 * A MODE file is a script of shell command lines, optionally INTERLEAVED with
 * input consumed by programs the script starts (e.g. NC-A06 then its own
 * CHECK / GENERATE-CODE / EXIT lines) - exactly like piping the lines to stdin.
 * We feed the whole (parameter-substituted) file through ONE console so that
 * the mode line-reader AND any program started mid-script draw from the SAME
 * stream, matching real SINTRAN @MODE semantics (ND-60.128.5 p200, ND-60.050.06
 * section 3.3.8). Command echo + program output go to the terminal (this is why
 * we cannot reuse the capture-to-buffer queued console). g_mode_active tells
 * run_domain to keep this console instead of reinstalling the stdio one. */
static char*  g_mode_buf    = NULL;   /* parameter-substituted script text */
static size_t g_mode_len    = 0;
static size_t g_mode_pos    = 0;
static int    g_mode_active = 0;      /* >0 while a MODE script is running */
static int    g_mode_depth  = 0;      /* nesting depth (SINTRAN max 10) */

static int mode_read_char(void* ctx) {
    (void)ctx;
    if (g_mode_pos >= g_mode_len) return -1;              /* EOF ends the mode */
    char c = g_mode_buf[g_mode_pos++];
    return (c == '\n') ? '\r' : (int)(unsigned char)c;    /* SINTRAN line break = CR */
}
static int mode_peek_char(void* ctx) {
    (void)ctx;
    if (g_mode_pos >= g_mode_len) return -1;
    char c = g_mode_buf[g_mode_pos];
    return (c == '\n') ? '\r' : (int)(unsigned char)c;
}
static bool mode_char_available(void* ctx) { (void)ctx; return g_mode_pos < g_mode_len; }
static int  mode_wait_for_input(void* ctx) { (void)ctx; return g_mode_pos < g_mode_len ? 1 : 0; }
static void mode_write_char(void* ctx, int ch) {
    (void)ctx;
    if (g_use_telnet) { char s[2] = { (char)ch, 0 }; nd500x_telnet_write(s); }
    else { putchar(ch); fflush(stdout); }
}
static ConsoleIO g_mode_console = {
    .read_char      = mode_read_char,
    .write_char     = mode_write_char,
    .char_available = mode_char_available,
    .peek_char      = mode_peek_char,
    .wait_for_input = mode_wait_for_input,
    .context        = NULL,
};

/* Read one line (up to CR) from the mode stream into out; -1 at end of stream. */
static int mode_getline(char* out, size_t n) {
    size_t i = 0;
    for (;;) {
        int c = mode_read_char(NULL);
        if (c < 0) { if (i == 0) return -1; break; }     /* EOF */
        if (c == '\r' || c == '\n') break;               /* end of line */
        if (i < n - 1) out[i++] = (char)c;
    }
    out[i] = '\0';
    return (int)i;
}

/* Terminal-type names are NOT hardcoded - they are read from the real VTM
 * terminal-definition file (e.g. DDBTABLES-*.VTM) that ships with the system,
 * which embeds the authoritative "<number>: <name>" list. Different VTM files
 * carry different lists, so the shell must read whatever the loaded system has.
 * The list is located under the SINTRAN root (current user's dir, then SYSTEM)
 * and parsed on demand; the cache is cleared on login/logout. */
#define VTM_MAX 256
static int  g_vtm_count = -1;              /* -1 = not yet loaded */
static int  g_vtm_num[VTM_MAX];
static char g_vtm_name[VTM_MAX][40];
static char g_vtm_path[1024];

/* Find a *.VTM file under the SINTRAN root: current user's dir first, then
 * SYSTEM. Returns 1 and fills out on success. */
static int vtm_locate(char* out, size_t n) {
    const char* root = mon_config_get_sintran_root();
    const char* user = mon_config_get_current_user();
    if (!root || !*root) root = ".";
    char dirs[2][1024];
    snprintf(dirs[0], sizeof dirs[0], "%s/%s", root, (user && *user) ? user : "SYSTEM");
    snprintf(dirs[1], sizeof dirs[1], "%s/SYSTEM", root);
    for (int d = 0; d < 2; d++) {
        DIR* dp = opendir(dirs[d]);
        if (!dp) continue;
        struct dirent* e;
        while ((e = readdir(dp)) != NULL) {
            const char* dot = strrchr(e->d_name, '.');
            if (dot && strcasecmp(dot, ".VTM") == 0) {
                snprintf(out, n, "%s/%s", dirs[d], e->d_name);
                closedir(dp);
                return 1;
            }
        }
        closedir(dp);
    }
    return 0;
}

/* Parse the embedded "<num>: <name>" terminal list out of the VTM file. */
static void vtm_load(void) {
    g_vtm_count = 0;
    g_vtm_path[0] = '\0';
    char path[1024];
    if (!vtm_locate(path, sizeof path)) return;
    snprintf(g_vtm_path, sizeof g_vtm_path, "%s", path);

    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 8 * 1024 * 1024) { fclose(f); return; }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); return; }

    long i = 0;
    while (i < sz && g_vtm_count < VTM_MAX) {
        /* Entry = start-of-number, 1..3 digits, ':', space(s), name terminated
         * by a run of 2+ spaces or a control byte (the column padding). */
        int at_start = (i == 0) || buf[i-1] == ' ' || buf[i-1] < 32;
        if (at_start && isdigit(buf[i])) {
            long j = i;
            while (j < sz && isdigit(buf[j])) j++;
            if (j < sz && buf[j] == ':' && (j - i) <= 3) {
                int num = atoi((char*)&buf[i]);
                long k = j + 1;
                while (k < sz && buf[k] == ' ') k++;
                long m = k;
                while (m < sz && buf[m] >= 32) {
                    if (buf[m] == ' ' && m + 1 < sz && buf[m+1] == ' ') break;
                    m++;
                }
                long len = m - k;
                while (len > 0 && buf[k + len - 1] == ' ') len--;
                if (len > 0 && len < (long)sizeof g_vtm_name[0] &&
                    isalpha(buf[k]) && num >= 0 && num <= 999) {
                    g_vtm_num[g_vtm_count] = num;
                    memcpy(g_vtm_name[g_vtm_count], &buf[k], (size_t)len);
                    g_vtm_name[g_vtm_count][len] = '\0';
                    g_vtm_count++;
                }
                i = m;
                continue;
            }
        }
        i++;
    }
    free(buf);
}

static void vtm_invalidate(void) { g_vtm_count = -1; }
static void vtm_ensure(void) { if (g_vtm_count < 0) vtm_load(); }

/* Name for a terminal type, straight from the VTM; NULL if not listed. */
static const char* vtm_name(int type) {
    vtm_ensure();
    for (int i = 0; i < g_vtm_count; i++) if (g_vtm_num[i] == type) return g_vtm_name[i];
    return NULL;
}

/* Print the terminal-type list from the VTM, two per row. */
static void vtm_list(void) {
    vtm_ensure();
    if (g_vtm_count <= 0) {
        printf("No terminal-type table found - expected a *.VTM file under the SINTRAN\n");
        printf("root (%s), in the current user's directory or SYSTEM.\n",
               mon_config_get_sintran_root());
        return;
    }
    printf("Terminal types are (from %s):\n", g_vtm_path);
    for (int i = 0; i < g_vtm_count; i++) {
        printf("%4d: %-34.34s", g_vtm_num[i], g_vtm_name[i]);
        if ((i & 1) || i == g_vtm_count - 1) printf("\n");
    }
}

/* ------------------------------------------------------------- tokenising */

/* Split a line into up to max tokens on whitespace and commas (SINTRAN uses
 * comma-separated parameters). Returns the token count. Tokens point into buf,
 * which is modified in place. */
static int tokenise(char* buf, char** tok, int max) {
    int n = 0;
    char* p = buf;
    while (*p && n < max) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != ',') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

static void str_upper(char* s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* True if abbrev matches command name under the hyphen-word prefix rule. */
static int cmd_matches(const char* abbrev, const char* name) {
    /* Walk both as sequences of '-'-separated words; every abbrev word must be a
     * (non-empty) prefix of the corresponding name word; abbrev may have fewer
     * words than the name but not more. */
    const char* a = abbrev;
    const char* c = name;
    for (;;) {
        /* match one word */
        if (!*a) return 0;                 /* empty abbrev word -> no match */
        while (*a && *a != '-' && *c && *c != '-') {
            if (toupper((unsigned char)*a) != toupper((unsigned char)*c)) return 0;
            a++; c++;
        }
        if (*a && *a != '-') return 0;     /* abbrev word longer than name word */
        /* advance c to end of its word */
        while (*c && *c != '-') c++;
        /* end of abbrev? then it matched a prefix of the words so far */
        if (!*a) return 1;
        /* both must have a following '-' word */
        if (*a == '-' && *c == '-') { a++; c++; continue; }
        return 0;
    }
}

/* -------------------------------------------------------------- DOM runner */

/* Resolve NAME to <root>/<user>/<NAME>.DOM. Returns:
 *   1  unique hit (out[] filled),
 *   0  no such domain in this directory,
 *  -1  ambiguous - more than one abbreviation match (never guesses). */
static int dom_host_path(const char* user, const char* name, char* out, size_t n) {
    const char* root = mon_config_get_sintran_root();
    char upname[64];
    snprintf(upname, sizeof(upname), "%s", name);
    str_upper(upname);
    /* strip an explicit :type if the user typed NAME:DOM */
    char* colon = strchr(upname, ':');
    if (colon) *colon = '\0';
    snprintf(out, n, "%s/%s/%s.DOM", root && *root ? root : ".", user, upname);
    if (access(out, F_OK) == 0) return 1;
    /* Literal NAME.DOM absent: apply the SINTRAN abbreviated-name rules
     * (carved COMPS/GOBJI) so "LIN" resolves to LINKER.DOM, exactly as
     * RECOVER-DOMAIN / OPEN would. A unique prefix hit rewrites out[];
     * an ambiguous prefix is reported, NOT resolved to an arbitrary pick. */
    char resolved[1024];
    int rc = mon_resolve_abbrev(out, resolved, sizeof(resolved));
    if (rc == 0) { snprintf(out, n, "%s", resolved); return 1; }
    if (rc == -47) return -1;   /* 057 ambiguous file name */
    return 0;                    /* -46 no such file name */
}

/* Resolve a domain name to a host path: current user's directory first, then
 * SYSTEM (ND-60.050.06 1720-1724). Returns 1 (out[] filled), 0 (not found),
 * or -1 (ambiguous in a searched directory - refuse to guess). */
static int resolve_domain(const char* name, char* out, size_t n) {
    const char* user = mon_config_get_current_user();
    if (user && *user) {
        int rc = dom_host_path(user, name, out, n);
        if (rc != 0) return rc;   /* unique hit or ambiguous - stop here */
    }
    return dom_host_path("SYSTEM", name, out, n);
}

/* Restore cooked terminal mode after a DOM run left the stdio console raw. */
static void restore_cooked(void) {
    if (g_have_cooked && isatty(STDIN_FILENO)) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_cooked_termios);
    }
}

/* Nested command runner for MON 317B UECOM (registered via mon_set_execute_command).
 * NC-A06 (CAT-NC-CROSS-A) invokes its code-generator back-end CAT-CAT5-B06 - and
 * other passes - as NESTED SINTRAN commands via UECOM. We resolve the command to
 * a program, run it RE-ENTRANTLY sharing the global monitor file table (so the
 * back-end reads the scratch the front-end produced and writes the caller's
 * opened output), then restore the caller and return so it resumes after UECOM.
 * Return: 0 ran ok; -1 not a known program (caller keeps benign stub); >0 failed. */
#define UECOM_MAX_NEST 4
static int g_uecom_nest = 0;

/* A program suspended on a DEVICE-0 read (MON 1B INBT, logical device 0 = the
 * SINTRAN command buffer). The command buffer only ever held the invocation
 * arguments, so once exhausted the program could never see anything the user
 * typed: the run loop's mon_console_wait_for_input() confirmed a console byte,
 * but the retried INBT re-read the (still empty) command buffer and suspended
 * again, forever (CODE-COVERAGE and the PLANC compiler prompt-read this way).
 * Real SINTRAN refills a background program's command buffer with the next
 * line from the command input device - the terminal. Model that: read ONE
 * line from the active console (echoing it), load it as the new command
 * buffer content, and let the caller resume the program. Deliberately NOT a
 * fallthrough to console reads inside INBT itself: that would steal bytes
 * from programs that interleave device-0 polls with device-1/DVINST reads
 * (the ND linker) and from MODE script streams.
 * Returns 1 when a line was fed, 0 on EOF (nothing can ever arrive), and -1
 * on an ESCAPE user break (the caller aborts the program back to '@'). */
static int shell_feed_command_line(void) {
    ConsoleIO* con = mon_file_table_get_console();
    if (!con || !con->read_char) return 0;
    char line[256];
    size_t n = 0;
    if (!mon_console_wait_for_input()) return 0;
    for (;;) {
        int c = con->read_char(con->context);
        if (c < 0) {                       /* EOF mid-line */
            if (n == 0) return 0;
            break;                         /* deliver what we have */
        }
        /* ESCAPE user break: while blocked here the run loop's async break
         * poll is not running, so the check must happen on the byte itself -
         * otherwise ESC was swallowed as ordinary line input and a program
         * reading device 0 (CODE-COVERAGE, PLANC) could not be aborted.
         * Honors 71B DESCF / 72B EESCF via mon_is_escape_break. */
        if (mon_is_escape_break(1, (uint8_t)c)) return -1;
        if (c == '\r' || c == '\n') break;
        if (n < sizeof(line) - 1) line[n++] = (char)c;
        if (con->write_char) con->write_char(con->context, c);   /* echo */
        if (con->char_available && !con->char_available(con->context)) {
            if (!mon_console_wait_for_input()) break;
        }
    }
    line[n] = '\0';
    if (con->write_char) {                 /* echo the line terminator */
        con->write_char(con->context, '\r');
        con->write_char(con->context, '\n');
    }
    mon_set_command_buffer(line);          /* resets read pos; INBT retry reads it */
    return 1;
}

/* Resume decision for a STOP_WAIT_INPUT suspend: device 0 refills the command
 * buffer from the console; any other device just blocks for console input
 * (the retried read consumes it directly). Returns 1 to resume, 0 to end the
 * run (EOF, or ESCAPE user break - reported like the async break poll). */
static int shell_wait_input_resume(void) {
    if (g_machine->stop_data == 0) {
        int r = shell_feed_command_line();
        if (r < 0) {
            g_machine->stop_reason = STOP_USER_REQUESTED;
            printf("\n-- aborted (ESCAPE user break) --\n");
            return 0;
        }
        return r;
    }
    return mon_console_wait_for_input();
}

static int shell_execute_command(void* cpu_v, void* machine_v, const char* command) {
    (void)cpu_v; (void)machine_v;   /* use the shell globals g_cpu / g_machine */
    if (!command || !*command) return -1;

    /* Split "NAME <args...>" - first whitespace-delimited token is the program. */
    char name[128];
    const char* p = command;
    while (*p == ' ' || *p == '\t') p++;
    int ni = 0;
    while (*p && *p != ' ' && *p != '\t' && ni < (int)sizeof(name) - 1) name[ni++] = *p++;
    name[ni] = '\0';
    while (*p == ' ' || *p == '\t') p++;
    const char* args = p;   /* remainder (may be empty) */
    if (name[0] == '\0') return -1;

    char path[1024];
    if (resolve_domain(name, path, sizeof(path)) != 1)
        return -1;          /* not a known program -> benign stub in the handler */

    if (g_uecom_nest >= UECOM_MAX_NEST) {
        mon_log(MON_LOG_WARN, "UECOM: nesting too deep, refusing '%s'", name);
        return 1;
    }

    /* Save the caller's FULL CPU state (registers, PC, domain, MMU regs, trap +
     * pending-call state). The nested run intentionally KEEPS its changes to
     * machine MEMORY and the FILE TABLE (that is how the back-end's output
     * survives); only the caller's CPU context must be restored so it resumes
     * exactly after its UECOM MON call. The caller's domain register bank
     * (TOS/LL/HL/THA in its DIT slot) is untouched by the sub-program's own
     * freshly-allocated domain. */
    /* ND500X_STODBG: dump the caller's heap-vars (at TOS) at the snapshot and
     * restore boundaries - discriminating experiment for the NC exit crash,
     * see docs/HANDOFF-NC-HEAP-CRASH-2026-07-27.md section 2b. */
    if (getenv("ND500X_STODBG")) {
        uint32_t hv[3] = { 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu };
        for (int i = 0; i < 3; i++) {
            uint32_t pa = nd500_mmu_peek(g_cpu, g_cpu->TOS + (uint32_t)i * 4u);
            if (pa != 0xFFFFFFFFu) hv[i] = nd500_bus_read32(g_machine, pa);
        }
        fprintf(stderr, "[STODBG] UECOM pre-snapshot '%s' nest=%d TOS=0x%08X "
                "MAXL=0x%08X STAH=0x%08X ENDH=0x%08X THA=0x%08X OTE1=0x%08X CED=%u\n",
                name, g_uecom_nest, g_cpu->TOS, hv[0], hv[1], hv[2],
                g_cpu->THA, g_cpu->OTE1, g_cpu->CED);
        /* Fixed probe addresses for the CAT-CAT5-B exit crash: heap-vars MAXL at
         * VA 0x08022B8C and the THA[27] vector slot at VA 0x080235E8 (both in
         * CAT's seg-1 data space), with their physical translations. */
        uint32_t pa_hv  = nd500_mmu_peek(g_cpu, 0x08022B8Cu);
        uint32_t pa_tha = nd500_mmu_peek(g_cpu, 0x080235E8u);
        fprintf(stderr, "[STODBG]   probe pre  va08022B8C pa=0x%08X val=0x%08X | "
                "va080235E8 pa=0x%08X val=0x%08X\n",
                pa_hv,  (pa_hv  != 0xFFFFFFFFu) ? nd500_bus_read32(g_machine, pa_hv)  : 0xDEADBEEFu,
                pa_tha, (pa_tha != 0xFFFFFFFFu) ? nd500_bus_read32(g_machine, pa_tha) : 0xDEADBEEFu);
    }

    Nd500Cpu saved_cpu = *g_cpu;
    int saved_run  = g_machine->run_flag;
    int saved_stop = g_machine->stop_reason;

    /* Open a nested allocation scope. The sub-program's segments, page tables
     * and MON segments come from physical pages the caller does not own, so
     * BOTH domains stay resident at once and the sub-program cannot touch the
     * caller's image. Closing the scope after the run frees exactly what the
     * sub-program took.
     *
     * This replaces a copy of ALL of physical memory taken around every nested
     * run: the loader used to place every program at the same fixed physical
     * base, so a nested load overwrote the caller outright and the only way
     * back was to restore the whole machine. That is also why a sub-program
     * could only return results through FILES - anything it left in memory was
     * discarded by the restore. It no longer is. */
    void* seg_backup = nd500_segment_alloc_state_save(g_machine);

    /* And the C-side MMU tables (global PST + per-domain capabilities): the
     * nested DOM load overwrites PST entries the caller's domain still
     * references, so without this the restored caller translates its VAs
     * through the sub-program's page tables (wrong physical pages). */
    void* mmu_backup = nd500_mmu_state_save();

    g_uecom_nest++;

    int rc = ndlib_load_dom_header(path);
    if (rc == 0) rc = ndlib_load_dom_segments();
    uint32_t start_addr = 0;
    int loaded_domain = -1;
    if (rc == 0)
        rc = ndlib_dom_load_to_machine(g_machine, g_cpu, -1, NULL, NULL,
                                       &start_addr, &loaded_domain);
    if (rc != 0) {
        nd500_segment_alloc_state_restore(seg_backup);
        nd500_mmu_state_restore(mmu_backup);
        *g_cpu = saved_cpu;
        g_machine->run_flag  = saved_run;
        g_machine->stop_reason = saved_stop;
        g_uecom_nest--;
        mon_log(MON_LOG_WARN, "UECOM: failed to load '%s' (%s)", name, path);
        return 1;
    }

    mon_set_command_buffer(args ? args : "");

    /* Tag files the sub-program opens at a raised generation so its MON 0B LEAVE
     * closes only its OWN files and leaves the caller's (scratch, sources) open,
     * so the caller resumes cleanly (fixes NC's post-codegen stack overflow). */
    mon_file_table_push_generation();

    /* Nested execution loop - mirrors run_domain's, minus banners/console setup
     * (the console is already bridged by the outer run). Runs until the sub-
     * program issues MON 0B LEAVE (clears run_flag / sets STOP_MON_HALT). */
    g_machine->run_flag = 1;
    g_machine->stop_reason = STOP_NONE;
    uint64_t steps = 0;
    while (g_machine->run_flag) {
        if (steps >= SHELL_MAX_STEPS) break;
        int ok = nd500_cpu_step(g_cpu);
        steps++;
        if (g_machine->stop_reason == STOP_WAIT_INPUT) {
            if (shell_wait_input_resume()) {
                g_machine->stop_reason = STOP_NONE;
                g_machine->run_flag = 1;
                continue;
            }
            break;
        }
        if (g_machine->stop_reason == STOP_MON_HALT) break;
        if (!ok) break;
    }

    /* Sub-program done: drop back to the caller's file generation (its LEAVE
     * already closed only its own files). */
    mon_file_table_pop_generation();

    /* Restore the caller so it resumes right after its UECOM call: free the
     * sub-program's pages and drop its MMU entries, then reinstate the full CPU
     * context. The caller's own memory was never touched. */
    nd500_segment_alloc_state_restore(seg_backup);
    nd500_mmu_state_restore(mmu_backup);
    *g_cpu = saved_cpu;
    g_machine->run_flag  = saved_run;
    g_machine->stop_reason = saved_stop;
    g_uecom_nest--;

    if (getenv("ND500X_STODBG")) {
        uint32_t hv[3] = { 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu };
        for (int i = 0; i < 3; i++) {
            uint32_t pa = nd500_mmu_peek(g_cpu, g_cpu->TOS + (uint32_t)i * 4u);
            if (pa != 0xFFFFFFFFu) hv[i] = nd500_bus_read32(g_machine, pa);
        }
        fprintf(stderr, "[STODBG] UECOM post-restore '%s' nest=%d TOS=0x%08X "
                "MAXL=0x%08X STAH=0x%08X ENDH=0x%08X THA=0x%08X OTE1=0x%08X CED=%u\n",
                name, g_uecom_nest, g_cpu->TOS, hv[0], hv[1], hv[2],
                g_cpu->THA, g_cpu->OTE1, g_cpu->CED);
        uint32_t pa_hv  = nd500_mmu_peek(g_cpu, 0x08022B8Cu);
        uint32_t pa_tha = nd500_mmu_peek(g_cpu, 0x080235E8u);
        fprintf(stderr, "[STODBG]   probe post va08022B8C pa=0x%08X val=0x%08X | "
                "va080235E8 pa=0x%08X val=0x%08X\n",
                pa_hv,  (pa_hv  != 0xFFFFFFFFu) ? nd500_bus_read32(g_machine, pa_hv)  : 0xDEADBEEFu,
                pa_tha, (pa_tha != 0xFFFFFFFFu) ? nd500_bus_read32(g_machine, pa_tha) : 0xDEADBEEFu);
    }

    mon_log(MON_LOG_INFO, "UECOM: nested '%s' ran %llu instrs (domain %d)",
            name, (unsigned long long)steps, loaded_domain);
    return 0;
}

/* Load and execute a DOM, bridging its terminal I/O to the host console. */
/* args = the text typed after the program name (e.g. "TEST" in "NC TEST").
 * SINTRAN passes this to the program via the command buffer (MON 12B SETCM);
 * NC and other tools read their arguments (e.g. the source file) from it. */
static void run_domain(const char* name, const char* args) {
    char path[1024];
    int rc0 = resolve_domain(name, path, sizeof(path));
    if (rc0 == -1) {
        printf("AMBIGUOUS DOMAIN NAME\n");   /* SINTRAN 057 - refuse to guess */
        return;
    }
    if (rc0 != 1) {
        printf("NO SUCH COMMAND OR DOMAIN\n");
        return;
    }

    int rc = ndlib_load_dom_header(path);
    if (rc != 0) { printf("DOM load failed: %s\n", path); return; }
    if (ndlib_load_dom_segments() != 0) { printf("DOM segment load failed\n"); return; }

    /* Snapshot the physical-page allocator AND the MMU tables (PST +
     * capabilities). Every program run consumes fresh watermark pages
     * (bounded-DATA reserve + page tables + MON segments, ~1 MB+) that were
     * never reclaimed when the program exited - after ~11 runs in one session
     * the 16 MB machine was exhausted and every further load failed with
     * "DOM configuration failed". Restoring the allocator alone is not
     * enough: the dead run's PST entries still reference its pages, so
     * find_highest_used_pfn re-seeds the watermark ABOVE them and the leak
     * ratchets on. A run leaves nothing live behind (files are written back
     * at MON 0B LEAVE), so roll BOTH back when the program exits - exactly
     * what the nested UECOM path (shell_execute_command) has done all along. */
    void* seg_backup = nd500_segment_alloc_state_save(g_machine);
    void* mmu_backup = nd500_mmu_state_save();

    uint32_t start_addr = 0;
    int loaded_domain = -1;
    /* NULL log callback: keep the shell clean (no MMU/segment dump per run).
     * Set ND500X_LOADDBG=1 for the loader's own diagnostics on stderr. */
    static int loaddbg = -1;
    if (loaddbg < 0) { const char* e = getenv("ND500X_LOADDBG"); loaddbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
    rc = ndlib_dom_load_to_machine(g_machine, g_cpu, -1,
                                   loaddbg ? loaddbg_cb : NULL, NULL,
                                   &start_addr, &loaded_domain);
    if (rc != 0) {
        nd500_mmu_state_restore(mmu_backup);
        nd500_segment_alloc_state_restore(seg_backup);
        printf("DOM configuration failed\n");
        return;
    }

    /* Hand the typed arguments to the program the SINTRAN way. */
    mon_set_command_buffer(args ? args : "");

    printf("-- %s placed (domain %d, start 0x%08X)%s%s --\n", name, loaded_domain, g_cpu->PC,
           (args && *args) ? " args=" : "", (args && *args) ? args : "");

    /* Bridge terminal I/O to the active transport; run until the program exits.
     * Inside a MODE script keep the mode console so the program reads its input
     * (CHECK/GENERATE-CODE/EXIT...) from the same script stream, not the tty. */
    if (g_mode_active)     { /* keep g_mode_console */ }
    else if (g_use_telnet) mon_file_table_set_console(nd500x_telnet_console());
    else                   mon_install_stdio_console();

    /* Re-establish the always-open scratch (file 100 = SCRATCH64) before EACH
     * program. A program's MON 0B LEAVE runs close_all_for_exit -> file-table
     * reset, which closes EVERY open file including the scratch and never
     * reopens it. It is opened once at MON init, so only the FIRST program had
     * it; a SECOND program in the same session (a 2nd compile, or the linker
     * after NC) then failed at startup with SINTRAN error 132B (no file opened
     * with this number). Reopen it here if it is gone so every program starts
     * with a fresh scratch, exactly as the first one did. */
    if (mon_config_get_auto_scratch_64()) {
        OpenFileEntry* scr = mon_file_table_get(64 /* file 100 octal */);
        if (!scr || !scr->in_use) {
            mon_open_scratch_file("(SCRATCH)SCRATCH64", "DATA");
        }
    }

    g_machine->run_flag = 1;
    g_machine->stop_reason = STOP_NONE;
    uint64_t steps = 0;
    while (g_machine->run_flag) {
        if (steps >= SHELL_MAX_STEPS) { printf("\n-- step limit reached --\n"); break; }
        int ok = nd500_cpu_step(g_cpu);
        steps++;
        /* Asynchronous ESCAPE user-break: a compute-bound program never issues a
         * terminal read, so the INBT/DVINST escape check can't fire. Poll the
         * console every so often; if the user pressed ESCAPE and escape is
         * enabled on the terminal, abort back to the '@' prompt like real
         * SINTRAN's driver-level user-break. Poll interval keeps overhead
         * negligible while staying responsive. */
        if ((steps & 0x3FFF) == 0 && mon_console_poll_user_break()) {
            g_machine->run_flag = 0;
            g_machine->stop_reason = STOP_USER_REQUESTED;
            printf("\n-- aborted (ESCAPE user break) --\n");
            break;
        }
        /* Interactive terminal read with no input: the MON call suspended the
         * process (STOP_WAIT_INPUT) - block for the user's line, then resume and
         * retry the read. This MUST be checked regardless of cpu_step's return:
         * a suspended step returns "ok" while clearing run_flag, so checking it
         * only on the failure branch would let the program fall out and exit. */
        if (g_machine->stop_reason == STOP_WAIT_INPUT) {
            if (shell_wait_input_resume()) {
                g_machine->stop_reason = STOP_NONE;
                g_machine->run_flag = 1;
                continue;
            }
            break; /* no more input can ever arrive (EOF) */
        }
        if (g_machine->stop_reason == STOP_MON_HALT) break;
        if (!ok) break;
    }
    if (!g_use_telnet) restore_cooked();
    printf("\n-- program exited (%llu instructions) --\n", (unsigned long long)steps);

    /* Release the run's resources so a long session does not exhaust the
     * machine: restore the MMU tables (drops the dead run's PST entries and
     * capabilities - domain numbers are reused now, so stale entries must
     * not leak into the next occupant), reclaim its watermark pages, and
     * free the domain number. */
    nd500_mmu_state_restore(mmu_backup);
    nd500_segment_alloc_state_restore(seg_backup);
    if (loaded_domain > 0) {
        nd500_domain_free(g_cpu, (uint8_t)loaded_domain);
    }
}

/* --------------------------------------------------------------- commands */

typedef void (*cmd_fn)(int argc, char** argv);

static void cmd_help(int argc, char** argv);
static void cmd_login(int argc, char** argv);
static void cmd_logout(int argc, char** argv);
static void cmd_exit(int argc, char** argv);
static void cmd_list_files(int argc, char** argv);
static void cmd_set_term(int argc, char** argv);
static void cmd_get_term(int argc, char** argv);
static void cmd_recover_domain(int argc, char** argv);
static void cmd_create_file(int argc, char** argv);
static void cmd_type(int argc, char** argv);
static void cmd_edit(int argc, char** argv);
static void cmd_mode(int argc, char** argv);
static void cmd_delete_file(int argc, char** argv);
static void cmd_rename_file(int argc, char** argv);
static void cmd_copy_file(int argc, char** argv);
static void cmd_list_users(int argc, char** argv);
static void cmd_create_user(int argc, char** argv);

static const struct {
    const char* name;
    cmd_fn fn;
    int needs_login;
    const char* help;
} COMMANDS[] = {
    { "HELP",             cmd_help,           0, "HELP [<command>] - list commands" },
    { "LOGIN",            cmd_login,          0, "LOGIN <user> - log in (nd500x convenience)" },
    { "LOGOUT",           cmd_logout,         1, "LOGOUT - end the session" },
    { "EXIT",             cmd_exit,           0, "EXIT - leave the emulator" },
    { "LIST-FILES",       cmd_list_files,     1, "LIST-FILES [<pattern>] - list files" },
    { "SET-TERMINAL-TYPE",cmd_set_term,       0, "SET-TERMINAL-TYPE [<term>],<type> - set type; with no type, lists types from the VTM" },
    { "GET-TERMINAL-TYPE",cmd_get_term,       0, "GET-TERMINAL-TYPE - show current terminal type" },
    { "RECOVER-DOMAIN",   cmd_recover_domain, 1, "RECOVER-DOMAIN <name> - load and run a domain" },
    { "CREATE-FILE",      cmd_create_file,    1, "CREATE-FILE <name> - create an empty file (default type :DATA)" },
    { "TYPE",             cmd_type,           1, "TYPE <name>:<type> - copy a file's contents to the terminal" },
    { "EDIT",             cmd_edit,           1, "EDIT <name>:<type> - open the file in VS Code on the host" },
    { "MODE",             cmd_mode,           1, "MODE <file> - run a script of commands from <file>:MODE (SINTRAN @MODE)" },
    { "DELETE-FILE",      cmd_delete_file,    1, "DELETE-FILE <name>:<type> - delete a file" },
    { "RENAME-FILE",      cmd_rename_file,    1, "RENAME-FILE <old>,<new> - rename a file" },
    { "COPY-FILE",        cmd_copy_file,      1, "COPY-FILE <destination>,<source> - copy a file" },
    { "LIST-USERS",       cmd_list_users,     0, "LIST-USERS - list users (directories under the SINTRAN root)" },
    { "CREATE-USER",      cmd_create_user,    0, "CREATE-USER <name> - create a user directory" },
};
#define NCOMMANDS ((int)(sizeof(COMMANDS)/sizeof(COMMANDS[0])))

static void cmd_help(int argc, char** argv) {
    if (argc >= 2) {
        /* HELP <command>: list matching entries (abbreviation allowed). */
        char up[64]; snprintf(up, sizeof(up), "%s", argv[1]); str_upper(up);
        int found = 0;
        for (int i = 0; i < NCOMMANDS; i++) {
            if (cmd_matches(up, COMMANDS[i].name)) { printf("  %s\n", COMMANDS[i].help); found = 1; }
        }
        if (!found) printf("NO SUCH COMMAND OR DOMAIN\n");
        return;
    }
    printf("SINTRAN-flavoured shell (nd500x). Commands:\n");
    for (int i = 0; i < NCOMMANDS; i++) printf("  %s\n", COMMANDS[i].help);
    printf("Type a domain name to run it (RECOVER-DOMAIN). Commands may be abbreviated.\n");
}

static void cmd_login(int argc, char** argv) {
    const char* user = (argc >= 2) ? argv[1] : "SYSTEM";
    mon_config_set_current_user(user);          /* uppercases like SINTRAN */
    vtm_invalidate();                           /* VTM lookup depends on user dir */
    g_logged_in = 1;
    printf("User %s logged in.\n", mon_config_get_current_user());
}

static void cmd_logout(int argc, char** argv) {
    (void)argc; (void)argv;
    g_logged_in = 0;
    mon_config_set_current_user("SYSTEM");
    vtm_invalidate();
    printf("-- EXIT --\n");
}

static void cmd_exit(int argc, char** argv) {
    (void)argc; (void)argv;
    g_running = 0;
}

/* Convert a host basename NAME.TYPE to the SINTRAN spelling NAME:TYPE.
 * SINTRAN separates a file's name from its type with a colon; the host
 * filesystem uses a dot. Only the LAST dot is the type separator (SINTRAN
 * names themselves may contain '-' but not '.'). Names with no dot are
 * copied unchanged. */
static void host_to_sintran_name(const char* host, char* out, size_t n) {
    snprintf(out, n, "%s", host);
    char* dot = strrchr(out, '.');
    if (dot) *dot = ':';
}

static void list_dir(const char* label, const char* dir, const char* pattern) {
    DIR* d = opendir(dir);
    if (!d) return;
    printf("  (%s)\n", label);
    struct dirent* e;
    int n = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        /* Filter the SINTRAN way: COMPS-match the host name against the
         * NAME:TYPE pattern (e.g. ":DOM" lists only type-DOM files). */
        if (pattern && !mon_sintran_name_matches(pattern, e->d_name)) continue;
        char sname[280];
        host_to_sintran_name(e->d_name, sname, sizeof(sname));
        printf("    %3d  %s\n", ++n, sname);
    }
    closedir(d);
    if (n == 0) printf("    (no files)\n");
}

static void cmd_list_files(int argc, char** argv) {
    /* Optional SINTRAN file-spec argument filters the listing (NAME:TYPE, with
     * empty parts = "any"): e.g. LIST-FILES :DOM lists only type-DOM files,
     * LIST-FILES LINK* only names starting LINK. No argument lists everything. */
    const char* pattern = (argc >= 2) ? argv[1] : NULL;
    const char* root = mon_config_get_sintran_root();
    const char* user = mon_config_get_current_user();
    char dir[1024];
    if (!root || !*root) root = ".";
    snprintf(dir, sizeof(dir), "%s/%s", root, user && *user ? user : "SYSTEM");
    list_dir(user && *user ? user : "SYSTEM", dir, pattern);
    snprintf(dir, sizeof(dir), "%s/SYSTEM", root);
    if (!(user && strcasecmp(user, "SYSTEM") == 0)) list_dir("SYSTEM", dir, pattern);
}

static void cmd_set_term(int argc, char** argv) {
    /* SET-TERMINAL-TYPE <terminal number>,<terminal type>; the manual example is
     * ",,53" (default terminal, type 53). With no type given, list the available
     * terminal types (read from the VTM file). Otherwise take the LAST token as
     * the type so both "53" and ",,53" work. */
    if (argc < 2) { vtm_list(); return; }
    const char* last = argv[argc - 1];
    if (!isdigit((unsigned char)last[0])) {   /* e.g. "list" / "?" -> show list */
        vtm_list();
        return;
    }
    int type = (int)strtol(last, NULL, 0);
    /* Store into the emulator's per-terminal state (device 1 = own terminal),
     * the SAME place MON 17B MSTTY writes and MON 16B MGTTY reads - so a running
     * DOM that calls GET-TERMINAL-TYPE sees exactly what was set here. */
    mon_set_terminal_type(SHELL_TERM_DEVICE, type);
    int stored = (int)mon_get_terminal_type(SHELL_TERM_DEVICE);
    const char* name = vtm_name(stored);
    if (name) printf("Terminal type set to %d (%s).\n", stored, name);
    else      printf("Terminal type set to %d (not in the VTM list).\n", stored);
}

static void cmd_get_term(int argc, char** argv) {
    (void)argc; (void)argv;
    int type = (int)mon_get_terminal_type(SHELL_TERM_DEVICE);
    const char* name = vtm_name(type);
    if (name) printf("TERMINAL TYPE: %d (%s)\n", type, name);
    else      printf("TERMINAL TYPE: %d\n", type);
}

static void cmd_recover_domain(int argc, char** argv) {
    if (argc < 2) { printf("DOMAIN NAME MISSING\n"); return; }
    char args[512] = "";
    for (int i = 2; i < argc; i++) {
        if (args[0]) strncat(args, " ", sizeof args - strlen(args) - 1);
        strncat(args, argv[i], sizeof args - strlen(args) - 1);
    }
    run_domain(argv[1], args);
}

/* Map a SINTRAN file reference to a host path in the current user's directory.
 * Uppercases the name and turns NAME:TYPE into host NAME.TYPE; if no type is
 * given and deftype is set, appends it (SINTRAN's per-command default type). */
static void sh_basename(const char* name, const char* deftype, char* out, size_t n) {
    char up[160];
    snprintf(up, sizeof up, "%s", name);
    str_upper(up);
    char* colon = strchr(up, ':');
    if (colon) *colon = '.';
    if (!strchr(up, '.') && deftype && *deftype) snprintf(out, n, "%s.%s", up, deftype);
    else                                         snprintf(out, n, "%s", up);
}

static void sh_user_path(const char* name, const char* deftype, char* out, size_t n) {
    const char* root = mon_config_get_sintran_root();
    const char* user = mon_config_get_current_user();
    if (!root || !*root) root = ".";
    if (!user || !*user) user = "SYSTEM";
    char base[192];
    sh_basename(name, deftype, base, sizeof base);
    snprintf(out, n, "%s/%s/%s", root, user, base);
}

static void cmd_create_file(int argc, char** argv) {
    if (argc < 2) { printf("FILE NAME MISSING\n"); return; }
    char path[1024];
    sh_user_path(argv[1], "DATA", path, sizeof path);
    if (access(path, F_OK) == 0) { printf("FILE ALREADY EXISTS\n"); return; }
    FILE* f = fopen(path, "wb");
    if (!f) { printf("CANNOT CREATE FILE\n"); return; }
    fclose(f);
    printf("Created %s\n", path);
}

static void cmd_delete_file(int argc, char** argv) {
    if (argc < 2) { printf("FILE NAME MISSING\n"); return; }
    char path[1024];
    sh_user_path(argv[1], NULL, path, sizeof path);
    if (unlink(path) != 0) { printf("NO SUCH FILE NAME\n"); return; }
    printf("Deleted %s\n", argv[1]);
}

/* Resolve a SINTRAN file name to a host path, current user's directory first
 * then SYSTEM (so TYPE/EDIT reach shared files too). Returns 1 if the file
 * exists (out = the existing path); 0 if not (out = the current-user candidate,
 * so EDIT can still open it as a new file). */
static int sh_resolve_file(const char* name, char* out, size_t n) {
    sh_user_path(name, NULL, out, n);
    if (access(out, F_OK) == 0) return 1;
    const char* root = mon_config_get_sintran_root();
    if (!root || !*root) root = ".";
    char base[192];
    sh_basename(name, NULL, base, sizeof base);
    char sys[1024];
    snprintf(sys, sizeof sys, "%s/SYSTEM/%s", root, base);
    if (access(sys, F_OK) == 0) { snprintf(out, n, "%s", sys); return 1; }
    return 0;
}

/* TYPE <name>:<type> - copy a file's contents to the terminal (SINTRAN
 * COPY-TERMINAL, shortened). SINTRAN text files terminate lines with CR (0x0D);
 * translate CR / CRLF to LF so the host terminal renders them correctly. */
static void cmd_type(int argc, char** argv) {
    if (argc < 2) { printf("FILE NAME MISSING\n"); return; }
    char path[1024];
    if (!sh_resolve_file(argv[1], path, sizeof path)) { printf("NO SUCH FILE NAME\n"); return; }
    FILE* f = fopen(path, "rb");
    if (!f) { printf("NO SUCH FILE NAME\n"); return; }
    unsigned char buf[4096];
    char out[8193];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) {
        size_t o = 0;
        for (size_t i = 0; i < k; i++) {
            unsigned char c = buf[i];
            if (c == '\r') {
                out[o++] = '\n';
                if (i + 1 < k && buf[i + 1] == '\n') i++;  /* collapse CRLF */
            } else if (c == '\0') {
                continue;                                  /* skip NUL padding */
            } else {
                out[o++] = (char)c;
            }
        }
        out[o] = '\0';
        sh_puts(out);
    }
    fclose(f);
    sh_puts("\n");   /* leave the next @ prompt on its own line */
}

/* EDIT <name>:<type> - open the file in VS Code on the host. `code` hands the
 * path to the running VS Code instance and returns immediately; a non-existent
 * file opens as a new buffer that is created on first save. */
static void cmd_edit(int argc, char** argv) {
    if (argc < 2) { printf("FILE NAME MISSING\n"); return; }
    char path[1024];
    sh_resolve_file(argv[1], path, sizeof path);  /* out is set even when absent */

    /* Need `code` on PATH (VS Code + Remote-WSL). Check first so we can tell the
     * user instead of silently doing nothing. */
    if (system("command -v code >/dev/null 2>&1") != 0) {
        printf("'code' not found on PATH - install VS Code / the Remote-WSL 'code' shim.\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        /* Child: detach from the terminal and exec VS Code (no shell, so the
         * host path is passed verbatim - no injection from the file name). */
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) { dup2(devnull, 0); dup2(devnull, 1); dup2(devnull, 2); }
        execlp("code", "code", path, (char*)NULL);
        _exit(127);
    } else if (pid > 0) {
        printf("Opening %s in VS Code...\n", path);
    } else {
        printf("Could not launch editor (fork failed)\n");
    }
}

static void cmd_rename_file(int argc, char** argv) {
    if (argc < 3) { printf("USAGE: RENAME-FILE <old>,<new>\n"); return; }
    char op[1024], np[1024];
    sh_user_path(argv[1], NULL, op, sizeof op);
    sh_user_path(argv[2], NULL, np, sizeof np);
    if (rename(op, np) != 0) { printf("NO SUCH FILE NAME\n"); return; }
    printf("Renamed %s -> %s\n", argv[1], argv[2]);
}

static void cmd_copy_file(int argc, char** argv) {
    /* SINTRAN order is COPY-FILE <destination>,<source> (RefMan 2031). */
    if (argc < 3) { printf("USAGE: COPY-FILE <destination>,<source>\n"); return; }
    char dp[1024], sp[1024];
    sh_user_path(argv[1], NULL, dp, sizeof dp);
    sh_user_path(argv[2], NULL, sp, sizeof sp);
    FILE* in = fopen(sp, "rb");
    if (!in) { printf("NO SUCH FILE NAME\n"); return; }
    FILE* out = fopen(dp, "wb");
    if (!out) { fclose(in); printf("CANNOT CREATE FILE\n"); return; }
    char buf[8192];
    size_t k;
    while ((k = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, k, out);
    fclose(in); fclose(out);
    printf("Copied %s -> %s\n", argv[2], argv[1]);
}

static void cmd_list_users(int argc, char** argv) {
    (void)argc; (void)argv;
    const char* root = mon_config_get_sintran_root();
    if (!root || !*root) root = ".";
    DIR* d = opendir(root);
    if (!d) { printf("NO SUCH DIRECTORY\n"); return; }
    printf("Users (directories under %s):\n", root);
    struct dirent* e;
    int n = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", root, e->d_name);
        struct stat st;
        if (stat(p, &st) == 0 && S_ISDIR(st.st_mode)) printf("    %s\n", e->d_name);
        n++;
    }
    closedir(d);
    if (n == 0) printf("    (none)\n");
}

static void cmd_create_user(int argc, char** argv) {
    if (argc < 2) { printf("USER NAME MISSING\n"); return; }
    const char* root = mon_config_get_sintran_root();
    if (!root || !*root) root = ".";
    char up[128];
    snprintf(up, sizeof up, "%s", argv[1]);
    str_upper(up);
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", root, up);
    if (mkdir(p, 0755) != 0) { printf("USER ALREADY EXISTS OR CANNOT CREATE\n"); return; }
    printf("Created user %s\n", up);
}

/* ------------------------------------------------------------------- REPL */

static void dispatch(char* line) {
    char work[1024];
    snprintf(work, sizeof(work), "%s", line);
    char* tok[16];
    int argc = tokenise(work, tok, 16);
    if (argc == 0) return;

    char verb[64];
    snprintf(verb, sizeof(verb), "%s", tok[0]);
    str_upper(verb);

    /* Match against the command table under the abbreviation rule. */
    int match = -1, matches = 0;
    for (int i = 0; i < NCOMMANDS; i++) {
        if (cmd_matches(verb, COMMANDS[i].name)) { match = i; matches++; }
    }
    if (matches > 1) { printf("AMBIGUOUS COMMAND\n"); return; }
    if (matches == 1) {
        if (COMMANDS[match].needs_login && !g_logged_in) {
            printf("NOT LOGGED IN - use LOGIN <user>\n");
            return;
        }
        COMMANDS[match].fn(argc, tok);
        return;
    }
    /* No command matched: treat the verb as a domain name (RECOVER default).
     * Pass the original line's tail (after the program name) as the program's
     * arguments, preserving commas/spacing SINTRAN-style. */
    if (!g_logged_in) { printf("NOT LOGGED IN - use LOGIN <user>\n"); return; }
    const char* tail = line;
    while (*tail && *tail != ' ' && *tail != '\t' && *tail != ',') tail++;  /* skip name */
    while (*tail == ' ' || *tail == '\t' || *tail == ',') tail++;           /* skip seps */
    run_domain(tok[0], tail);
}

/* MODE <file> - run a script of shell commands from a mode file (SINTRAN @MODE).
 * The file may interleave shell commands with input consumed by programs it
 * starts; both read from the one script stream (see g_mode_console). Faithful to
 * real SINTRAN @MODE: NO parameters (one literal file per task, e.g.
 * COMPILE-HELLO:MODE), @CC comment lines, a leading @ herald is tolerated, EOF
 * ends the mode, nesting to depth 10. Default file type :MODE, then :SYMB (the
 * real SINTRAN default). */
static void cmd_mode(int argc, char** argv) {
    if (argc < 2) { printf("FILE NAME MISSING\n"); return; }
    if (g_mode_depth >= 10) { printf("MODE NESTING TOO DEEP (max 10)\n"); return; }

    /* Resolve the script to a host path: an explicit :type wins, else try
     * :MODE then :SYMB; current user's directory first, then SYSTEM. */
    char path[1024];
    int found = 0;
    if (strchr(argv[1], ':')) {
        found = sh_resolve_file(argv[1], path, sizeof path);
    } else {
        char nm[192];
        snprintf(nm, sizeof nm, "%s:MODE", argv[1]);
        found = sh_resolve_file(nm, path, sizeof path);
        if (!found) { snprintf(nm, sizeof nm, "%s:SYMB", argv[1]); found = sh_resolve_file(nm, path, sizeof path); }
    }
    if (!found) { printf("NO SUCH FILE NAME\n"); return; }

    FILE* f = fopen(path, "rb");
    if (!f) { printf("NO SUCH FILE NAME\n"); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 4 * 1024 * 1024) { fclose(f); printf("CANNOT READ FILE\n"); return; }
    char* raw = (char*)malloc((size_t)sz + 1);
    if (!raw) { fclose(f); printf("OUT OF MEMORY\n"); return; }
    size_t got = fread(raw, 1, (size_t)sz, f);
    fclose(f);
    raw[got] = '\0';

    /* Save the outer input state (supports nesting), install this script
     * VERBATIM - real SINTRAN mode files take no parameters. */
    ConsoleIO* prev_console = mon_file_table_get_console();
    char*  save_buf = g_mode_buf;
    size_t save_len = g_mode_len, save_pos = g_mode_pos;
    g_mode_buf = raw; g_mode_len = got; g_mode_pos = 0;
    mon_file_table_set_console(&g_mode_console);
    g_mode_active++; g_mode_depth++;

    /* Read shell command lines from the script and dispatch them; a line that
     * starts a program lets that program consume the following lines from the
     * same stream. */
    char line[2048];
    while (g_running) {
        int n = mode_getline(line, sizeof line);
        if (n < 0) break;                                /* end of script */
        char* s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\0') continue;                        /* blank line */
        if (*s == '@') s++;                              /* tolerate the @ herald */
        /* Comments: @CC <text> (SINTRAN), or a leading % / ; (convenience). */
        if (s[0] == '%' || s[0] == ';') continue;
        if ((s[0] == 'C' || s[0] == 'c') && (s[1] == 'C' || s[1] == 'c') &&
            (s[2] == ' ' || s[2] == '\t' || s[2] == '\0')) continue;
        printf("@%s\n", s);                              /* echo like SINTRAN does */
        char work[2048];
        snprintf(work, sizeof work, "%s", s);
        dispatch(work);
    }

    /* Restore the outer input state. */
    g_mode_active--; g_mode_depth--;
    mon_file_table_set_console(prev_console);
    free(raw);
    g_mode_buf = save_buf; g_mode_len = save_len; g_mode_pos = save_pos;
}

static char* shell_readline(const char* prompt) {
    if (g_use_telnet) {
        static char tbuf[1024];
        nd500x_telnet_write(prompt);
        int n = nd500x_telnet_readline(tbuf, sizeof tbuf);
        if (n < 0) return NULL;            /* client disconnected */
        return strdup(tbuf);
    }
#ifdef HAVE_READLINE
    char* line = readline(prompt);
    if (line && *line) add_history(line);
    return line;
#else
    static char buf[1024];
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    size_t n = strlen(buf);
    if (n && buf[n-1] == '\n') buf[n-1] = '\0';
    return strdup(buf);
#endif
}

int nd500x_shell_run(Nd500Machine* machine, Nd500Cpu* cpu, const char* script_path,
                     int telnet_port) {
    g_machine = machine;
    g_cpu = cpu;
    g_logged_in = 0;
    g_running = 1;

    /* Let MON 317B UECOM actually run nested programs (NC's CAT-500 back-end,
     * other passes) re-entrantly via this shell's DOM load+run path. */
    mon_set_execute_command(shell_execute_command);

    /* ND500X_MONLOG=1 enables MON-call tracing to stderr (debugging the shell)
     * and keeps the emulator's informational output; otherwise the shell runs
     * quiet (no DOM-load / MMU-enable / MON-halt notices cluttering the prompt). */
    if (getenv("ND500X_MONLOG")) {
        mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG);
    } else {
        nd500_quiet = 1;
        nd500_log_quiet = 1;
    }

    /* Telnet transport: start the server and wait for a client before the REPL.
     * Once connected, all shell I/O flows over the socket. */
    if (telnet_port > 0) {
        if (nd500x_telnet_start(telnet_port) != 0) return 1;
        fprintf(stderr, "Waiting for a telnet client on port %d...\n", telnet_port);
        if (!nd500x_telnet_wait_client()) { nd500x_telnet_stop(); return 1; }
        g_use_telnet = 1;
    }

    if (!g_use_telnet && isatty(STDIN_FILENO) &&
        tcgetattr(STDIN_FILENO, &g_cooked_termios) == 0) {
        g_have_cooked = 1;
    }

    /* Auto-log-in as the configured current user (from ini / --user, else the
     * SYSTEM default set by main). LOGIN <other> switches; LOGOUT drops out. */
    g_logged_in = 1;
    vtm_invalidate();
    printf("\nSINTRAN III (nd500x) - user %s. Type HELP for commands.\n",
           mon_config_get_current_user());
    printf("SINTRAN root: %s\n\n", mon_config_get_sintran_root());

    /* Optional scripted commands first (batch). */
    if (script_path) {
        FILE* f = fopen(script_path, "r");
        if (!f) {
            printf("Cannot open script: %s\n", script_path);
        } else {
            char line[1024];
            while (g_running && fgets(line, sizeof(line), f)) {
                size_t n = strlen(line);
                if (n && line[n-1] == '\n') line[n-1] = '\0';
                if (line[0] == '\0' || line[0] == '@') {
                    /* allow a leading '@' like the real prompt echo */
                    dispatch(line[0] == '@' ? line + 1 : line);
                } else {
                    dispatch(line);
                }
            }
            fclose(f);
        }
    }

    while (g_running) {
        char* line = shell_readline("@");
        if (!line) break;                 /* EOF */
        dispatch(line);
        free(line);
    }

    restore_cooked();
    if (g_use_telnet) nd500x_telnet_stop();
    return 0;
}
