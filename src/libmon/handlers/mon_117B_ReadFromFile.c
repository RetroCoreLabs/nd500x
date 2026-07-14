/*
 * MON 117B [RFILE/ReadFromFile]
 *
 * Reads any number of bytes from a file. The read operation must start
 * at the beginning of a block. The file must be opened for random read access.
 *
 * - The standard block size is 512 bytes. You can change this with SetBlockSize.
 * - The first block is number 0.
 * - Access code D (direct transfer) requires block size multiple of page size.
 * - Peripheral files are always read sequentially.
 *
 * Parameters:
 *   [I] FileNo (INTEGER): File number (64-127 for mass storage)
 *   [I] WaitFlag (INTEGER): Wait flag (0=return immediately if busy)
 *   [O] Buff (BYTES): Buffer address to store data
 *   [I] BlockNo (INTEGER): Block number (0-based)
 *   [I] NoOfBytes (LONGINT): Number of bytes to read
 *
 * Returns:
 *   K flag set on error, error code in W1:
 *     52 = Invalid parameter
 *     53 = File not open
 *     55 = End of file reached
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"
#include "../mon_file_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default block size - can be changed by MON 076B SETBS */
#define DEFAULT_BLOCK_SIZE 512
#define MAX_READ_SIZE      65536  /* Reasonable max for single read */

