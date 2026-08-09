#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#ifdef _WIN32
/* SHGetFolderPathA / CSIDL_PERSONAL, used by expand_tilde() below. */
#include <shlobj.h>
#endif
#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"
#include "../../cpu/nd500_mmu.h"
#include "../../cpu/nd500_domain.h"
#include "../../cpu/nd500_indirect.h"
#include "../../debugger/debugger.h"
#include "../../debugger/commands.h"
#include "nd500x_shell.h"
#include "nd500x_ndix.h"
#include "../../machine/nd500_ndix_boot.h"
#include "ndix_ffs.h"   /* --extract reads a file straight out of the image */
/* nd500_fecall_local_unit() - which guest tty this window is attached to, so
 * the telnet server can be told not to offer that one. */
#include "../../cpu/nd500_fecall.h"
#include "ndix_menu.h"
#include "uplink_tcp.h"   /* ND500X_ETH_UPLINK=tcp:host[:port] */
#include "../../ndlib/ndlib.h"
#include "../../ndlib/ndlib_color.h"
#include <ndmon/mon.h>
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_config.h>
#include <ndmon/mon_terminal_state.h>
#include "nd500_dom.h"

static void print_usage(const char* prog) {
    printf("ND-500 Emulator - nd500x\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --debug                  Enter interactive debugger REPL\n");
    printf("  --monitor                Enter the SINTRAN-flavoured shell (login, run domains)\n");
    printf("  --script <path>          Feed shell commands from a file (with --monitor)\n");
    printf("  --ndix <disk-image>      Boot the NDIX kernel with <disk-image> as root disk\n");
    printf("  --kernel <path>          NDIX kernel image for --ndix (default: see below)\n");
    printf("  --extract <in> <out>     Copy one file OUT of the --ndix image and exit.\n");
    printf("                           No boot, no guest. e.g. --extract /lib/crt0.o crt0.o\n");
    printf("  --telnet[=<port>] [<n>]  Serve terminals over TCP/telnet (default port 5000).\n");
    printf("                           <n> limits how many guest ttys are offered, in the\n");
    printf("                           order console, tty01, tty02, tty81 (default: all 4).\n");
    printf("                           With --ndix: the guest ttys. Otherwise: the SINTRAN shell.\n");
    printf("  --config <path>          Load settings from an ini file (else ./nd500x.ini)\n");
    printf("                           keys: sintran-root, user, terminal-type, telnet-port, monitor\n");
    printf("  -i <path>                Load a.out file (legacy)\n");
    printf("  --aout <path>            Load a.out file\n");
    printf("  --pseg <path>            Load PSEG binary (auto-detects kernel/user mode)\n");
    printf("  --dseg <path>            Load DSEG binary (auto-detects kernel/user mode)\n");
    printf("  --dom <path>             Load DOM/SEG file header\n");
    printf("  --mode <mode>            Override mode: kernel | user | auto\n");
    printf("                           (kernel: PSEG=0x08000000, DSEG=0x00000000)\n");
    printf("                           (user:   PSEG=0xD0000000, DSEG=0xF0000000)\n");
    printf("                           (auto:   detect from filename - 'user' in name)\n");
    printf("  --pc <addr>              Set starting PC address\n");
    printf("  --args <string>          Set command buffer (program arguments)\n");
    printf("  --disasm <len>           Disassemble <len> bytes and exit\n");
    printf("  --addr <addr>            Start address for disassembly (default: 0)\n");
    printf("  --hexdump <len>          Hex dump <len> bytes and exit\n");
    printf("  --radix <mode>           Set numeric radix: decimal | hex | octal\n");
    printf("  --run                    Run program (exit on MON 0B or error)\n");
    printf("  --dap [port]             Start DAP server (default port 4500) and wait for client\n");
    printf("  --max-steps <n>          Maximum instructions to execute (default: unlimited)\n");
    printf("  --trace-file <path>      Write instruction trace to file\n");
    printf("  --sintran-root <path>    Set SINTRAN file system root directory\n");
    printf("  --user <name>            Set current SINTRAN user (default: GUEST)\n");
    printf("  --no-scratch-64          Disable automatic scratch file 64\n");
    printf("  -ansi                    Force enable ANSI colors\n");
    printf("  -noansi                  Force disable ANSI colors\n");
    printf("  --help                   Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --aout kernel --debug\n", prog);
    printf("  %s --pseg kernel.pseg --dseg kernel.dseg --mode kernel --debug\n", prog);
    printf("  %s --pseg user_prog.pseg --mode user --debug\n", prog);
    printf("  %s --aout program --disasm 100 --addr 0x1000\n", prog);
    printf("  %s --dom program.dom --run --trace-file trace.txt\n", prog);
    printf("  %s --dom program.dom --run --max-steps 10000 --trace-file trace.txt\n", prog);
    printf("  %s --dom program.dom --args \"input.txt\" --run\n", prog);
    printf("\n");
    printf("SINTRAN shell (--monitor):\n");
    printf("  Programs are found under the SINTRAN root (--sintran-root, default '.'):\n");
    printf("      <root>/<USER>/<NAME>.DOM     (searched first, after you LOGIN)\n");
    printf("      <root>/SYSTEM/<NAME>.DOM     (searched next)\n");
    printf("  So put your .DOM files in <root>/SYSTEM/ or <root>/<USER>/ and run them by\n");
    printf("  name. With no --sintran-root, <root> is the directory you launch from.\n");
    printf("  %s --monitor --sintran-root ./sintran\n", prog);
    printf("  %s --monitor --sintran-root build/link_sandbox --script session.cmd\n", prog);
    printf("\n");
    printf("NDIX boot (--ndix):\n");
    printf("  %s --ndix <root-disk-image>\n", prog);
    printf("  %s --ndix <root-disk-image> --telnet\n", prog);
    printf("  Boots multiuser to a login prompt (root, no password). Typed lines go to\n");
    printf("  the guest console; a line starting with '~' goes to the debugger instead.\n");
    printf("  Press F12 for the emulator menu: virtual consoles, or exit NDIX.\n");
    printf("  The kernel image is looked up in this order:\n");
    printf("      --kernel <path>\n");
    printf("      $ND500X_KERNEL\n");
    printf("      /vmunix INSIDE the disk image        (so the image is all you need)\n");
    printf("      <root>/kernel/MASTER/GENERIC/vmunix\n");
    printf("      <root>/vmunix\n");
    printf("  where <root> is --sintran-root if given, else the directory holding the\n");
    printf("  disk image. A kernel you just rebuilt is NOT picked up until you copy it\n");
    printf("  into the image with nd500-mkproto - or pass --kernel.\n");
    printf("  Guest writes go THROUGH to the disk image and survive a restart. Set\n");
    printf("  ND500X_DISK_RW=cow for a scratch session, or =0 for read-only.\n");
    printf("  --ndix only sets DEFAULTS for ND500X_DISK, ND500X_DISK_RW,\n");
    printf("  ND500X_MMU_GUEST_TABLES, ND500X_NOXMSG and ND500X_CONSOLE_STDIN - any of\n");
    printf("  them exported beforehand still wins.\n");
    printf("\n");
}

