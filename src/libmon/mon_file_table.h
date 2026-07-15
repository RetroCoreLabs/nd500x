/*
 * SINTRAN III File System Tables
 *
 * Tracks device reservations (MON 122/123) and open files (MON 50/43).
 * Used by file-related MON calls including ReadObjectEntry (41B).
 *
 * THREAD SAFETY: This module uses static global tables without mutex protection.
 * It is designed for single-threaded access only. If the emulator uses multiple
 * threads (e.g., debugger thread + execution thread), external synchronization
 * is required before calling any functions in this module.
 *
 * Reference: SINTRAN III System Supervisor (ND-830003)
 */

#ifndef MON_FILE_TABLE_H
#define MON_FILE_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* SINTRAN logical device number ranges (from Appendix B) */
#define FILE_NUMBER_MIN     64   /* Octal 100 - start of mass storage files */
#define FILE_NUMBER_MAX     127  /* Octal 177 - end of mass storage files */
#define FILE_TABLE_SIZE     64   /* 127 - 64 + 1 */
#define MAX_DEVICES         2048 /* Max device number for reservations */
#define SINTRAN_PAGE_SIZE   2048 /* SINTRAN file system page size in bytes */

/* File type flags (from SINTRAN file system) */
#define FILETYPE_TERMINAL    (1 << 0)  /* T - Terminal file */
#define FILETYPE_PERIPHERAL  (1 << 1)  /* P - Peripheral file */
#define FILETYPE_SPOOLING    (1 << 2)  /* S - Spooling file */
#define FILETYPE_INDEXED     (1 << 3)  /* I - Indexed file */
#define FILETYPE_CONTIGUOUS  (1 << 4)  /* C - Contiguous file */
#define FILETYPE_ALLOCATED   (1 << 5)  /* A - Allocated file */
#define FILETYPE_MAGTAPE     (1 << 6)  /* M - Magnetic tape file */
#define FILETYPE_LIBRARY     (1 << 7)  /* L - Library file */

/* Header bits for ObjectEntry */
#define HEADER_USED          (1 << 15) /* U - Entry used */
#define HEADER_WRITE_OPEN    (1 << 14) /* W - Currently opened for write */
#define HEADER_RESERVED      (1 << 13) /* R - Reserved */
#define HEADER_MODIFIED      (1 << 12) /* M - File modified */

/* Access mode codes (from OPEN MON call - per MASTER reference ND-860228.2 EN)
 *   0 = Sequential write
 *   1 = Sequential read
 *   2 = Random read or write
 *   3 = Random read only
 *   4 = Sequential read or write
 *   5 = Sequential write append
 *   6 = Random read or write common on contiguous files
 *   7 = Random read common on contiguous files
 *   8 = Random read or write on contiguous files (direct transfer for RT)
 *   9 = Random read, write append for WriteToFile
 */
#define ACCESS_SEQ_WRITE     0  /* Sequential write */
#define ACCESS_SEQ_READ      1  /* Sequential read */
#define ACCESS_RAND_RDWR     2  /* Random read or write */
#define ACCESS_RAND_READ     3  /* Random read only */
#define ACCESS_SEQ_RDWR      4  /* Sequential read or write */
#define ACCESS_SEQ_APPEND    5  /* Sequential write append */
#define ACCESS_RAND_RDWR_CTG 6  /* Random r/w common on contiguous */
#define ACCESS_RAND_READ_CTG 7  /* Random read common on contiguous */
#define ACCESS_RAND_RDWR_RT  8  /* Random r/w direct transfer for RT */
#define ACCESS_RAND_EXTEND   9  /* Random read, write append */

/* I/O flags for reservation (MON 122/123) */
#define IO_FLAG_INPUT        0
#define IO_FLAG_OUTPUT       1

/* SINTRAN string terminator */
#define SINTRAN_STRING_END   0x27

