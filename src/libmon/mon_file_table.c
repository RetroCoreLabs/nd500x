/*
 * SINTRAN III File System Tables Implementation
 *
 * Implements device reservations and open file tracking for MON calls.
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#include "mon_file_table.h"
#include "mon.h"
#include "mon_path.h"
#include "mon_clock.h"
#include <string.h>
#include <stdlib.h>
#include <strings.h>  /* strcasecmp */
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>  /* unlink */
#include <dirent.h>  /* SINTRAN abbreviated-name resolution */

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

/* Forward declarations */
static uint32_t unix_to_nd_date(time_t t);

/* Static tables */
static ReservationEntry reservation_table[MAX_DEVICES];
static OpenFileEntry open_files[FILE_TABLE_SIZE];
static ConsoleIO* console_io = NULL;

/* Helper: Write 16-bit big-endian */
static void write_be16(uint8_t* buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

/* Helper: Write 32-bit big-endian */
static void write_be32(uint8_t* buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

/* Helper: Read 16-bit big-endian */
static uint16_t read_be16(const uint8_t* buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}

/* Helper: Read 32-bit big-endian */
static uint32_t read_be32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

/* Helper: Write SINTRAN string (0x27 terminated) */
static void write_sintran_string(uint8_t* buf, const char* str, size_t max_len) {
    size_t len = str ? strlen(str) : 0;
    if (len > max_len) len = max_len;
    if (len > 0) {
        memcpy(buf, str, len);
    }
    if (len < max_len) {
        buf[len] = SINTRAN_STRING_END;
    }
}

/* ============================================================
 * Initialization
 * ============================================================ */

void mon_file_table_init(void) {
    memset(reservation_table, 0, sizeof(reservation_table));
    memset(open_files, 0, sizeof(open_files));
    /* Note: Don't reset console_io - it's an external hook that persists across init */
    mon_log(MON_LOG_INFO, "MON file table initialized");
}

void mon_file_table_reset(void) {
    /* Close any open host files */
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        if (open_files[i].in_use && open_files[i].host_file) {
            fclose(open_files[i].host_file);
        }
    }
    mon_file_table_init();
}

/* ============================================================
 * Device Classification (SINTRAN Appendix B)
 * ============================================================ */

bool is_character_device(uint32_t dev) {
    /* Octal 0-77 (decimal 0-63) = Character devices */
    return dev <= 63;
}

bool is_mass_storage_file(uint32_t dev) {
    /* Octal 100-177 (decimal 64-127) = Open mass storage files */
    return dev >= 64 && dev <= 127;
}

bool is_terminal(uint32_t dev) {
    /* Multiple terminal ranges */
    return (dev >= 1024 && dev <= 1087) ||  /* Terminals 65-128 (octal 2000-2077) */
           (dev >= 1472 && dev <= 1535) ||  /* Terminals 129-192 (octal 2700-2777) */
           (dev >= 1536 && dev <= 1599);    /* Terminals 193-256 (octal 3000-3077) */
}

/* ============================================================
 * Reservation API (MON 122/123)
 * ============================================================ */

int mon_reserve_device(uint32_t device_no, uint8_t io_flag, bool wait) {
    if (device_no >= MAX_DEVICES) {
        mon_log(MON_LOG_WARN, "MON RESRV: Device number %u out of range", device_no);
        return -1;
    }

    if (io_flag > 1) {
        mon_log(MON_LOG_WARN, "MON RESRV: Invalid io_flag %u (must be 0 or 1)", io_flag);
        return -52;  /* Invalid parameter */
    }

    ReservationEntry* entry = &reservation_table[device_no];

    if (io_flag == IO_FLAG_INPUT) {
        if (entry->reserved_input) {
            if (!wait) {
                return -1;  /* Already reserved, don't wait */
            }
            /* In a real implementation, we would wait here */
            /* For now, just fail immediately */
            mon_log(MON_LOG_WARN, "MON RESRV: Device %u input already reserved", device_no);
            return -1;
        }
        entry->reserved_input = true;
    } else {
        if (entry->reserved_output) {
            if (!wait) {
                return -1;
            }
            mon_log(MON_LOG_WARN, "MON RESRV: Device %u output already reserved", device_no);
            return -1;
        }
        entry->reserved_output = true;
    }

    mon_log(MON_LOG_DEBUG, "MON RESRV: Reserved device %u (%s)",
              device_no, io_flag == IO_FLAG_INPUT ? "input" : "output");
    return 0;
}

int mon_release_device(uint32_t device_no, uint8_t io_flag) {
    if (device_no >= MAX_DEVICES) {
        return -1;
    }

    if (io_flag > 1) {
        mon_log(MON_LOG_WARN, "MON RELES: Invalid io_flag %u (must be 0 or 1)", io_flag);
        return -52;  /* Invalid parameter */
    }

    ReservationEntry* entry = &reservation_table[device_no];

    /* Per carved L07 BRELEASE (BRELE=010610B): releasing a valid device that is
     * not currently reserved is an idempotent SUCCESS no-op (the primitive
     * branches on DF.RESLI=0 straight to the register-restore/return with NO
     * error code loaded). Signal "not reserved" distinctly (-2) so the caller
     * can succeed on it, while a genuinely invalid device (-1) / bad io_flag
     * (-52) still errors. */
    if (io_flag == IO_FLAG_INPUT) {
        if (!entry->reserved_input) {
            mon_log(MON_LOG_DEBUG, "MON RELES: Device %u input not reserved (no-op)", device_no);
            return -2;
        }
        entry->reserved_input = false;
    } else {
        if (!entry->reserved_output) {
            mon_log(MON_LOG_DEBUG, "MON RELES: Device %u output not reserved (no-op)", device_no);
            return -2;
        }
        entry->reserved_output = false;
    }

    mon_log(MON_LOG_DEBUG, "MON RELES: Released device %u (%s)",
              device_no, io_flag == IO_FLAG_INPUT ? "input" : "output");
    return 0;
}

bool mon_is_device_reserved(uint32_t device_no, uint8_t io_flag) {
    if (device_no >= MAX_DEVICES) {
        return false;
    }

    ReservationEntry* entry = &reservation_table[device_no];
    return io_flag == IO_FLAG_INPUT ? entry->reserved_input : entry->reserved_output;
}

/* ============================================================
 * Open File API (MON 50/43)
 * ============================================================ */

bool mon_file_table_is_valid_file_number(int file_number) {
    return file_number >= FILE_NUMBER_MIN && file_number <= FILE_NUMBER_MAX;
}

OpenFileEntry* mon_file_table_get(int file_number) {
    if (!mon_file_table_is_valid_file_number(file_number)) {
        return NULL;
    }
    int index = file_number - FILE_NUMBER_MIN;
    return &open_files[index];
}

/* Global scratch file counter for unique naming */
static int g_scratch_counter = 1;

/* ------------------------------------------------------------------
 * SINTRAN abbreviated file-name matching.
 *
 * Carved from L-VSX-500 segment 006-S3FS: comparator COMPS @041552,
 * scanner decision GOBJI @056326 (terminal codes at 056576-056607).
 * See CARVE-ANSWER-FLPAR-MDEAB-FOR-IMPLEMENTER.md at
 * /mnt/e/Dev/Ronny/NDInsight/tools/sintran-segment-carver/versions/
 *   L-VSX-500/re/segments-ref/006-S3FS/
 *
 * COMPS compares a whole supplied name against a whole stored name in
 * one pass; the '-' handling yields per-subpart behaviour implicitly.
 * On the guest side both strings end with 047 ('); by the time a name
 * reaches here the descriptor reader has already converted that to a
 * C NUL, so NUL is the terminator below.
 * ------------------------------------------------------------------ */
#define SINTRAN_NO_MATCH     0
#define SINTRAN_PREFIX_MATCH 1
#define SINTRAN_EXACT_MATCH  2

static int sintran_comps(const char* a, const char* b) {
    int i = 0, j = 0;
    for (;;) {
        char ca = a[i], cb = b[j];
        if (ca == cb) {
            if (ca == '\0') return SINTRAN_EXACT_MATCH;   /* 041603/041612 */
            i++; j++; continue;                           /* 041606 */
        }
        if (ca == '*') { i++; j++; continue; }            /* 041600/041661 */
        if (ca == '\0') return SINTRAN_PREFIX_MATCH;      /* 041616-041620 */
        if (ca == '-') {                                  /* 041621-041706 */
            /* Positional/empty subpart: skip B to its next '-' boundary.
             * UNVERIFIED (per carve doc): the exact branch when B's
             * terminator arrives before a '-' - i.e. the supplied name has
             * MORE subparts than the stored one - was not traced. Treated
             * as a mismatch here; the verified common path is unaffected. */
            while (b[j] != '-' && b[j] != '\0') j++;
            if (b[j] == '\0') return SINTRAN_NO_MATCH;
            i++; j++;
            continue;
        }
        return SINTRAN_NO_MATCH;                          /* 041621->041623->041614 */
    }
}

/* Scanner decision (GOBJI @056326): exact wins outright; otherwise count
 * prefix matches - 0 -> 056 no-such, >1 -> 057 ambiguous, 1 -> unique.
 * Returns 0 and fills resolved[] on a unique/exact hit; else -46 (no such
 * file) or -47 (ambiguous). host_path is a literal path that already failed
 * to open; its directory is scanned for an abbreviation match. */
static int sintran_resolve_abbrev(const char* host_path, char* resolved, size_t resolved_size) {
    char dir[256];
    const char* base;
    const char* slash = strrchr(host_path, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - host_path);
        if (dlen >= sizeof(dir)) return -46;
        memcpy(dir, host_path, dlen);
        dir[dlen] = '\0';
        base = slash + 1;
    } else {
        strcpy(dir, ".");
        base = host_path;
    }

    /* Split the supplied basename into name and type; they are matched as
     * separate strings (the guest's NAME:TYPE framing is split off before
     * name matching - SEPOB @056645 / SEPFS @042622). */
    char want_name[128], want_type[32];
    const char* dot = strrchr(base, '.');
    if (dot) {
        size_t nlen = (size_t)(dot - base);
        if (nlen >= sizeof(want_name)) return -46;
        memcpy(want_name, base, nlen);
        want_name[nlen] = '\0';
        snprintf(want_type, sizeof(want_type), "%s", dot + 1);
    } else {
        snprintf(want_name, sizeof(want_name), "%s", base);
        want_type[0] = '\0';
    }

    DIR* d = opendir(dir);
    if (!d) return -46;

    int matches = 0;
    char found[256];
    found[0] = '\0';
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char have_name[128], have_type[32];
        const char* hdot = strrchr(de->d_name, '.');
        if (hdot) {
            size_t nlen = (size_t)(hdot - de->d_name);
            if (nlen >= sizeof(have_name)) continue;
            memcpy(have_name, de->d_name, nlen);
            have_name[nlen] = '\0';
            snprintf(have_type, sizeof(have_type), "%s", hdot + 1);
        } else {
            snprintf(have_name, sizeof(have_name), "%s", de->d_name);
            have_type[0] = '\0';
        }

        int nres = sintran_comps(want_name, have_name);
        if (nres == SINTRAN_NO_MATCH) continue;
        int tres = sintran_comps(want_type, have_type);
        if (tres == SINTRAN_NO_MATCH) continue;

        snprintf(found, sizeof(found), "%s/%s", dir, de->d_name);
        if (nres == SINTRAN_EXACT_MATCH && tres == SINTRAN_EXACT_MATCH) {
            matches = 1;
            break;                       /* exact wins outright, scan stops */
        }
        matches++;                       /* 056535 MIN ,B41 */
    }
    closedir(d);

    if (matches == 0) return -46;        /* 056 No such file name */
    if (matches > 1) return -47;         /* 057 Ambiguous file name */
    snprintf(resolved, resolved_size, "%s", found);
    return 0;
}