/* Trim leading/trailing ASCII whitespace in place; returns the trimmed start. */
static char* ini_trim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\r'||e[-1]=='\n')) *--e = '\0';
    return s;
}

/* Where a leading "~" points on THIS platform.
 *
 * On Unix that is $HOME, the usual thing.
 *
 * On Windows it is the DOCUMENTS folder, not the user profile. That is a
 * deliberate difference: the things a '~' path names here (SINTRAN user
 * directories, disk images, configs) are the user's own documents, and
 * dropping them straight into C:\Users\<name> is not where a Windows user
 * expects to find their files.
 *
 * Documents is asked for through the shell API rather than built as
 * "%USERPROFILE%\Documents", because that guess is wrong on a lot of machines.
 * Measured on this box (2026-08-06):
 *
 *   HKCU\...\Explorer\User Shell Folders  Personal = C:\Users\ronny\OneDrive\Documents
 *
 * i.e. Documents is redirected into OneDrive, and the string-concatenation
 * version would have pointed at a directory that is not the user's Documents
 * folder at all. SHGetFolderPath reads that same redirection and gets it right.
 *
 * $HOME is NOT consulted first on Windows: under MSYS/Git-Bash it holds a POSIX
 * path like /c/Users/ronny, which a native Windows program cannot open.
 *
 * Returns NULL if no directory could be determined; 'out' otherwise. SHGFP_TYPE_CURRENT
 * asks for the path in effect now rather than the registry default. */
static const char* platform_home_dir(char* out, size_t out_size) {
#ifdef _WIN32
    char docs[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL,
                                   SHGFP_TYPE_CURRENT, docs))
        && docs[0]) {
        if ((size_t)snprintf(out, out_size, "%s", docs) < out_size) return out;
        return NULL;                    /* longer than the caller's buffer */
    }
    /* Last resort if the shell API is unavailable: the profile's Documents.
     * Wrong under folder redirection, but better than expanding to nothing. */
    {
        const char* prof = getenv("USERPROFILE");
        if (prof && prof[0]
            && (size_t)snprintf(out, out_size, "%s\\Documents", prof) < out_size)
            return out;
    }
    return NULL;
#else
    const char* home = getenv("HOME");
    if (!home || !home[0]) return NULL;
    if ((size_t)snprintf(out, out_size, "%s", home) >= out_size) return NULL;
    return out;
#endif
}

/* Expand a leading "~/" (or a bare "~") to the user's home directory
 * (on Windows: the Documents folder - see platform_home_dir()).
 *
 * The ini file is read with fopen/fgets, so no shell ever sees its values. On
 * the command line "--sintran-root ~/ND500USERS" works only because bash
 * expands the tilde first; written in the ini the same string arrives here
 * literally, and mon_config_set_sintran_root() (mon_config.c) just strncpy's
 * whatever it is given. mon_ensure_directory() (mon_path.c) would then mkdir a
 * directory literally NAMED "~" in the current working directory, which is how
 * SINTRAN user dirs ended up inside the repository.
 *
 * A '~' anywhere but the first character is left alone. So is the "~user" form,
 * which would need getpwnam and is not what anyone writes here.
 *
 * Returns 'out' when it expanded, else 'in' unchanged. Every case where an
 * expansion was clearly wanted but could not be done warns, because failing
 * silently means falling back to the literal-"~" bug this exists to prevent. */