/* ObjectEntry - 64-byte file metadata structure */
typedef struct {
    uint16_t header;              /* Offset 0: Status bits */
    char object_name[16];         /* Offset 2: Filename (0x27 terminated) */
    char type[4];                 /* Offset 18: Extension (0x27 terminated) */
    uint16_t next_version;        /* Offset 22 */
    uint16_t prev_version;        /* Offset 24 */
    uint16_t access_bits;         /* Offset 26 */
    uint16_t file_type;           /* Offset 28 */
    uint16_t device_number;       /* Offset 30 */
    uint16_t reserved;            /* Offset 32 */
    uint16_t object_index;        /* Offset 34 */
    uint16_t current_open_count;  /* Offset 36 */
    uint16_t total_open_count;    /* Offset 38 */
    uint32_t date_created;        /* Offset 40 */
    uint32_t date_read;           /* Offset 44 */
    uint32_t date_written;        /* Offset 48 */
    uint32_t pages_in_file;       /* Offset 52 */
    uint32_t bytes_in_file;       /* Offset 56 */
    uint32_t file_pointer;        /* Offset 60 */
} ObjectEntry;

/* Reservation entry - tracks device reservations */
typedef struct {
    bool reserved_input;
    bool reserved_output;
    uint16_t owner_process;
} ReservationEntry;

/* Open file entry - runtime tracking */
typedef struct {
    bool in_use;
    bool is_scratch;              /* True if scratch file (delete on close) */
    ObjectEntry object_entry;
    uint32_t current_position;
    uint8_t access_mode;
    uint32_t block_size;          /* Block size for RFILE/WFILE (default 512) */
    bool max_bytes_set;           /* True if MON 73B SMAX recorded a logical length
                                   * in object_entry.bytes_in_file to apply at CLOSE */
    bool mapped_as_segment;       /* True if connected as segment (MON 412B) */
    uint32_t mapped_segment_no;   /* Logical segment number if mapped */
    uint8_t segment_access_type;  /* 0=read, 1=write, 2=read/write */
    char host_path[256];          /* Path to host file */
    FILE* host_file;              /* Host file handle */
} OpenFileEntry;

/* Initialization */
void mon_file_table_init(void);
void mon_file_table_reset(void);

/* Reservation API (MON 122/123) */
int mon_reserve_device(uint32_t device_no, uint8_t io_flag, bool wait);
int mon_release_device(uint32_t device_no, uint8_t io_flag);
bool mon_is_device_reserved(uint32_t device_no, uint8_t io_flag);

/* Open File API (MON 50/43) */
int mon_file_open_ex(const char* filename, const char* filetype, uint8_t access_mode, int requested_file_no);
int mon_file_open(const char* filename, const char* filetype, uint8_t access_mode);
int mon_file_close(int file_number);
OpenFileEntry* mon_file_table_get(int file_number);
bool mon_file_table_is_valid_file_number(int file_number);

/* ObjectEntry serialization */
void object_entry_to_buffer(const ObjectEntry* entry, uint8_t buffer[64]);
void object_entry_from_buffer(ObjectEntry* entry, const uint8_t buffer[64]);

/* ObjectEntry initialization helpers */
void object_entry_init_terminal(ObjectEntry* entry, const char* name, uint16_t device_number);
void object_entry_init_file(ObjectEntry* entry, const char* name, const char* type,
                           uint32_t size_bytes, uint16_t file_type_flags);

/* Device classification (based on SINTRAN Appendix B) */
bool is_character_device(uint32_t dev);
bool is_mass_storage_file(uint32_t dev);
bool is_terminal(uint32_t dev);

/* Console I/O support (for INBT/OUTBT integration)
 * Character devices (0-63) and Terminals use these for I/O */
