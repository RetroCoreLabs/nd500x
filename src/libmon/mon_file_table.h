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

/* Access mode codes (from OPEN MON call) */
#define ACCESS_SEQ_READ      0  /* Sequential read */
#define ACCESS_SEQ_WRITE     1  /* Sequential write */
#define ACCESS_RAND_READ     2  /* Random read */
#define ACCESS_RAND_WRITE    3  /* Random write */
#define ACCESS_RAND_RDWR     4  /* Random read/write */
#define ACCESS_SEQ_APPEND    5  /* Sequential append */
#define ACCESS_SEQ_COMMON    6  /* Sequential common */
#define ACCESS_RAND_COMMON   7  /* Random common */
#define ACCESS_SEQ_EXTEND    8  /* Sequential extend */
#define ACCESS_RAND_EXTEND   9  /* Random extend */

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
    ObjectEntry object_entry;
    uint32_t current_position;
    uint8_t access_mode;
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
} ConsoleIO;

void mon_file_table_set_console(ConsoleIO* console);

/* Returns the console I/O handler, or NULL if not set.
 * Callers must check for NULL before using the returned pointer. */
ConsoleIO* mon_file_table_get_console(void);

#endif /* MON_FILE_TABLE_H */