static const char* expand_tilde(const char* in, char* out, size_t out_size) {
    if (!in || in[0] != '~') return in;
    if (in[1] != '\0' && in[1] != '/' && in[1] != '\\') {
        fprintf(stderr, "warning: '%s': the ~user form is not supported, "
                        "using it literally. Write an absolute path.\n", in);
        return in;
    }
    char homebuf[PATH_MAX];
    const char* home = platform_home_dir(homebuf, sizeof homebuf);
    if (!home) {
        fprintf(stderr, "warning: '%s': could not work out %s, "
                        "using it literally. Write an absolute path.\n",
#ifdef _WIN32
                in, "your Documents folder");
#else
                in, "your home directory");
#endif
        return in;
    }
    int n = snprintf(out, out_size, "%s%s", home, in + 1);
    if (n < 0 || (size_t)n >= out_size) {
        fprintf(stderr, "warning: '%s' expands to more than %zu characters, "
                        "using it literally. Write a shorter absolute path.\n",
                in, out_size - 1);
        return in;
    }
    return out;
}

/* Load an ini/config file: simple "key = value" lines, '#'/';' comments,
 * '[section]' lines ignored. Recognised keys: sintran-root, user,
 * terminal-type, telnet-port, monitor. Command-line flags override these
 * because the ini is applied before the argument loop. Returns 0 if loaded. */
static int load_config(const char* path, int* telnet_port, int* monitor_mode, int* user_set) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char* p = ini_trim(line);
        if (*p == '\0' || *p == '#' || *p == ';' || *p == '[') continue;
        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = ini_trim(p);
        char* val = ini_trim(eq + 1);
        /* 512 matches MAX_ROOT_PATH in mon_config.c, so anything that would be
         * truncated there is caught here with a warning instead. */
        char rootbuf[512];
        if (strcasecmp(key, "sintran-root") == 0)      mon_config_set_sintran_root(expand_tilde(val, rootbuf, sizeof rootbuf));
        else if (strcasecmp(key, "user") == 0)       { mon_config_set_current_user(val); if (user_set) *user_set = 1; }
        else if (strcasecmp(key, "terminal-type") == 0) mon_set_terminal_type(1, atoi(val));
        else if (strcasecmp(key, "telnet-port") == 0) { *telnet_port = atoi(val); *monitor_mode = 1; }
        else if (strcasecmp(key, "monitor") == 0)      { if (atoi(val)) *monitor_mode = 1; }
    }
    fclose(f);
    fprintf(stderr, "Loaded config: %s\n", path);
    return 0;
}

/* Adapter: nd500x_ndix_autoboot emits debugger commands without needing to know
 * the CmdContext type. */
static int ndix_autoboot_run(Nd500Machine* m, const char* cmd, void* ctx) {
	printf("[ndix] %s\n", cmd);
	return nd500_cmd_execute(m, cmd, (CmdContext*)ctx);
}