/* Public wrapper: given a literal host path <dir>/<NAME>.<TYPE> that does not
 * exist verbatim, scan its directory for a SINTRAN abbreviated-name match using
 * the carved COMPS/GOBJI rules. Returns 0 and fills resolved[] on a unique hit,
 * -46 (no such file) if nothing matched, -47 (ambiguous) if more than one did. */
int mon_resolve_abbrev(const char* host_path, char* resolved, size_t resolved_size) {
    return sintran_resolve_abbrev(host_path, resolved, resolved_size);
}

int mon_file_open_ex(const char* filename, const char* filetype, uint8_t access_mode, int requested_file_no) {
    int free_slot = -1;
    bool is_scratch = false;
    char scratch_name[64] = {0};
    const char* effective_filename = filename;
    const char* effective_filetype = filetype;

    /* SINTRAN "create-on-open" convention: a file name enclosed in double quotes
     * means "create this file if it does not already exist" (it becomes an indexed
     * file); WITHOUT quotes the file must already exist. Primary sources:
     * ND-60.050.06 SINTRAN III Users Guide p1422 ("A file may be created ... by
     * the @OPEN-FILE ... command ... with the file name surrounded by quotation
     * marks"), and ND-60.128.5 SINTRAN III Reference Manual ("If the file does not
     * exist, it is created by giving the name in quotes. It will be an indexed
     * file.").
     *
     * The quotes arrive as literal characters in the name (the ND linker passes
     * FileName='"A-TEST"' for `OPEN-DOMAIN "A-TEST"`). Strip them so the real file
     * name is used; the create itself is handled by the write-mode fallback below.
     * quoted_create records that this open explicitly asked to create. */
    char unquoted_name[128];
    bool quoted_create = false;
    if (filename) {
        size_t nlen = mon_strlen_sintran(filename);
        if (nlen >= 2 && filename[0] == '"' && filename[nlen - 1] == '"') {
            size_t inner = nlen - 2;
            if (inner < sizeof(unquoted_name)) {
                memcpy(unquoted_name, filename + 1, inner);
                unquoted_name[inner] = '\0';
                effective_filename = unquoted_name;
                quoted_create = true;
                mon_log(MON_LOG_DEBUG, "MON OPEN: quoted name '%s' -> create '%s'",
                        filename, unquoted_name);
            }
        }
    }


    /* Handle :TYPE scratch file syntax
     *
     * When filename is just ":TYPE" (e.g., ":NRF"), it means "create a
     * scratch file of type TYPE". Generate unique name like SCRATCH-00001.
     */
    if (filename && filename[0] == ':') {
        is_scratch = true;

        /* Use part after colon as the file type if not empty */
        const char* type_part = filename + 1;
        size_t type_len = mon_strlen_sintran(type_part);
        if (type_len > 0 && (!filetype || filetype[0] == '\0' ||
            (uint8_t)filetype[0] == SINTRAN_STRING_END)) {
            effective_filetype = type_part;
        }

        /* Generate unique scratch filename */
        snprintf(scratch_name, sizeof(scratch_name), "(SCRATCH)SCRATCH-%05d",
                 g_scratch_counter++);
        effective_filename = scratch_name;

        mon_log(MON_LOG_INFO, "MON OPEN: Generating scratch file '%s:%s' for '%s'",
                scratch_name, effective_filetype ? effective_filetype : "",
                filename);
    }

    /* If caller requested a specific file number, try to use it */
    if (requested_file_no != 0) {
        if (!mon_file_table_is_valid_file_number(requested_file_no)) {
            mon_log(MON_LOG_WARN, "MON OPEN: Requested file number %d out of range (64-127)",
                    requested_file_no);
            return -52;  /* Error 52: Invalid parameter */
        }

        int requested_slot = requested_file_no - FILE_NUMBER_MIN;
        if (open_files[requested_slot].in_use) {
            mon_log(MON_LOG_WARN, "MON OPEN: Requested file number %d already in use",
                    requested_file_no);
            return -54;  /* Error 54: File already open */
        }
        free_slot = requested_slot;
    } else {
        /* Find a free slot */
        for (int i = 0; i < FILE_TABLE_SIZE; i++) {
            if (!open_files[i].in_use) {
                free_slot = i;
                break;
            }
        }
    }

    if (free_slot < 0) {
        mon_log(MON_LOG_WARN, "MON OPEN: No free file slots");
        return -55;  /* Error 55: No free file slots */
    }

    OpenFileEntry* entry = &open_files[free_slot];
    int file_number = FILE_NUMBER_MIN + free_slot;

    /* Parse SINTRAN name to extract user, name, and extension */
    char parsed_user[32] = {0};
    char parsed_name[32] = {0};
    char parsed_ext[16] = {0};
    mon_parse_sintran_name(effective_filename, parsed_user, sizeof(parsed_user),
                           parsed_name, sizeof(parsed_name),
                           parsed_ext, sizeof(parsed_ext));

    /* Check if this is a scratch file (user is "SCRATCH" or detected from :TYPE syntax) */
    if (!is_scratch) {
        is_scratch = (strcasecmp(parsed_user, "SCRATCH") == 0);
    }

    /* Translate SINTRAN path to host path. For a LOOKUP (opening an existing
     * file) apply the verified GFILI own-directory-then-(SYSTEM) fallback; for a
     * CREATE (quoted name or scratch ":TYPE") stay in the own/named directory so
     * the new file is not shadowed by a same-named SYSTEM file. */
    char host_path[256];
    int xlate = (quoted_create || is_scratch)
        ? mon_translate_path(effective_filename, effective_filetype, host_path, sizeof(host_path))
        : mon_translate_path_lookup(effective_filename, effective_filetype, host_path, sizeof(host_path));
    if (xlate != 0) {
        /* Fallback to simple path construction */
        if (effective_filetype && effective_filetype[0]) {
            snprintf(host_path, sizeof(host_path), "%s.%s", effective_filename, effective_filetype);
        } else {
            snprintf(host_path, sizeof(host_path), "%s", effective_filename);
        }
    }

    /* Ensure parent directory exists for scratch files */
    if (is_scratch) {
        char dir_path[256];
        strncpy(dir_path, host_path, sizeof(dir_path) - 1);
        dir_path[sizeof(dir_path) - 1] = '\0';
        char* last_slash = strrchr(dir_path, '/');
#ifdef _WIN32
        char* last_backslash = strrchr(dir_path, '\\');
        if (last_backslash > last_slash) last_slash = last_backslash;
#endif
        if (last_slash) {
            *last_slash = '\0';
            mon_ensure_directory(dir_path);
        }
    }

    /* Determine fopen mode based on access code
     * Per SINTRAN MASTER reference ND-860228.2 EN:
     *   0 = Sequential write
     *   1 = Sequential read
     *   2 = Random read or write
     *   3 = Random read only
     *   4 = Sequential read or write
     *   5 = Sequential write append
     *   6-9 = Various contiguous/extend modes
     */
    const char* fmode;
    bool allows_write = false;
    switch (access_mode) {
        case ACCESS_SEQ_READ:      /* 1 */
        case ACCESS_RAND_READ:     /* 3 */
        case ACCESS_RAND_READ_CTG: /* 7 */
            fmode = "rb";
            break;
        case ACCESS_SEQ_WRITE:     /* 0 */
            fmode = "wb";
            allows_write = true;
            break;
        case ACCESS_RAND_RDWR:     /* 2 */
        case ACCESS_SEQ_RDWR:      /* 4 */
        case ACCESS_RAND_RDWR_CTG: /* 6 */
        case ACCESS_RAND_RDWR_RT:  /* 8 */
            fmode = "r+b";
            allows_write = true;
            break;
        case ACCESS_SEQ_APPEND:    /* 5 */
        case ACCESS_RAND_EXTEND:   /* 9 */
            fmode = "ab";
            allows_write = true;
            break;
        default:
            fmode = "rb";
            break;
    }

    /* Try to open the host file */
    FILE* fp = fopen(host_path, fmode);

    /* CREATE semantics, byte-verified against the SINTRAN GCFIL/CROBJ/GFILI
     * resolver (carve 006-S3FS: GCFIL @064670B, CROBJ @063726B, GFILI @057173B;
     * see CARVE-ANSWER-OPEN-QUOTED-FILENAME.md):
     *
     *  - A QUOTED name creates the file if it is absent, and errors 076B "File
     *    already exists" if it is present (CROBJ - never truncates/overwrites).
     *  - A scratch file (":TYPE" syntax) is created on open by definition.
     *  - An UNQUOTED name is LOOKUP-ONLY (GFILI): if it is missing, OPEN returns
     *    056B "No such file name" for EVERY access code. SINTRAN III has NO
     *    write-open-creates - a plain write-open of a missing file does not create
     *    it. (Callers that need a new file either quote the name or @CREATE-FILE /
     *    MON 221B it first - which is what NC does for its scratch CAT file.)
     *
     * This replaces a non-standard auto-create-on-write that diverged from real
     * hardware. */
    if (quoted_create) {
        if (fp) {
            fclose(fp);
            mon_log(MON_LOG_WARN, "MON OPEN: quoted name but '%s' already exists -> 076B", host_path);
            return -62;  /* 076B File already exists */
        }
        fp = fopen(host_path, "w+b");  /* absent + quoted -> create */
    } else if (!fp && is_scratch && allows_write) {
        fp = fopen(host_path, "w+b");  /* scratch files are created on open */
    }

    if (!fp && !allows_write) {
        /* The literal name did not exist. SINTRAN allows an abbreviated name
         * on a read open (not when creating a file), so resolve it against the
         * directory using the carved COMPS/GOBJI rules. */
        char resolved[256];
        int rc = sintran_resolve_abbrev(host_path, resolved, sizeof(resolved));
        if (rc == 0) {
            fp = fopen(resolved, fmode);
            if (fp) {
                mon_log(MON_LOG_INFO, "MON OPEN: '%s' resolved to '%s'", host_path, resolved);
                snprintf(host_path, sizeof(host_path), "%s", resolved);
            }
        } else if (rc == -47) {
            mon_log(MON_LOG_WARN, "MON OPEN: Ambiguous file name '%s'", host_path);
            return -47;  /* Error 57B: Ambiguous file name */
        }
    }

    if (!fp) {
        mon_log(MON_LOG_WARN, "MON OPEN: Cannot open host file '%s'", host_path);
        return -46;  /* Error 46: No such filename */
    }

    /* Get file size with error checking */
    if (fseek(fp, 0, SEEK_END) != 0) {
        mon_log(MON_LOG_WARN, "MON OPEN: Cannot seek to end of '%s'", host_path);
        fclose(fp);
        return -46;
    }
    long file_size = ftell(fp);
    if (file_size < 0) {
        mon_log(MON_LOG_WARN, "MON OPEN: Cannot determine size of '%s'", host_path);
        fclose(fp);
        return -46;
    }
    /* Check for overflow on 64-bit systems */
    if ((unsigned long)file_size > UINT32_MAX) {
        mon_log(MON_LOG_WARN, "MON OPEN: File '%s' too large (%ld bytes, max %u)",
                host_path, file_size, UINT32_MAX);
        fclose(fp);
        return -46;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        mon_log(MON_LOG_WARN, "MON OPEN: Cannot seek to start of '%s'", host_path);
        fclose(fp);
        return -46;
    }

    /* Initialize entry */
    memset(entry, 0, sizeof(OpenFileEntry));
    entry->in_use = true;
    entry->is_scratch = is_scratch;
    entry->access_mode = access_mode;
    entry->current_position = 0;
    entry->block_size = 512;  /* Default block size */
    entry->host_file = fp;
    strncpy(entry->host_path, host_path, sizeof(entry->host_path) - 1);

    /* Initialize ObjectEntry - use parsed name if available */
    const char* obj_name = parsed_name[0] ? parsed_name : effective_filename;
    const char* obj_type = effective_filetype ? effective_filetype : (parsed_ext[0] ? parsed_ext : NULL);
    object_entry_init_file(&entry->object_entry, obj_name, obj_type,
                          (uint32_t)file_size, FILETYPE_INDEXED);
    entry->object_entry.current_open_count = 1;
    entry->object_entry.total_open_count = 1;
    entry->object_entry.object_index = (uint16_t)file_number;
    entry->object_entry.header = HEADER_USED;
    if (allows_write) {
        entry->object_entry.header |= HEADER_WRITE_OPEN;
    }

    /* Set dates from host file stats */
    struct stat st;
    if (stat(host_path, &st) == 0) {
        entry->object_entry.date_created = unix_to_nd_date(st.st_ctime);
        entry->object_entry.date_read = unix_to_nd_date(st.st_atime);
        entry->object_entry.date_written = unix_to_nd_date(st.st_mtime);
    }

    mon_log(MON_LOG_INFO, "MON OPEN: Opened '%s' as file number %d (mode=%d%s)",
              host_path, file_number, access_mode, is_scratch ? ", scratch" : "");

    return file_number;
}

