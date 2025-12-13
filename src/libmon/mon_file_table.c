/*
 * SINTRAN III File System Tables Implementation
 *
 * Implements device reservations and open file tracking for MON calls.
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#include "mon_file_table.h"
#include "mon.h"
#include <string.h>
#include <stdlib.h>

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
    console_io = NULL;
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

int mon_file_open_ex(const char* filename, const char* filetype, uint8_t access_mode, int requested_file_no) {
    int free_slot = -1;

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

    /* Construct host file path
     * For now, look in current directory or a configured path
     * TODO: Add configurable search path */
    char host_path[256];
    if (filetype && filetype[0]) {
        snprintf(host_path, sizeof(host_path), "%s.%s", filename, filetype);
    } else {
        snprintf(host_path, sizeof(host_path), "%s", filename);
    }

    /* Determine fopen mode based on access code */
    const char* fmode;
    switch (access_mode) {
        case ACCESS_SEQ_READ:
        case ACCESS_RAND_READ:
            fmode = "rb";
            break;
        case ACCESS_SEQ_WRITE:
        case ACCESS_RAND_WRITE:
            fmode = "wb";
            break;
        case ACCESS_RAND_RDWR:
            fmode = "r+b";
            break;
        case ACCESS_SEQ_APPEND:
        case ACCESS_SEQ_EXTEND:
        case ACCESS_RAND_EXTEND:
            fmode = "ab";
            break;
        default:
            fmode = "rb";
            break;
    }

    /* Try to open the host file */
    FILE* fp = fopen(host_path, fmode);
    if (!fp) {
        /* For write modes, try creating the file */
        if (access_mode == ACCESS_SEQ_WRITE || access_mode == ACCESS_RAND_WRITE) {
            fp = fopen(host_path, "w+b");
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
    entry->access_mode = access_mode;
    entry->current_position = 0;
    entry->block_size = 512;  /* Default block size */
    entry->host_file = fp;
    strncpy(entry->host_path, host_path, sizeof(entry->host_path) - 1);

    /* Initialize ObjectEntry */
    object_entry_init_file(&entry->object_entry, filename, filetype,
                          (uint32_t)file_size, FILETYPE_INDEXED);
    entry->object_entry.current_open_count = 1;
    entry->object_entry.total_open_count = 1;
    entry->object_entry.object_index = (uint16_t)file_number;
    entry->object_entry.header = HEADER_USED;

    mon_log(MON_LOG_INFO, "MON OPEN: Opened '%s' as file number %d (mode=%d)",
              host_path, file_number, access_mode);

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

    /* Close host file */
    if (entry->host_file) {
        fclose(entry->host_file);
    }

    mon_log(MON_LOG_INFO, "MON CLOSE: Closed file number %d ('%s')",
              file_number, entry->host_path);

    /* Clear entry */
    memset(entry, 0, sizeof(OpenFileEntry));

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
 * Host Path Utilities
 * ============================================================ */

void mon_build_host_path(const char* filename, char* host_path, size_t max_len) {
    /* Simple mapping: use filename as-is, add .dat extension if no extension present */
    const char* dot = strrchr(filename, '.');
    if (dot) {
        snprintf(host_path, max_len, "%s", filename);
    } else {
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