int main(int argc, char** argv) {
    /* No arguments: show the full usage rather than starting headless. */
    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    }

    int debug = 0;
    const char* input_path = NULL;
    const char* aout_path = NULL;
    const char* pseg_path = NULL;
    const char* dseg_path = NULL;
    const char* dom_path = NULL;
    const char* mode_str = NULL; /* "kernel" or "user" */
    const char* args_str = NULL; /* Command buffer / program arguments */
    uint32_t start_pc = 0;
    int has_start_pc = 0;
    uint32_t dis_len = 0;
    uint32_t dis_addr = 0;
    uint32_t hex_len = 0;
    int ansi_flag = 0; /* 0=auto, 1=force-enable, -1=force-disable */
    const char* radix_str = NULL;
    int run_mode = 0;  /* Non-interactive run */
    int monitor_mode = 0;  /* SINTRAN-flavoured interactive shell */
    const char* script_path = NULL;  /* optional shell command script */
    int telnet_port = 0;   /* >0: serve terminals over TCP/telnet */
    int telnet_ttys = 0;   /* how many guest ttys to offer; 0 = all of them */
    const char* ndix_image = NULL;   /* --ndix root disk image */
    /* --extract <path-in-image> <host-file>: copy one file out of the image and
     * exit, without booting anything. */
    const char* extract_guest = NULL;
    const char* extract_host  = NULL;
    const char* ndix_kernel = NULL;  /* --kernel override */
    const char* sintran_root_opt = NULL;  /* --sintran-root as typed */
    char sintran_root_buf[512];           /* backs sintran_root_opt if '~' expanded */
    uint64_t max_steps = 0;  /* 0 = unlimited */
    const char* trace_file_path = NULL;
    int dap_port = 0;  /* 0 = DAP server not requested */

    /* Load a config file BEFORE parsing flags so command-line flags override it.
     * Use --config <path> if given, else ./nd500x.ini if present. */
    const char* config_path = NULL;
    int user_set = 0;   /* did ini or --user set an explicit user? */
    for (int i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "--config") == 0) { config_path = argv[i + 1]; break; }
    }
    if (config_path) load_config(config_path, &telnet_port, &monitor_mode, &user_set);
    else             load_config("./nd500x.ini", &telnet_port, &monitor_mode, &user_set);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            input_path = argv[++i];
        } else if (strcmp(argv[i], "--aout") == 0 && i + 1 < argc) {
            aout_path = argv[++i];
        } else if (strcmp(argv[i], "--pseg") == 0 && i + 1 < argc) {
            pseg_path = argv[++i];
        } else if (strcmp(argv[i], "--dseg") == 0 && i + 1 < argc) {
            dseg_path = argv[++i];
        } else if (strcmp(argv[i], "--dom") == 0 && i + 1 < argc) {
            dom_path = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode_str = argv[++i];
        } else if (strcmp(argv[i], "--args") == 0 && i + 1 < argc) {
            args_str = argv[++i];
        } else if (strcmp(argv[i], "--pc") == 0 && i + 1 < argc) {
            start_pc = (uint32_t)strtoul(argv[++i], NULL, 0);
            has_start_pc = 1;
        } else if (strcmp(argv[i], "--disasm") == 0 && i + 1 < argc) {
            dis_len = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc) {
            dis_addr = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--hexdump") == 0 && i + 1 < argc) {
            hex_len = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--radix") == 0 && i + 1 < argc) {
            radix_str = argv[++i];
        } else if (strcmp(argv[i], "-ansi") == 0) {
            ansi_flag = 1;
        } else if (strcmp(argv[i], "-noansi") == 0) {
            ansi_flag = -1;
        } else if (strcmp(argv[i], "--dap") == 0) {
            /* Optional port argument (default 4711) */
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                dap_port = atoi(argv[++i]);
            }
            if (dap_port <= 0) dap_port = 4500;
        } else if (strcmp(argv[i], "--monitor") == 0 || strcmp(argv[i], "--shell") == 0) {
            monitor_mode = 1;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script_path = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            ++i;   /* already handled in the pre-parse pass above */
        } else if (strcmp(argv[i], "--telnet") == 0 ||
                   strncmp(argv[i], "--telnet=", 9) == 0) {
            /* Accepted forms, all meaning "serve terminals over telnet":
             *   --telnet                  default port 5000, every tty
             *   --telnet=<port>           explicit port, every tty
             *   --telnet <port>           explicit port (the original spelling)
             *   --telnet <port> <count>   also limit how many ttys are offered
             * 5000 avoids the DAP ports (4500 here, 4711 in nd100x). The count
             * is only read after an explicit port, so a lone numeric argument
             * stays the port it has always been. */
            const char* eq = strchr(argv[i], '=');
            if (eq) {
                telnet_port = atoi(eq + 1);
            } else if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
                telnet_port = atoi(argv[++i]);
            }
            /* The count follows the port in both spellings. Nothing else in this
             * parser takes a bare numeric argument, so consuming one here cannot
             * steal it from another option. */
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                telnet_ttys = atoi(argv[++i]);
            if (telnet_port <= 0) telnet_port = 5000;
        } else if (strcmp(argv[i], "--ndix") == 0 && i + 1 < argc) {
            ndix_image = argv[++i];
        } else if (strcmp(argv[i], "--extract") == 0 && i + 2 < argc) {
            extract_guest = argv[++i];
            extract_host  = argv[++i];
        } else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
            ndix_kernel = argv[++i];
        } else if (strcmp(argv[i], "--run") == 0) {
            run_mode = 1;
        } else if (strcmp(argv[i], "--max-steps") == 0 && i + 1 < argc) {
            max_steps = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--trace-file") == 0 && i + 1 < argc) {
            trace_file_path = argv[++i];
        } else if (strcmp(argv[i], "--sintran-root") == 0 && i + 1 < argc) {
            /* Normally the shell has already expanded any '~' here, but not if
             * it was quoted. Expanding before --ndix sees it too (it realpath's
             * this value) keeps the flag and the ini key behaving alike. */
            sintran_root_opt = expand_tilde(argv[++i], sintran_root_buf, sizeof sintran_root_buf);
            mon_config_set_sintran_root(sintran_root_opt);
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            mon_config_set_current_user(argv[++i]);
            user_set = 1;
        } else if (strcmp(argv[i], "--no-scratch-64") == 0) {
            mon_config_set_auto_scratch_64(0);
        }
    }

    /* --ndix: resolve paths, export the boot environment defaults and move to
     * the kernel directory. Must run BEFORE the machine is built and before
     * anything reads ND500X_*. It implies the debugger REPL, which is what
     * drives the boot script and carries console input. */
    /* --extract: pull one file out of the disk image and stop. No machine, no
     * boot, no guest.
     *
     * The emulator has always been able to do this - it is how the kernel is
     * taken out of /vmunix at startup (extract_kernel() in nd500x_ndix.c) - but
     * the ability was not reachable from outside. That mattered: /lib/crt0.o
     * and /lib/libc.a exist ONLY inside the image, so rebuilding any part of
     * the userland meant first booting the guest and reading them out through a
     * terminal. One flag turns a filesystem the host could only look at into
     * one it can take things from.
     *
     * Deliberately read-only: it copies out, never in. */
    if (extract_guest) {
        long n = 0;
        const char* why = "";
        uint8_t* data;
        FILE* out;

        if (!ndix_image) {
            fprintf(stderr, "error: --extract needs --ndix <image> as well\n");
            return 1;
        }
        data = ndix_ffs_read_file(ndix_image, extract_guest, &n, &why);
        if (!data) {
            fprintf(stderr, "error: cannot read %s from %s: %s\n",
                    extract_guest, ndix_image, why && why[0] ? why : "not found");
            return 1;
        }
        out = fopen(extract_host, "wb");
        if (!out) {
            perror(extract_host);
            free(data);
            return 1;
        }
        if (n > 0 && fwrite(data, 1, (size_t)n, out) != (size_t)n) {
            perror(extract_host);
            fclose(out);
            free(data);
            return 1;
        }
        fclose(out);
        free(data);
        printf("extracted %s -> %s (%ld bytes)\n", extract_guest, extract_host, n);
        return 0;
    }

    char ndix_load_cmd[PATH_MAX + 8];
    if (ndix_image) {
        if (nd500x_ndix_setup(ndix_image, ndix_kernel, sintran_root_opt,
                              ndix_load_cmd, (int)sizeof ndix_load_cmd) != 0) {
            return 1;
        }
        debug = 1;
        monitor_mode = 0;
    } else if (telnet_port > 0) {
        /* Without --ndix, telnet still means what it always did: serve the
         * SINTRAN shell. (An ini telnet-port already set monitor_mode.) */
        monitor_mode = 1;
    }

    /* Initialize color system based on flags */
    ndlib_color_init(ansi_flag);

    /* Set radix if specified */
    if (radix_str) {
        if (strcasecmp(radix_str, "decimal") == 0 || strcasecmp(radix_str, "dec") == 0) {
            nd500_dbg_set_radix(0);
        } else if (strcasecmp(radix_str, "hex") == 0) {
            nd500_dbg_set_radix(1);
        } else if (strcasecmp(radix_str, "octal") == 0 || strcasecmp(radix_str, "oct") == 0) {
            nd500_dbg_set_radix(2);
        } else {
            printf("Invalid radix: %s (use: decimal, hex, octal)\n", radix_str);
            return 1;
        }
    }

    /* Set up trace file if requested */
    if (trace_file_path) {
        if (nd500_dbg_set_trace_file(trace_file_path) != 0) {
            return 1;  /* Error opening file already printed */
        }
        printf("Trace output: %s\n", trace_file_path);
    }

    Nd500Machine machine;
    /* Initialize with 16MB of physical memory
     * Virtual addresses are mapped to physical via MMU */
    nd500_machine_init(&machine, 16 * 1024 * 1024);
	Nd500Cpu cpu;
	nd500_cpu_init(&cpu, &machine);
	nd500_cpu_reset(&cpu);

	/* Initialize SINTRAN MON call emulation */
	mon_init();

	/* Set command buffer if --args was specified */
	if (args_str) {
	    mon_set_command_buffer(args_str);
	    printf("Command buffer: \"%s\"\n", args_str);
	}

	uint32_t text_size = 0;
    if (input_path) {
		uint32_t entry = 0, pc = 0;
		/* Use unified loading (auto-loads .map and .s files) */
		if (ndlib_load_aout_with_debug(&machine, input_path, 1, &entry, &pc) == 0) {
			printf("loaded: %s (entry=0x%08X, PC=0x%08X)\n", input_path, entry, pc);
			(void)ndlib_aout_dump_metadata(input_path);
			/* Setup segment 31 (SINTRAN MON calls) as indirect segment
			 * This is normally done by SINTRAN firmware before jumping to kernel */
			nd500_setup_sintran_segment(&cpu, 0);  /* domain 0 = kernel domain */
		} else {
			printf("load failed: %s\n", input_path);
		}
	}

    if (aout_path) {
        /* Look for initialization script BEFORE loading the aout file
         * This allows the script to configure MMU before data is loaded */
        char init_path[512];
        strncpy(init_path, aout_path, sizeof(init_path) - 1);
        init_path[sizeof(init_path) - 1] = '\0';

        /* Find extension or end of string */
        char* ext = strrchr(init_path, '.');
        if (ext && (strcmp(ext, ".o") == 0 || strcmp(ext, ".out") == 0)) {
            /* Replace .o or .out with .init */
            strcpy(ext, ".init");
        } else {
            /* No extension - append .init to basename (e.g., kernel → kernel.init) */
            strncat(init_path, ".init", sizeof(init_path) - strlen(init_path) - 1);
        }

        /* Execute init script BEFORE loading aout (ignore errors - script is optional) */
        nd500_execute_init_script(&machine, init_path);

        /* Now load the aout file with MMU potentially configured */
        uint32_t entry = 0, pc = 0;
		/* Use unified loading (auto-loads .map and .s files) */
        if (ndlib_load_aout_with_debug(&machine, aout_path, 1, &entry, &pc) == 0) {
            printf("loaded: %s (entry=0x%08X, PC=0x%08X)\n", aout_path, entry, pc);
            (void)ndlib_aout_dump_metadata(aout_path);
            if (has_start_pc) cpu.PC = start_pc;  /* Override PC if specified */
            /* Setup segment 31 (SINTRAN MON calls) as indirect segment
             * This is normally done by SINTRAN firmware before jumping to kernel */
            nd500_setup_sintran_segment(&cpu, 0);  /* domain 0 = kernel domain */
            /* Enable MMU (required for indirect calls to work) */
            machine.mmu_enabled = 1;
            /* EXPERIMENT: enable data MMU so kernel data/stack segment (0xE8000000,
             * segment 29) is translated + demand-backed instead of falling through
             * to identity beyond physical RAM. */
            nd500_mmu_init(&cpu);
            nd500_mmu_enable_data(&cpu);
        } else {
            printf("load failed: %s\n", aout_path);
        }
    }

    if (pseg_path || dseg_path) {
        int kernel_mode = 1;
        int mode_specified = 0;

        if (mode_str) {
            if (strcasecmp(mode_str, "kernel") == 0) {
                kernel_mode = 1;
                mode_specified = 1;
            } else if (strcasecmp(mode_str, "user") == 0) {
                kernel_mode = 0;
                mode_specified = 1;
            } else if (strcasecmp(mode_str, "auto") == 0) {
                /* Auto-detect based on filename */
                mode_specified = 1;
                if (pseg_path && strstr(pseg_path, "user")) kernel_mode = 0;
                else if (dseg_path && strstr(dseg_path, "user")) kernel_mode = 0;
                else kernel_mode = 1;
            }
        }

        /* Auto-detect if not specified */
        if (!mode_specified) {
            /* Default: kernel mode unless filename contains "user" */
            if (pseg_path && strstr(pseg_path, "user")) kernel_mode = 0;
            else if (dseg_path && strstr(dseg_path, "user")) kernel_mode = 0;
            else kernel_mode = 1;
        }

        const char* mode_name = kernel_mode ? "kernel" : "user";
        uint32_t pseg_base = kernel_mode ? 0x08000000u : 0xD0000000u;
        uint32_t dseg_base = kernel_mode ? 0x00000000u : 0xF0000000u;

        if (pseg_path) {
            int rc = nd500_load_pseg_file(&machine, pseg_path, pseg_base);
            if (rc == 0) {
                printf("PSEG loaded at 0x%08X from %s (%s mode)\n", pseg_base, pseg_path, mode_name);
            } else {
                printf("PSEG load failed: %s\n", pseg_path);
                printf("Error: %s\n", nd500_load_strerror(rc, pseg_base, machine.memory_size));
            }
        }
        if (dseg_path) {
            int rc = nd500_load_dseg_file(&machine, dseg_path, dseg_base);
            if (rc == 0) {
                printf("DSEG loaded at 0x%08X from %s (%s mode)\n", dseg_base, dseg_path, mode_name);
            } else {
                printf("DSEG load failed: %s\n", dseg_path);
                printf("Error: %s\n", nd500_load_strerror(rc, dseg_base, machine.memory_size));
            }
        }
        if (has_start_pc) cpu.PC = start_pc; else if (pseg_path) cpu.PC = pseg_base;
    }

    if (dom_path) {
        int rc = ndlib_load_dom_header(dom_path);
        if (rc != 0) {
            if (rc == -1) {
                printf("DOM load failed: %s (file not found)\n", dom_path);
            } else if (rc == -2) {
                printf("DOM load failed: %s (file too small?)\n", dom_path);
            } else {
                printf("DOM load failed: %s (error %d)\n", dom_path, rc);
            }
        } else {
            /* Load segments into memory */
            rc = ndlib_load_dom_segments();
            if (rc != 0) {
                printf("DOM segment load failed: %s\n", dom_path);
            } else {
                /* Load segments to machine and configure MMU/domain system
                 * Pass -1 to auto-allocate domain (will get domain 1-255) */
                uint32_t start_addr = 0;
                int loaded_domain = -1;
                rc = ndlib_dom_load_to_machine(&machine, &cpu, -1, ndlib_dom_log_printf, NULL, &start_addr, &loaded_domain);
                if (rc != 0) {
                    printf("DOM configuration failed: %s\n", dom_path);
                } else {
                    /* Override PC if user specified --pc flag */
                    if (has_start_pc) {
                        cpu.PC = start_pc;
                    }
                    printf("DOM loaded: %s (start=0x%08X, domain=%d, MMU enabled)\n", dom_path, cpu.PC, loaded_domain);
                }
            }
        }
    }

    /* Start DAP server if requested (runs on a background thread) */
    if (dap_port > 0) {
#ifdef DAP_ENABLED
        /* Install stdio console so MON call output is visible */
        mon_install_stdio_console();
        if (nd500_dap_start(&machine, dap_port) != 0) {
            printf("Failed to start DAP server on port %d\n", dap_port);
            return 1;
        }
        printf("DAP server listening on port %d\n", dap_port);
        if (!debug) {
            /* Headless: keep serving until the DAP thread exits */
            while (nd500_dap_is_active()) {
                struct timespec ts = {0, 100000000}; /* 100ms */
                nanosleep(&ts, NULL);
            }
            nd500_machine_free(&machine);
            return 0;
        }
#else
        printf("DAP not available (built without libdap)\n");
        return 1;
#endif
    }

	if (debug) {
		/* F12 belongs to the emulator, not to the guest. Installed only on the
		 * --ndix path: the menu's entries (virtual consoles, shut NDIX down)
		 * only mean anything when there is an NDIX guest running. */
		if (ndix_image)
			nd500_debugger_set_guest_key_handler(ndix_menu_guest_key);

		/* Guest terminals over telnet. Started before the boot script so the
		 * very first console bytes reach an already-connected client; the
		 * local stdio console keeps its copy of unit 0 either way, so nothing
		 * is lost whether or not anyone connects. */
		if (ndix_image && telnet_port > 0) {
			/* A busy port must not stop the machine booting: the local
			 * console is still perfectly usable, and refusing to run because
			 * something else holds the port (often a previous run that has
			 * not exited yet) is worse than losing the telnet listener. Warn
			 * loudly and carry on. */
			if (nd500x_ndix_telnet_start(telnet_port, telnet_ttys) != 0)
				fprintf(stderr, "[telnet] continuing without the telnet server "
				        "- local console only\n");
			else {
				/* Claim the line this window is attached to before any client
				 * can connect, so the console is not offered in the telnet menu
				 * and then shared with the local terminal. */
				nd500x_ndix_telnet_mark_local(nd500_fecall_local_unit());
				nd500_debugger_set_stdin_eof_quiet(1);
			}
		}
		/* In --ndix mode the user is talking to NDIX, not to the debugger -
		 * the REPL is only here because the boot sequence runs through it.
		 * Silence its banner and its per-read PC prompt so guest output is
		 * not interleaved with emulator chrome. '~' still reaches the
		 * debugger. */
		if (ndix_image) nd500_debugger_set_quiet_banner(1);
		/* --ndix: boot the kernel exactly as the old shell wrapper did -
		 * "load <kernel>" (which auto-sources <kernel>.init) then "run" -
		 * while stdin stays on the terminal for guest console input. */
		if (ndix_image) {
			CmdContext bctx = {0};
			if (nd500x_ndix_autoboot_needed()) {
				/* No <kernel>.init beside the kernel: do the setup ourselves,
				 * deriving the load addresses from the .pseg/.dseg files. An
				 * .init, when present, still wins via `load` auto-sourcing it. */
				if (nd500x_ndix_autoboot(&machine, ndix_autoboot_run, &bctx) != 0)
					return 1;
			} else {
				printf("[ndix] %s\n", ndix_load_cmd);
				nd500_cmd_execute(&machine, ndix_load_cmd, &bctx);
			}
			/* proc0's kernel-stack/u-area segment (segment 29, _u at
			 * 0xE8000000). machdep.c:181-193 derives the twelve well-known
			 * kernel segment indices but never assigns Pst[stackindex] - it
			 * assumes an ND-100 bootstrap already filled them - and
			 * init_main.c:72 then reads that slot to build proc0's p_p0br.
			 * There is no ND-100 here, so nd500x builds it.
			 *
			 * Deliberately on BOTH boot routes, not inside autoboot: the
			 * <kernel>.init route is the one that actually runs for the
			 * shipped kernel (vmunix.init exists beside it), and it needs
			 * this just as much. Runs after either route has set PSTP and
			 * loaded the image, and before the guest first touches
			 * 0xE8000000. */
			/* The library function, not the debugger command: the work is
			 * the same one implementation, and this way the step does not
			 * need a command table to exist. 0/0 = the default addresses. */
			nd500_ndix_uarea(&machine, 0, 0);

			/* Dial the ethernet relay, if ND500X_ETH_UPLINK names one.
			 * AFTER the boot path, because that is what installs the
			 * loopback uplink when the setting says "loop" - a TCP spec
			 * takes it from there. Before "run", so the link is up before
			 * the guest's first frame.
			 *
			 * The return value is deliberately ignored: a relay that is not
			 * running must not stop the machine booting. It says so on
			 * stderr and et0 comes up with nothing on the other end, which
			 * is what it had before any of this existed. */
			(void)nd500x_uplink_tcp_start(&cpu);

			printf("[ndix] run\n");
			nd500_cmd_execute(&machine, "run", &bctx);
		}
		/* --script with --debug: execute each line as a debugger command
		 * BEFORE the interactive REPL (e.g. "load vmunix" + "run"), so a
		 * boot script can keep stdin connected to the real terminal for
		 * guest console input instead of feeding commands through a
		 * fragile printf|cat pipe. */
		if (script_path) {
			FILE* sf = fopen(script_path, "r");
			if (!sf) {
				fprintf(stderr, "error: cannot open --script file %s\n", script_path);
				return 1;
			}
			char sline[256];
			CmdContext sctx = {0};
			while (fgets(sline, sizeof(sline), sf)) {
				sline[strcspn(sline, "\r\n")] = '\0';
				if (!sline[0] || sline[0] == '#') continue;
				printf("[script] %s\n", sline);
				nd500_cmd_execute(&machine, sline, &sctx);
			}
			fclose(sf);
		}
		int rc = nd500_debugger_repl(&machine);
		nd500x_ndix_telnet_stop();
		return rc;
	}

	if (monitor_mode) {
		/* Spec decision: default shell user is SYSTEM unless ini/--user set one. */
		if (!user_set) mon_config_set_current_user("SYSTEM");
		return nd500x_shell_run(&machine, &cpu, script_path, telnet_port);
	}

    /* Non-interactive run mode */
    if (run_mode) {
        /* Install stdio console for interactive I/O */
        mon_install_stdio_console();

        machine.run_flag = 1;
        machine.stop_reason = STOP_NONE;

        uint64_t steps = 0;
        uint32_t last_pc = cpu.PC;
        int stuck_count = 0;

        printf("Running from PC=0x%08X", cpu.PC);
        if (max_steps > 0) {
            printf(" (max %llu steps)", (unsigned long long)max_steps);
        }
        printf("...\n");

        while (machine.run_flag) {
            /* Check step limit */
            if (max_steps > 0 && steps >= max_steps) {
                printf("Reached max steps limit (%llu)\n", (unsigned long long)max_steps);
                break;
            }

            /* Execute one instruction */
            if (!nd500_cpu_step(&cpu)) {
                if (machine.stop_reason != STOP_NONE) {
                    break;
                }
            }

            steps++;

            /* Detect infinite loop (PC stuck) */
            if (cpu.PC == last_pc) {
                stuck_count++;
                if (stuck_count > 100) {
                    printf("PC stuck at 0x%08X for 100+ instructions\n", cpu.PC);
                    break;
                }
            } else {
                stuck_count = 0;
                last_pc = cpu.PC;
            }

            /* Blocking terminal read: the program did an INBT/terminal read with
             * no input available. On a real interactive terminal we honour the
             * SINTRAN "the program waits for input" semantics by blocking until
             * the user types, then resuming - the CPU already rewound to the
             * CALLG, so the read re-executes and consumes the new character. */
            if (machine.stop_reason == STOP_WAIT_INPUT) {
                if (mon_console_wait_for_input()) {
                    machine.stop_reason = STOP_NONE;
                    machine.run_flag = 1;
                    stuck_count = 0;
                    continue;
                }
                /* No input can ever arrive (EOF on redirected stdin) - stop. */
                printf("Waiting for input, but none available (EOF)\n");
                break;
            }

            /* Check for program exit */
            if (machine.stop_reason == STOP_MON_HALT) {
                printf("Program exited normally (MON 0B)\n");
                break;
            }

            if (machine.stop_reason == STOP_MON_UNIMPLEMENTED) {
                printf("Unimplemented MON call at PC=0x%08X\n", cpu.PC);
                break;
            }
        }

        printf("Execution complete: %llu instructions\n", (unsigned long long)steps);
        printf("Final PC: 0x%08X\n", cpu.PC);

        if (machine.stop_reason != STOP_NONE && machine.stop_reason != STOP_MON_HALT) {
            printf("Stop reason: %s\n", nd500_stop_reason_str(machine.stop_reason));
        }

        /* Close trace file if open */
        nd500_dbg_close_trace_file();

        nd500_machine_free(&machine);
        return (machine.stop_reason == STOP_MON_HALT || machine.stop_reason == STOP_NONE) ? 0 : 1;
    }

    if (hex_len > 0) {
        uint32_t end = dis_addr + hex_len;
        for (uint32_t a = dis_addr; a < end; ++a) {
            if ((a - dis_addr) % 16 == 0) printf("%08X: ", a);
            printf("%02X ", nd500_bus_read8(&machine, a));
            if ((a - dis_addr) % 16 == 15 || a + 1 == end) printf("\n");
        }
        return 0;
    }

    if (dis_len > 0) {
        /* If text_size is known and user didn't specify length, clamp to text */
        uint32_t actual_len = (text_size > 0 && dis_len > text_size) ? text_size : dis_len;
        char outbuf[16384];
        size_t n = nd500_dbg_disasm(&machine, dis_addr, actual_len, outbuf, sizeof(outbuf));
        if (n > 0) {
            fwrite(outbuf, 1, n, stdout);
            if (outbuf[n-1] != '\n') fputc('\n', stdout);
        }
        return 0;
    }

	printf("nd500x: nothing to do. Use --monitor for the SINTRAN shell, --debug for the\n");
	printf("low-level debugger, or --run to execute a loaded program. See --help.\n");
	return 0;
}
