/*
 * MON 41B [ROBJE/ReadObjectEntry]
 *
 * Gets information about a file or device. An object entry describes each file.
 * It contains the file name, the access rights, the date last opened for read
 * and write, the size, and more.
 *
 * Device Classification (SINTRAN Appendix B):
 *   Octal 0-77 (0-63):        Character devices -> Synthetic terminal ObjectEntry
 *   Octal 100-177 (64-127):   Open mass storage files -> OpenFileTable lookup
 *   Octal 2000-2077 (1024-1087): Terminals 65-128 -> Synthetic terminal ObjectEntry
 *   Octal 2700-2777 (1472-1535): Terminals 129-192 -> Synthetic terminal ObjectEntry
 *   Octal 3000-3077 (1536-1599): Terminals 193-256 -> Synthetic terminal ObjectEntry
 *   All others:               Unsupported -> Error 46
 *
 * Object Entry Structure (64 bytes):
 *   Offset  Size  Field
 *   ------  ----  -----
 *   0       2     Header (bit 15=Used, 14=Write, 13=Reserved, 12=Modified)
 *   2       16    ObjectName (0x27 terminated)
 *   18      4     Type/Extension (0x27 terminated)
 *   22      2     NextVersion pointer
 *   24      2     PrevVersion pointer
 *   26      2     AccessBits (Public/Friend/Own)
 *   28      2     FileType (T/P/S/I/C/A/M/L flags)
 *   30      2     DeviceNumber
 *   32      2     (Reserved)
 *   34      2     ObjectIndex (UserIndex << 8 | EntryOffset)
 *   36      2     CurrentOpenCount
 *   38      2     TotalOpenCount
 *   40      4     DateCreated
 *   44      4     LastDateOpenedForRead
 *   48      4     LastDateOpenedForWrite
 *   52      4     PagesInFile
 *   56      4     BytesInFile (stored as value-1)
 *   60      4     FilePointer (block pointer)
 *
 * Parameters:
 *   [I] FileNumber (WORD): The file/device number
 *   [O] Buff (BYTES[64]): The 64-byte object entry buffer
 *   [O] W1: Standard Error Code on error (K flag set). See appendix A.
 *
 * Errors:
 *   46 = No such filename (unsupported device type)
 *   52 = Invalid parameter (missing arguments)
 *   53 = File not open (mass storage file not in OpenFileTable)
 *
 * Reference: SINTRAN III System Supervisor (ND-830003), Appendix C
 *            ND-60052-04-EN NORD FILE SYSTEM, Section 3.1.4
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"
#include <string.h>
#include <stdio.h>

/* Helper: Write object entry to memory buffer */
static void write_object_entry_to_memory(MonContext* ctx, uint32_t addr,
                                         const uint8_t* buffer) {
    for (int i = 0; i < 64; i++) {
        ctx->write_byte(ctx->cpu, addr + i, buffer[i]);
    }
}

/* Handle character devices (0-63): Return synthetic terminal ObjectEntry */
static MonResult handle_character_device(MonContext* ctx, uint32_t device_no,
                                         uint32_t buff_addr) {
    ObjectEntry entry;
    char name[17];

    /* Generate name based on device number */
    switch (device_no) {
        case 0:
            snprintf(name, sizeof(name), "TERMINAL");
            break;
        case 1:
            snprintf(name, sizeof(name), "INPUT");
            break;
        case 2:
            snprintf(name, sizeof(name), "OUTPUT");
            break;
        case 3:
            snprintf(name, sizeof(name), "ERROR-LOG");
            break;
        default:
            snprintf(name, sizeof(name), "DEVICE-%u", device_no);
            break;
    }

    object_entry_init_terminal(&entry, name, (uint16_t)device_no);

    /* Serialize and write to memory */
    uint8_t buffer[64];
    object_entry_to_buffer(&entry, buffer);
    write_object_entry_to_memory(ctx, buff_addr, buffer);

    mon_log(MON_LOG_INFO, MON_ID_41B ": OUT: Device %o -> '%s'",
            device_no, name);

    mon_set_success(ctx);
    return MON_SUCCESS;
}

