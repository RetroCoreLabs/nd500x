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

    if (io_flag == IO_FLAG_INPUT) {
        if (!entry->reserved_input) {
            mon_log(MON_LOG_WARN, "MON RELES: Device %u input not reserved", device_no);
            return -1;
        }
        entry->reserved_input = false;
    } else {
        if (!entry->reserved_output) {
            mon_log(MON_LOG_WARN, "MON RELES: Device %u output not reserved", device_no);
            return -1;
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

int mon_file_open_ex(const char* filename, const char* filetype, uint8_t access_mode, int requested_file_no) {
    int free_slot = -1;
    bool is_scratch = false;
    char scratch_name[64] = {0};
    const char* effective_filename = filename;
    const char* effective_filetype = filetype;

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

    /* Translate SINTRAN path to host path */
    char host_path[256];
    if (mon_translate_path(effective_filename, effective_filetype, host_path, sizeof(host_path)) != 0) {
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
    if (!fp && allows_write) {
        /* For write modes, try creating the file */
        fp = fopen(host_path, "w+b");
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

    /* Close host file */
    if (entry->host_file) {
        fclose(entry->host_file);
    }

    mon_log(MON_LOG_INFO, "MON CLOSE: Closed file number %d ('%s')",
              file_number, host_path);

    /* Clear entry */
    memset(entry, 0, sizeof(OpenFileEntry));

    /* Delete scratch files */
    if (is_scratch && host_path[0] != '\0') {
        unlink(host_path);
        mon_log(MON_LOG_INFO, "MON CLOSE: Deleted scratch file '%s'", host_path);
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

/* Track last written char to avoid CR LF -> CR LF LF */
static int g_stdio_last_char = 0;

static void stdio_write_char(void* ctx, int ch) {
    (void)ctx;
    unsigned char c = (unsigned char)ch;

    /* If we just wrote CR (which output CR LF) and now get LF, skip it */
    if (g_stdio_last_char == '\r' && ch == '\n') {
        g_stdio_last_char = ch;
        return;  /* Already sent LF after CR */
    }

    write(STDOUT_FILENO, &c, 1);

    /* Translate CR to CRLF for Unix terminal */
    if (ch == '\r') {
        c = '\n';
        write(STDOUT_FILENO, &c, 1);
    }

    g_stdio_last_char = ch;
}

static ConsoleIO g_stdio_console = {
    .read_char = stdio_read_char,
    .write_char = stdio_write_char,
    .char_available = stdio_char_available,
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
    if (g_command_buffer_pos < (int)strlen(g_command_buffer)) {
        return (unsigned char)g_command_buffer[g_command_buffer_pos++];
    }
    return -1;  /* End of buffer */
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

            /* Delete the file */
            if (path_copy[0] != '\0') {
                unlink(path_copy);
                mon_log(MON_LOG_INFO, "Scratch file %d deleted: %s", file_number, path_copy);
            }
        }
    }
}