/* Wrapper for backwards compatibility - allocates file number automatically */
int mon_file_open(const char* filename, const char* filetype, uint8_t access_mode) {
    return mon_file_open_ex(filename, filetype, access_mode, 0);
}

int mon_file_close(int file_number) {
    if (!mon_file_table_is_valid_file_number(file_number)) {
        return -52;  /* Error 52: Invalid parameter (not in 64-127 range) */
    }

    int index = file_number - FILE_NUMBER_MIN;
    OpenFileEntry* entry = &open_files[index];

    if (!entry->in_use) {
        mon_log(MON_LOG_WARN, "MON CLOSE: File number %d not open", file_number);
        return -53;
    }

    /* Log if file was mapped as segment (automatic disconnect per SINTRAN docs) */
    if (entry->mapped_as_segment) {
        mon_log(MON_LOG_INFO, "MON CLOSE: File %d auto-disconnected from segment %u",
                file_number, entry->mapped_segment_no);
    }

    /* Save path and scratch flag before closing */
    char host_path[256];
    strncpy(host_path, entry->host_path, sizeof(host_path) - 1);
    host_path[sizeof(host_path) - 1] = '\0';
    bool is_scratch = entry->is_scratch;

    /* Apply a deferred MON 73B SMAX length: SMAX only RECORDS the logical
     * max-byte count; the physical truncation happens here at CLOSE. Only touch
     * files where SMAX actually ran (max_bytes_set), and never a scratch file
     * that is about to be unlinked. */
    if (entry->max_bytes_set && entry->host_file && !entry->is_scratch) {
        int fd = fileno(entry->host_file);
        if (fd >= 0) {
            fflush(entry->host_file);
            if (ftruncate(fd, (off_t)entry->object_entry.bytes_in_file) != 0) {
                mon_log(MON_LOG_WARN, "MON CLOSE: ftruncate('%s', %u) failed",
                        host_path, entry->object_entry.bytes_in_file);
            } else {
                mon_log(MON_LOG_DEBUG, "MON CLOSE: applied SMAX length %u to '%s'",
                        entry->object_entry.bytes_in_file, host_path);
            }
        }
    }

    /* Close host file */
    if (entry->host_file) {
        fclose(entry->host_file);
    }

    mon_log(MON_LOG_INFO, "MON CLOSE: Closed file number %d ('%s')",
              file_number, host_path);

    /* Clear entry */
    memset(entry, 0, sizeof(OpenFileEntry));

    /* Delete scratch files - UNLESS persistence is requested. The VDM compiler
     * pipeline needs the scratch file (SCRATCHnn:DATA, file 0100 octal = 64) to
     * survive NC's close so the CAT-500 back-end can read the CAT intermediate
     * from it. Set ND500X_KEEP_SCRATCH=1 to keep scratch files on close. */
    if (is_scratch && host_path[0] != '\0') {
        if (getenv("ND500X_KEEP_SCRATCH")) {
            mon_log(MON_LOG_INFO, "MON CLOSE: Kept scratch file '%s' (ND500X_KEEP_SCRATCH)", host_path);
        } else {
            unlink(host_path);
            mon_log(MON_LOG_INFO, "MON CLOSE: Deleted scratch file '%s'", host_path);
        }
    }

    return 0;
}