typedef struct {
    int (*read_char)(void* ctx);              /* Read single character */
    void (*write_char)(void* ctx, int ch);    /* Write single character */
    bool (*char_available)(void* ctx);        /* Check if input available */
    void* context;                            /* User context pointer */
    /* Optional: invoked when an ESCAPE (user-break) character is read on a
     * terminal whose escape is ENABLED (SINTRAN DFLAG.5IESC clear). The terminal
     * front-end implements the actual break/abort. May be NULL. */
    void (*user_break)(void* ctx, uint32_t device_no);
    /* Optional: BLOCK until at least one input character is available, then
     * return 1 (true). Used by an interactive front-end to honour SINTRAN's
     * "the program waits if there is no bytes in the input buffer" semantics
     * for a real terminal: when a blocking INBT/terminal read finds no input,
     * the run loop calls this to wait for the user instead of stopping. Returns
     * 0 if no input can ever arrive (e.g. EOF on a redirected stdin). NULL for
     * scripted/headless consoles, where the host decides on STOP_WAIT_INPUT. */
    int (*wait_for_input)(void* ctx);
} ConsoleIO;

/* Block until the currently-installed console has input, honouring a blocking
 * terminal read. Returns 1 if input is now available, 0 if the console cannot
 * block (no wait_for_input handler) or input can never arrive. */
int mon_console_wait_for_input(void);

void mon_file_table_set_console(ConsoleIO* console);

/* Returns the console I/O handler, or NULL if not set.
 * Callers must check for NULL before using the returned pointer. */
ConsoleIO* mon_file_table_get_console(void);

/* Queued Console I/O - for testing and scripted input.
 * Queues input to be read by MON calls like 1B (INBT) and 503B (DVINST).
 * Automatically installs a queue-based console handler.
 * Supports escape sequences: \r = CR, \n = LF */
void mon_queue_console_input(const char* input);

/* Clear the console input queue and output buffer */
void mon_clear_console_queue(void);

/* Get the console output that has been written (e.g., echo from MON 503B) */
const char* mon_get_console_output(void);
size_t mon_get_console_output_len(void);

/* Get remaining chars in input queue (for debugging) */
size_t mon_get_console_input_remaining(void);

/* Standard I/O Console - uses stdin/stdout for interactive mode */
void mon_install_stdio_console(void);

/* Host path utilities for file operations */
void mon_build_host_path(const char* filename, char* host_path, size_t max_len);

/* Command buffer support (MON 12B SETCM)
 * THREAD SAFETY: These functions use static global state without mutex protection.
 * External synchronization required if accessed from multiple threads. */
const char* mon_get_command_buffer(void);
int mon_read_command_buffer_char(void);
void mon_reset_command_buffer_pos(void);
void mon_set_command_buffer(const char* command);

/* ============================================================
 * Scratch File Support
 *
 * Scratch files are temporary files deleted when closed.
 * Files are numbered 64-127 (octal 100-177), assigned sequentially.
 * ============================================================ */

/**
 * Open a scratch file (temporary, deleted on close).
 *
 * Parameters:
 *   filename - SINTRAN filename (e.g., "(SCRATCH)SCRATCH64")
 *   filetype - File type/extension (e.g., "DATA")
 *
 * Returns: file number (64-127) on success, -1 on error
 */
int mon_open_scratch_file(const char* filename, const char* filetype);

/**
 * Check if a file is marked as scratch (delete on close).
 */
bool mon_is_scratch_file(int file_number);

/**
 * Close all scratch files and delete them.
 */
void mon_close_all_scratch_files(void);

/**
 * Register cleanup handler for atexit.
 *
 * Ensures all open files are closed and scratch files deleted
 * when the emulator exits.
 */
void mon_file_table_register_cleanup(void);

/**
 * Populate ObjectEntry from host file stats.
 *
 * Maps host file metadata to SINTRAN ObjectEntry structure:
 *   stat.st_size -> BytesInFile, PagesInFile
 *   stat.st_ctime -> DateCreated
 *   stat.st_atime -> DateRead
 *   stat.st_mtime -> DateWritten
 *
 * Parameters:
 *   entry - ObjectEntry to populate
 *   host_path - Path to host file
 *   sintran_name - SINTRAN filename (for ObjectName)
 *   sintran_type - SINTRAN extension (for Type)
 *
 * Returns: 0 on success, -1 on error
 */
int mon_populate_object_entry_from_host(ObjectEntry* entry,
                                         const char* host_path,
                                         const char* sintran_name,
                                         const char* sintran_type);

#endif /* MON_FILE_TABLE_H */