MonResult mon_117B_ReadFromFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 5) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": Missing parameters (need 5, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, MON_ERR_MISSING_PARAMETER);  /* 157B Missing parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t wait_flag = mon_read_param_word(ctx, 1);
    uint32_t buffer_addr = ctx->arg_addresses[2];  /* Output buffer */
    uint32_t block_no = mon_read_param_word(ctx, 3);
    uint32_t num_bytes = mon_read_param_word(ctx, 4);

    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "WaitFlag");
    MON_LOG_IN_WORD(ctx, 3, "BlockNo");
    MON_LOG_IN_WORD(ctx, 4, "NoOfBytes");

    mon_log(MON_LOG_DEBUG, MON_ID_117B ": IN: FileNo=%o, WaitFlag=%o, BuffAddr=0x%08X, BlockNo=%o, NoOfBytes=%o",
            file_no, wait_flag, buffer_addr, block_no, num_bytes);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": Invalid file number %o (must be %o-%o)", file_no, 64, 127);
        mon_set_error(ctx, MON_ERR_FILE_NUMBER_RANGE);  /* 127B File number out of range */
        return MON_ERROR;
    }

    /* Look up file in open file table */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": File %o not open", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Check file is not mapped as segment (per SINTRAN docs:
     * "You may not use ReadFromFile on a file which is connected as a segment") */
    if (entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": File %o is mapped as segment %o - use segment access",
                file_no, entry->mapped_segment_no);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Check access mode allows random reading
     * Valid modes: ACCESS_RAND_RDWR (2), ACCESS_RAND_READ (3),
     *              ACCESS_RAND_RDWR_CTG (6), ACCESS_RAND_READ_CTG (7),
     *              ACCESS_RAND_RDWR_RT (8), ACCESS_RAND_EXTEND (9)
     */
    if (entry->access_mode != ACCESS_RAND_RDWR &&
        entry->access_mode != ACCESS_RAND_READ &&
        entry->access_mode != ACCESS_RAND_RDWR_CTG &&
        entry->access_mode != ACCESS_RAND_READ_CTG &&
        entry->access_mode != ACCESS_RAND_RDWR_RT &&
        entry->access_mode != ACCESS_RAND_EXTEND) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": File %o not open for random read (access=%o)",
                file_no, entry->access_mode);
        mon_set_error(ctx, MON_ERR_NOT_OPEN_RAND_READ);  /* 126B */
        return MON_ERROR;
    }

    if (!entry->host_file) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": File %o has no host file handle", file_no);
        mon_set_error(ctx, MON_ERR_FILE_NOT_OPEN);  /* 132B No file opened with this number */
        return MON_ERROR;
    }

    /* Validate read size */
    if (num_bytes == 0) {
        /* Nothing to read */
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    if (num_bytes > MAX_READ_SIZE) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": Read size %o exceeds max %o", num_bytes, MAX_READ_SIZE);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    }

    /* Position the file.
     *
     * BlockNo is a SIGNED halfword: -1 means "read the next block", i.e. carry
     * on from wherever the previous transfer left off, without seeking. This is
     * how sequential streaming through RFILE works. Callers sign-extend it to a
     * word (PLANC emits H WCONV), so -1 arrives here as 0xFFFFFFFF; treating it
     * as an unsigned block index seeks past the end and fails every read. */
    uint32_t block_size = entry->block_size ? entry->block_size : DEFAULT_BLOCK_SIZE;
    int32_t block_no_signed = (int32_t)block_no;

    if (block_no_signed == -1) {
        /* Next block: resume at the recorded position. */
        if (fseek(entry->host_file, (long)entry->current_position, SEEK_SET) != 0) {
            mon_log(MON_LOG_WARN, MON_ID_117B ": Seek to next block (pos %o) failed",
                    entry->current_position);
            mon_set_error(ctx, MON_ERR_END_OF_FILE);  /* 003B End of file */
            return MON_ERROR;
        }
    } else if (block_no_signed < 0) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": Negative block number %d (only -1 is legal)",
                block_no_signed);
        mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
        return MON_ERROR;
    } else {
        /* Use 64-bit arithmetic to avoid overflow with large block numbers */
        long long file_offset = (long long)block_no * block_size;

        if (fseek(entry->host_file, (long)file_offset, SEEK_SET) != 0) {
            mon_log(MON_LOG_WARN, MON_ID_117B ": Seek to block %o (offset %lld) failed",
                    block_no, file_offset);
            mon_set_error(ctx, MON_ERR_END_OF_FILE);  /* 003B End of file */
            return MON_ERROR;
        }
    }

    /* Allocate temporary buffer and read from file */
    uint8_t* buffer = (uint8_t*)malloc(num_bytes);
    if (!buffer) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": Failed to allocate %o byte buffer", num_bytes);
        mon_set_error(ctx, MON_ERR_NO_BUFFER_SPACE);  /* 131B No more buffer space */
        return MON_ERROR;
    }

    size_t bytes_read = fread(buffer, 1, num_bytes, entry->host_file);

    if (bytes_read == 0 && ferror(entry->host_file)) {
        mon_log(MON_LOG_WARN, MON_ID_117B ": Read error on file %o", file_no);
        free(buffer);
        clearerr(entry->host_file);
        mon_set_error(ctx, MON_ERR_TRANSFER_ERROR);  /* 141B Transfer error */
        return MON_ERROR;
    }

    /* Write data to emulator memory */
    for (size_t i = 0; i < bytes_read; i++) {
        ctx->write_byte(ctx->cpu, buffer_addr + (uint32_t)i, buffer[i]);
    }

    /* Update file position */
    entry->current_position = (uint32_t)ftell(entry->host_file);

    mon_log(MON_LOG_DEBUG, MON_ID_117B ": OUT: Read %zu bytes from file %o block %o, pos=%o",
            bytes_read, file_no, block_no, entry->current_position);

    free(buffer);

    /* Return contract per the carved L07 RFILE (006-S3FS worker 102130B):
     *  - Error 3 (end-of-file) is a RANGE check raised only when the requested
     *    block/position is ENTIRELY beyond the file's data (carve: bounds test
     *    102364-102402, SAA 3 @ 102403). A block that exists but holds fewer
     *    bytes than requested PASSES and does the read.
     *  - A within-bounds read - even a short final block at EOF - returns
     *    SUCCESS (K clear) and writes the ACTUAL transferred byte count back to
     *    the caller (carve success path 102433-102470: recompute count -> B+16 ->
     *    stored via rec[26]). That count is how the reader learns the length; it
     *    is NOT signalled by an error.
     * The previous handler returned error 3 on every short read, so NC reading
     * 4096 bytes of a 23-byte source saw K-set/EXIT-ERROR, treated the source as
     * unreadable, and rejected valid declarations ("IDENTIFIER deleted"). */
    if (bytes_read == 0) {
        /* Nothing available at the requested position -> genuinely at/past EOF. */
        mon_log(MON_LOG_DEBUG, MON_ID_117B ": EOF - requested %u bytes at block %u, got 0",
                num_bytes, block_no);
        mon_set_error(ctx, MON_ERR_END_OF_FILE);  /* 003B End of file */
        return MON_ERROR;
    }    /* Success (full or short read): report the actual bytes transferred back to
     * the caller in the NoOfBytes parameter (arg 4), matching the carve's
     * count-write-back, so the reader knows exactly how many bytes it got. */
    mon_write_param_word(ctx, 4, (uint32_t)bytes_read);
    if (bytes_read < num_bytes) {
        mon_log(MON_LOG_DEBUG, MON_ID_117B ": Short read - requested %u, got %zu (SUCCESS, count returned)",
                num_bytes, bytes_read);
    }
    mon_set_success(ctx);
    return MON_SUCCESS;
}