/* ============================================================
 * ObjectEntry Serialization
 * ============================================================ */

void object_entry_to_buffer(const ObjectEntry* entry, uint8_t buffer[64]) {
    memset(buffer, 0, 64);

    /* Offset 0-1: Header */
    write_be16(&buffer[0], entry->header);

    /* Offset 2-17: ObjectName (16 bytes, 0x27 terminated) */
    write_sintran_string(&buffer[2], entry->object_name, 16);

    /* Offset 18-21: Type (4 bytes, 0x27 terminated) */
    write_sintran_string(&buffer[18], entry->type, 4);

    /* Offset 22-23: NextVersion */
    write_be16(&buffer[22], entry->next_version);

    /* Offset 24-25: PrevVersion */
    write_be16(&buffer[24], entry->prev_version);

    /* Offset 26-27: AccessBits */
    write_be16(&buffer[26], entry->access_bits);

    /* Offset 28-29: FileType flags */
    write_be16(&buffer[28], entry->file_type);

    /* Offset 30-31: DeviceNumber */
    write_be16(&buffer[30], entry->device_number);

    /* Offset 32-33: Reserved */
    write_be16(&buffer[32], entry->reserved);

    /* Offset 34-35: ObjectIndex */
    write_be16(&buffer[34], entry->object_index);

    /* Offset 36-37: CurrentOpenCount */
    write_be16(&buffer[36], entry->current_open_count);

    /* Offset 38-39: TotalOpenCount */
    write_be16(&buffer[38], entry->total_open_count);

    /* Offset 40-43: DateCreated */
    write_be32(&buffer[40], entry->date_created);

    /* Offset 44-47: LastDateOpenedForRead */
    write_be32(&buffer[44], entry->date_read);

    /* Offset 48-51: LastDateOpenedForWrite */
    write_be32(&buffer[48], entry->date_written);

    /* Offset 52-55: PagesInFile */
    write_be32(&buffer[52], entry->pages_in_file);

    /* Offset 56-59: BytesInFile (stored as value-1) */
    write_be32(&buffer[56], entry->bytes_in_file > 0 ? entry->bytes_in_file - 1 : 0);

    /* Offset 60-63: FilePointer */
    write_be32(&buffer[60], entry->file_pointer);
}