/* Handle terminals (1024+): Return synthetic terminal ObjectEntry */
static MonResult handle_terminal_device(MonContext* ctx, uint32_t device_no,
                                        uint32_t buff_addr) {
    ObjectEntry entry;
    char name[17];

    /* Calculate terminal number from device number range */
    uint32_t term_num;
    if (device_no >= 1024 && device_no <= 1087) {
        term_num = device_no - 1024 + 65;  /* Terminals 65-128 */
    } else if (device_no >= 1472 && device_no <= 1535) {
        term_num = device_no - 1472 + 129; /* Terminals 129-192 */
    } else if (device_no >= 1536 && device_no <= 1599) {
        term_num = device_no - 1536 + 193; /* Terminals 193-256 */
    } else {
        term_num = device_no;
    }

    snprintf(name, sizeof(name), "TERMINAL-%u", term_num);
    object_entry_init_terminal(&entry, name, (uint16_t)device_no);

    /* Serialize and write to memory */
    uint8_t buffer[64];
    object_entry_to_buffer(&entry, buffer);
    write_object_entry_to_memory(ctx, buff_addr, buffer);

    mon_log(MON_LOG_INFO, MON_ID_41B ": OUT: Terminal %o (device %o) -> '%s'",
            term_num, device_no, name);

    mon_set_success(ctx);
    return MON_SUCCESS;
}

/* Handle mass storage files (64-127): Look up in OpenFileTable */
static MonResult handle_mass_storage_file(MonContext* ctx, uint32_t file_number,
                                          uint32_t buff_addr) {
    OpenFileEntry* entry = mon_file_table_get((int)file_number);

    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_41B ": File %o not open", file_number);
        mon_set_error(ctx, 53);  /* File not open */
        return MON_ERROR;
    }

    /* Serialize ObjectEntry to buffer */
    uint8_t buffer[64];
    object_entry_to_buffer(&entry->object_entry, buffer);
    write_object_entry_to_memory(ctx, buff_addr, buffer);

    mon_log(MON_LOG_INFO, MON_ID_41B ": OUT: File %o -> '%s.%s'",
            file_number, entry->object_entry.object_name, entry->object_entry.type);

    mon_set_success(ctx);
    return MON_SUCCESS;
}

MonResult mon_41B_ReadObjectEntry(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 2) {
        mon_log(MON_LOG_WARN, MON_ID_41B ": Missing parameters (need 2, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read input parameter: FileNumber (WORD = 32-bit) */
    uint32_t file_number = mon_read_param_word(ctx, 0);
    uint32_t buff_addr = ctx->arg_addresses[1];

    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    mon_log(MON_LOG_DEBUG, MON_ID_41B ": IN: FileNumber=%o, BuffAddr=0x%08X",
            file_number, buff_addr);

    /*
     * Route by device class (SINTRAN Appendix B):
     *   - Character devices (0-63): Console I/O
     *   - Mass storage files (64-127): OpenFileTable lookup
     *   - Terminals (1024-1087, 1472-1535, 1536-1599): Console I/O
     *   - All others: Error 46 (unsupported)
     */
    if (is_character_device(file_number)) {
        return handle_character_device(ctx, file_number, buff_addr);
    }

    if (is_mass_storage_file(file_number)) {
        return handle_mass_storage_file(ctx, file_number, buff_addr);
    }

    if (is_terminal(file_number)) {
        return handle_terminal_device(ctx, file_number, buff_addr);
    }

    /* Unsupported device class */
    mon_log(MON_LOG_WARN, MON_ID_41B ": Unsupported device %o", file_number);
    mon_set_error(ctx, 46);  /* No such filename */
    return MON_ERROR;
}