void object_entry_from_buffer(ObjectEntry* entry, const uint8_t buffer[64]) {
    memset(entry, 0, sizeof(ObjectEntry));

    entry->header = read_be16(&buffer[0]);
    memcpy(entry->object_name, &buffer[2], 16);
    memcpy(entry->type, &buffer[18], 4);
    entry->next_version = read_be16(&buffer[22]);
    entry->prev_version = read_be16(&buffer[24]);
    entry->access_bits = read_be16(&buffer[26]);
    entry->file_type = read_be16(&buffer[28]);
    entry->device_number = read_be16(&buffer[30]);
    entry->reserved = read_be16(&buffer[32]);
    entry->object_index = read_be16(&buffer[34]);
    entry->current_open_count = read_be16(&buffer[36]);
    entry->total_open_count = read_be16(&buffer[38]);
    entry->date_created = read_be32(&buffer[40]);
    entry->date_read = read_be32(&buffer[44]);
    entry->date_written = read_be32(&buffer[48]);
    entry->pages_in_file = read_be32(&buffer[52]);
    entry->bytes_in_file = read_be32(&buffer[56]) + 1;  /* Stored as value-1 */
    entry->file_pointer = read_be32(&buffer[60]);
}

/* ============================================================
 * ObjectEntry Initialization Helpers
 * ============================================================ */

void object_entry_init_terminal(ObjectEntry* entry, const char* name, uint16_t device_number) {
    memset(entry, 0, sizeof(ObjectEntry));
    entry->header = HEADER_USED;
    strncpy(entry->object_name, name, 15);
    entry->object_name[15] = '\0';  /* Ensure null termination */
    entry->file_type = FILETYPE_TERMINAL;
    entry->device_number = device_number;
    entry->access_bits = 0x1F;  /* Full access */
    entry->current_open_count = 1;
    entry->total_open_count = 1;
}

void object_entry_init_file(ObjectEntry* entry, const char* name, const char* type,
                           uint32_t size_bytes, uint16_t file_type_flags) {
    memset(entry, 0, sizeof(ObjectEntry));
    entry->header = HEADER_USED;

    if (name) {
        strncpy(entry->object_name, name, 15);
        entry->object_name[15] = '\0';  /* Ensure null termination */
    }
    if (type) {
        strncpy(entry->type, type, 3);
        entry->type[3] = '\0';  /* Ensure null termination */
    }

    entry->file_type = file_type_flags;
    entry->bytes_in_file = size_bytes;
    entry->pages_in_file = (size_bytes + SINTRAN_PAGE_SIZE - 1) / SINTRAN_PAGE_SIZE;
    entry->access_bits = 0x07;  /* Read/Write/Append for owner */
}

/* ============================================================
 * Console I/O Support
 * ============================================================ */

void mon_file_table_set_console(ConsoleIO* console) {
    console_io = console;
}

ConsoleIO* mon_file_table_get_console(void) {
    return console_io;
}

int mon_console_wait_for_input(void) {
    if (console_io && console_io->wait_for_input) {
        return console_io->wait_for_input(console_io->context);
    }
    return 0;  /* Headless/scripted console cannot block; host decides */
}

/* ============================================================
 * Queued Console I/O Support
 *
 * Provides a queue-based console for testing and scripted input.
 * Input can be queued with mon_queue_console_input(), and the
 * built-in queued console handlers will be installed automatically.
 * ============================================================ */

#define QUEUED_CONSOLE_MAX 4096

static struct {
    char input_buffer[QUEUED_CONSOLE_MAX];
    char output_buffer[QUEUED_CONSOLE_MAX];
    size_t input_read_pos;
    size_t input_write_pos;
    size_t input_count;
    size_t output_len;
} g_queued_console = {0};

static bool queued_console_char_available(void* ctx) {
    (void)ctx;
    return g_queued_console.input_count > 0;
}

static int queued_console_read_char(void* ctx) {
    (void)ctx;
    if (g_queued_console.input_count == 0) {
        return -1;  /* EOF - no input available */
    }
    char ch = g_queued_console.input_buffer[g_queued_console.input_read_pos];
    g_queued_console.input_read_pos = (g_queued_console.input_read_pos + 1) % QUEUED_CONSOLE_MAX;
    g_queued_console.input_count--;
    return (unsigned char)ch;
}

static void queued_console_write_char(void* ctx, int ch) {
    (void)ctx;
    if (g_queued_console.output_len < QUEUED_CONSOLE_MAX - 1) {
        g_queued_console.output_buffer[g_queued_console.output_len++] = (char)ch;
        g_queued_console.output_buffer[g_queued_console.output_len] = '\0';
    }
}

/* Static console structure for queued I/O */
static ConsoleIO g_queued_console_io = {
    .read_char = queued_console_read_char,
    .write_char = queued_console_write_char,
    .char_available = queued_console_char_available,
    .context = NULL
};

void mon_queue_console_input(const char* input) {
    if (!input) return;

    /* Queue each character */
    while (*input && g_queued_console.input_count < QUEUED_CONSOLE_MAX) {
        g_queued_console.input_buffer[g_queued_console.input_write_pos] = *input++;
        g_queued_console.input_write_pos = (g_queued_console.input_write_pos + 1) % QUEUED_CONSOLE_MAX;
        g_queued_console.input_count++;
    }

    /* Install queued console if not already set */
    if (console_io != &g_queued_console_io) {
        console_io = &g_queued_console_io;
    }
}

void mon_clear_console_queue(void) {
    memset(&g_queued_console, 0, sizeof(g_queued_console));
}

const char* mon_get_console_output(void) {
    return g_queued_console.output_buffer;
}

size_t mon_get_console_output_len(void) {
    return g_queued_console.output_len;
}

size_t mon_get_console_input_remaining(void) {
    return g_queued_console.input_count;
}

/* Bytes from the read position up to and including the first break character
 * (CR = 0x0D), or 0 if no break character is queued. This is the SINTRAN
 * "NoUntilBreak" value reported by 313B IBRISZ: it lets a caller know whether a
 * complete line (terminated by a break) is available. Returns 0 for an empty or
 * unterminated buffer - which is also the correct value for our live
 * interactive console, where nothing is queued (input is read char-by-char). */
size_t mon_get_console_input_until_break(void) {
    size_t pos = g_queued_console.input_read_pos;
    for (size_t i = 0; i < g_queued_console.input_count; i++) {
        char c = g_queued_console.input_buffer[pos];
        if (c == '\r') return i + 1;   /* through the break char */
        pos = (pos + 1) % QUEUED_CONSOLE_MAX;
    }
    return 0;   /* no break character in the buffer */
}

/* ============================================================
 * Standard I/O Console
 *
 * Uses stdin/stdout for interactive console mode.
 * Call mon_install_stdio_console() to enable.
 * Sets terminal to raw mode for proper character-by-character I/O.
 * ============================================================ */

#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>

static struct termios g_orig_termios;
static bool g_termios_saved = false;

static void stdio_restore_terminal(void) {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_termios_saved = false;
    }
}

static void stdio_signal_handler(int sig) {
    stdio_restore_terminal();
    printf("\n");
    exit(128 + sig);
}

static void stdio_setup_terminal(void) {
    if (!isatty(STDIN_FILENO)) {
        return;  /* Not a terminal, skip raw mode */
    }

    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
        g_termios_saved = true;
        atexit(stdio_restore_terminal);

        /* Handle Ctrl+C gracefully */
        signal(SIGINT, stdio_signal_handler);
        signal(SIGTERM, stdio_signal_handler);

        struct termios raw = g_orig_termios;
        /* Disable canonical mode and local echo */
        raw.c_lflag &= ~(ICANON | ECHO);
        /* Read returns after 1 char, no timeout */
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
}

static bool stdio_char_available(void* ctx) {
    (void)ctx;
    /* Use select() to check if stdin has data available */
    fd_set fds;
    struct timeval tv = {0, 0};  /* No wait */
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int stdio_wait_for_input(void* ctx) {
    (void)ctx;
    /* Block until stdin has data (real interactive terminal wait). A NULL
     * timeout makes select() wait indefinitely. Returns 0 on error/EOF. */
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);
    return (r > 0) ? 1 : 0;
}

static int stdio_read_char(void* ctx) {
    (void)ctx;
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        /* Translate Unix LF (Enter key) to CR for SINTRAN */
        if (ch == '\n') {
            ch = '\r';
        }
        return ch;
    }
    return EOF;
}

static void stdio_write_char(void* ctx, int ch) {
    (void)ctx;
    unsigned char c = (unsigned char)ch;

    /* Drop NUL (0x00). SINTRAN/VT100 programs emit NUL as fill/timing padding
     * after a clear-screen (real terminals ignore it). Some terminal emulators
     * instead advance the cursor or print a glyph for NUL, which shifts the
     * whole screen one column and corrupts the layout. It carries no display
     * meaning, so never forward it. */
    if (c == 0x00) return;

    /* Write the byte VERBATIM - the real SINTRAN terminal driver does not
     * expand a bare CR into CR+LF on output. The Monitor Calls manual
     * (ND-860228.2 EN) shows every language example emitting a newline as
     * TWO explicit OutByte calls, CR (13) then LF (10):
     *     OutByte(FileNo, 13);   (* Out carriage return *)
     *     OutByte(FileNo, 10);   (* Out line feed *)
     * A well-behaved program (NC, CAT) sends its own LF; a full-screen VT100
     * program (LINKER, CONVERT-DOMAIN) sends a bare CR to return to column 1
     * of the SAME line and positions rows with absolute ESC[r;cH. Injecting an
     * LF after CR scrolled the screen and corrupted those cursor-addressed
     * forms - the "odd characters" symptom. Emulate the driver: pass through. */
    write(STDOUT_FILENO, &c, 1);
}

static ConsoleIO g_stdio_console = {
    .read_char = stdio_read_char,
    .write_char = stdio_write_char,
    .char_available = stdio_char_available,
    .wait_for_input = stdio_wait_for_input,
    .context = NULL
};

void mon_install_stdio_console(void) {
    stdio_setup_terminal();
    console_io = &g_stdio_console;
}

/* ============================================================
 * Host Path Utilities
 * ============================================================ */

void mon_build_host_path(const char* filename, char* host_path, size_t max_len) {
    /* SINTRAN uses : as extension separator, host uses .
     * Convert FILENAME:EXT to FILENAME.EXT
     */
    const char* colon = strrchr(filename, ':');
    const char* dot = strrchr(filename, '.');

    if (colon) {
        /* Has SINTRAN extension - convert : to . */
        size_t base_len = colon - filename;
        if (base_len >= max_len - 1) base_len = max_len - 2;
        memcpy(host_path, filename, base_len);
        host_path[base_len] = '.';
        strncpy(host_path + base_len + 1, colon + 1, max_len - base_len - 2);
        host_path[max_len - 1] = '\0';
    } else if (dot) {
        /* Already has . extension */
        snprintf(host_path, max_len, "%s", filename);
    } else {
        /* No extension - add .dat */
        snprintf(host_path, max_len, "%s.dat", filename);
    }
}

/* ============================================================
 * Command Buffer Support (MON 12B SETCM)
 *
 * THREAD SAFETY: These functions use static global state without
 * mutex protection. External synchronization required if accessed
 * from multiple threads.
 * ============================================================ */

static char g_command_buffer[256];
static int g_command_buffer_pos = 0;

const char* mon_get_command_buffer(void) {
    return g_command_buffer;
}

int mon_read_command_buffer_char(void) {
    int len = (int)strlen(g_command_buffer);

    if (g_command_buffer_pos < len) {
        return (unsigned char)g_command_buffer[g_command_buffer_pos++];
    }

    /* Device 0 = the SINTRAN command buffer = the INVOCATION command line, which
     * is ALWAYS terminated by CR (015B / 0x0D). Byte-proven in the L07 command
     * processor: at the 47B source marker it substitutes CR (050773: SAA 15 ->
     * SBYT) and resets the byte pointer, so the buffer a program reads via 1B INBT
     * ends in CR - never 47B, never raw EOF. A program launched with no arguments
     * still reads a lone CR.
     *
     * So once the stored bytes are exhausted, deliver exactly one CR if the line
     * did not already end in one, THEN signal end-of-line. Returning raw EOF here
     * instead made the ND linker busy-spin (measured: 20,088 consecutive 1B INBT
     * retries at PC=0xB004E759); delivering the CR lets it complete its line and
     * proceed - verified: the linker reads the CR once and never reads device 0
     * again. The end-of-line (-1) is only reached if a program reads PAST the CR,
     * which the linker does not; what a real device-0 read returns after the CR is
     * unproven (the caller treats -1 as "suspend"). */
    if (g_command_buffer_pos == len && (len == 0 || g_command_buffer[len - 1] != '\r')) {
        g_command_buffer_pos++;   /* consume the synthetic terminating CR */
        return '\r';
    }
    return -1;  /* end of line (CR already delivered) */
}

void mon_reset_command_buffer_pos(void) {
    g_command_buffer_pos = 0;
}

void mon_set_command_buffer(const char* command) {
    if (command) {
        strncpy(g_command_buffer, command, sizeof(g_command_buffer) - 1);
        g_command_buffer[sizeof(g_command_buffer) - 1] = '\0';
    } else {
        g_command_buffer[0] = '\0';
    }
    g_command_buffer_pos = 0;
}

/* ============================================================
 * Scratch File Support
 * ============================================================ */

#include "mon_path.h"
#include "mon_config.h"
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

static int g_cleanup_registered = 0;

/*
 * Convert Unix timestamp to ND date format (packed 32-bit)
 *
 * ND Date Format:
 *   Bits 31-26 (6 bits): Year offset from 1950 (0-63, valid years: 1950-2013)
 *   Bits 25-22 (4 bits): Month (1-12)
 *   Bits 21-17 (5 bits): Day of month (1-31)
 *   Bits 16-12 (5 bits): Hour (0-23)
 *   Bits 11-6  (6 bits): Minute (0-59)
 *   Bits 5-0   (6 bits): Second (0-59)
 */
static uint32_t unix_to_nd_date(time_t t) {
    /* In deterministic (pinned-clock) mode all object-entry dates report the
     * agreed fixed UTC instant, so ROBJE (41B) is bit-reproducible regardless
     * of the host filesystem timestamps. */
    if (mon_clock_is_deterministic()) {
        t = mon_clock_now();
    }
    if (t == 0) return 0;

    struct tm tm_storage;
    struct tm* tm = mon_clock_breakdown(t, &tm_storage);
    if (!tm) return 0;

    int year = tm->tm_year + 1900;

    /* Adjust year to valid ND range 1950-2013 */
    /* Subtract 10 years repeatedly until within range */
    while (year > 2013) {
        year -= 10;
    }
    while (year < 1950) {
        year += 10;
    }

    uint32_t nd_date = 0;
    nd_date |= ((year - 1950) & 0x3F) << 26;   /* Year offset (6 bits) */
    nd_date |= ((tm->tm_mon + 1) & 0x0F) << 22; /* Month 1-12 (4 bits) */
    nd_date |= (tm->tm_mday & 0x1F) << 17;      /* Day (5 bits) */
    nd_date |= (tm->tm_hour & 0x1F) << 12;      /* Hour (5 bits) */
    nd_date |= (tm->tm_min & 0x3F) << 6;        /* Minute (6 bits) */
    nd_date |= (tm->tm_sec & 0x3F);             /* Second (6 bits) */

    return nd_date;
}

/* Cleanup handler for atexit */
static void mon_file_table_cleanup(void) {
    /* Close all scratch files first (deletes them) */
    mon_close_all_scratch_files();

    /* Close remaining open files */
    mon_file_table_reset();
}

void mon_file_table_register_cleanup(void) {
    if (!g_cleanup_registered) {
        atexit(mon_file_table_cleanup);
        g_cleanup_registered = 1;
    }
}

int mon_populate_object_entry_from_host(ObjectEntry* entry,
                                         const char* host_path,
                                         const char* sintran_name,
                                         const char* sintran_type) {
    if (!entry) return -1;

    /* Clear entry */
    memset(entry, 0, sizeof(ObjectEntry));

    /* Set header - mark as used */
    entry->header = HEADER_USED;

    /* Set filename */
    if (sintran_name) {
        size_t name_len = strlen(sintran_name);
        if (name_len > 15) name_len = 15;
        memcpy(entry->object_name, sintran_name, name_len);
        entry->object_name[name_len] = SINTRAN_STRING_END;
    }

    /* Set type/extension */
    if (sintran_type) {
        size_t type_len = strlen(sintran_type);
        if (type_len > 3) type_len = 3;
        memcpy(entry->type, sintran_type, type_len);
        entry->type[type_len] = SINTRAN_STRING_END;
    }

    /* Set access bits - full read/write for owner and public */
    entry->access_bits = 0x1F1F;

    /* Get host file stats */
    if (host_path) {
        struct stat st;
        if (stat(host_path, &st) == 0) {
            /* File size */
            entry->bytes_in_file = (uint32_t)st.st_size;
            entry->pages_in_file = (st.st_size + SINTRAN_PAGE_SIZE - 1) / SINTRAN_PAGE_SIZE;

            /* Timestamps */
            entry->date_created = unix_to_nd_date(st.st_ctime);
            entry->date_read = unix_to_nd_date(st.st_atime);
            entry->date_written = unix_to_nd_date(st.st_mtime);
        }
    }

    /* Set open counts */
    entry->current_open_count = 1;
    entry->total_open_count = 1;

    return 0;
}

int mon_open_scratch_file(const char* filename, const char* filetype) {
    /* Scratch files are detected automatically if user is "SCRATCH".
     * This function just calls mon_file_open with random read/write mode. */
    return mon_file_open(filename, filetype, ACCESS_RAND_RDWR);
}

bool mon_is_scratch_file(int file_number) {
    if (!mon_file_table_is_valid_file_number(file_number)) {
        return false;
    }
    int index = file_number - FILE_NUMBER_MIN;
    return open_files[index].in_use && open_files[index].is_scratch;
}

void mon_close_all_scratch_files(void) {
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        if (open_files[i].in_use && open_files[i].is_scratch) {
            int file_number = FILE_NUMBER_MIN + i;
            char path_copy[256];
            strncpy(path_copy, open_files[i].host_path, sizeof(path_copy) - 1);
            path_copy[sizeof(path_copy) - 1] = '\0';

            /* Close the file handle */
            if (open_files[i].host_file) {
                fclose(open_files[i].host_file);
                open_files[i].host_file = NULL;
            }

            /* Clear entry */
            memset(&open_files[i], 0, sizeof(OpenFileEntry));

            /* Delete the file (unless persistence is requested - see mon_close_file) */
            if (path_copy[0] != '\0') {
                if (getenv("ND500X_KEEP_SCRATCH")) {
                    mon_log(MON_LOG_INFO, "Scratch file %d kept: %s (ND500X_KEEP_SCRATCH)", file_number, path_copy);
                } else {
                    unlink(path_copy);
                    mon_log(MON_LOG_INFO, "Scratch file %d deleted: %s", file_number, path_copy);
                }
            }
        }
    }
}
