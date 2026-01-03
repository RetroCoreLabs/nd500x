/*
 * MON 120B [WFILE/WriteToFile]
 *
 * Writes any number of bytes to a file. The write operation must start
 * at the beginning of a block. The file must be opened for random write access.
 *
 * - The standard block size is 512 bytes. You can change this with SetBlockSize.
 * - The first block is number 0.
 * - Access code D (direct transfer) requires block size multiple of page size.
 * - Peripheral files are always written sequentially.
 *
 * Parameters:
 *   [I] FileNo (INTEGER): File number (64-127 for mass storage)
 *   [I] ReturnFlag (INTEGER): Return flag (0=return immediately if busy)
 *   [I] Buff (BYTES): Buffer address containing data to write
 *   [I] BlockNo (INTEGER): Block number (0-based)
 *   [I] NoOfBytes (LONGINT): Number of bytes to write
 *
 * Returns:
 *   K flag set on error, error code in W1:
 *     52 = Invalid parameter
 *     53 = File not open
 *     55 = Write error
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_file_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default block size - can be changed by MON 076B SETBS */
#define DEFAULT_BLOCK_SIZE 512
#define MAX_WRITE_SIZE     65536  /* Reasonable max for single write */

MonResult mon_120B_WriteToFile(MonContext* ctx) {
    /* Defensive check for argument count */
    if (ctx->arg_count < 5) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": Missing parameters (need 5, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Read parameters */
    uint32_t file_no = mon_read_param_word(ctx, 0);
    uint32_t return_flag = mon_read_param_word(ctx, 1);
    uint32_t buffer_addr = ctx->arg_addresses[2];  /* Input buffer */
    uint32_t block_no = mon_read_param_word(ctx, 3);
    uint32_t num_bytes = mon_read_param_word(ctx, 4);

    MON_LOG_IN_WORD(ctx, 0, "FileNo");
    MON_LOG_IN_WORD(ctx, 1, "ReturnFlag");
    MON_LOG_IN_WORD(ctx, 3, "BlockNo");
    MON_LOG_IN_WORD(ctx, 4, "NoOfBytes");

    mon_log(MON_LOG_DEBUG, MON_ID_120B ": IN: FileNo=%o, ReturnFlag=%o, BuffAddr=0x%08X, BlockNo=%o, NoOfBytes=%o",
            file_no, return_flag, buffer_addr, block_no, num_bytes);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": Invalid file number %o (must be %o-%o)", file_no, 64, 127);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Look up file in open file table */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": File %o not open", file_no);
        mon_set_error(ctx, 53);  /* File not open */
        return MON_ERROR;
    }

    /* Check file is not mapped as segment (per SINTRAN docs:
     * "You may not use WriteToFile on a file which is connected as a segment") */
    if (entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": File %o is mapped as segment %o - use segment access",
                file_no, entry->mapped_segment_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Check access mode allows random writing
     * Valid modes: ACCESS_RAND_RDWR (2), ACCESS_RAND_RDWR_CTG (6),
     *              ACCESS_RAND_RDWR_RT (8), ACCESS_RAND_EXTEND (9)
     */
    if (entry->access_mode != ACCESS_RAND_RDWR &&
        entry->access_mode != ACCESS_RAND_RDWR_CTG &&
        entry->access_mode != ACCESS_RAND_RDWR_RT &&
        entry->access_mode != ACCESS_RAND_EXTEND) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": File %o not open for random write (access=%o)",
                file_no, entry->access_mode);
        mon_set_error(ctx, 52);  /* Invalid parameter (wrong access mode) */
        return MON_ERROR;
    }

    if (!entry->host_file) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": File %o has no host file handle", file_no);
        mon_set_error(ctx, 53);
        return MON_ERROR;
    }

    /* Validate write size */
    if (num_bytes == 0) {
        /* Nothing to write */
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    if (num_bytes > MAX_WRITE_SIZE) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": Write size %o exceeds max %o", num_bytes, MAX_WRITE_SIZE);
        mon_set_error(ctx, 52);
        return MON_ERROR;
    }

    /* Calculate file offset from block number
     * Use 64-bit arithmetic to avoid overflow with large block numbers */
    uint32_t block_size = entry->block_size ? entry->block_size : DEFAULT_BLOCK_SIZE;
    long long file_offset = (long long)block_no * block_size;

    /* Seek to the block position */
    if (fseek(entry->host_file, (long)file_offset, SEEK_SET) != 0) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": Seek to block %o (offset %lld) failed",
                block_no, file_offset);
        mon_set_error(ctx, 55);  /* Seek error */
        return MON_ERROR;
    }

    /* Allocate temporary buffer and read from emulator memory */
    uint8_t* buffer = (uint8_t*)malloc(num_bytes);
    if (!buffer) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": Failed to allocate %o byte buffer", num_bytes);
        mon_set_error(ctx, 52);
        return MON_ERROR;
    }

    /* Read data from emulator memory */
    for (uint32_t i = 0; i < num_bytes; i++) {
        buffer[i] = ctx->read_byte(ctx->cpu, buffer_addr + i);
    }

    /* Write to file */
    size_t bytes_written = fwrite(buffer, 1, num_bytes, entry->host_file);

    if (bytes_written != num_bytes) {
        mon_log(MON_LOG_WARN, MON_ID_120B ": Write error on file %o (wrote %zu of %o)",
                file_no, bytes_written, num_bytes);
        free(buffer);
        mon_set_error(ctx, 55);
        return MON_ERROR;
    }

    /* Flush to ensure data is written */
    fflush(entry->host_file);

    /* Update file position */
    entry->current_position = (uint32_t)ftell(entry->host_file);

    /* Update bytes_in_file if we extended the file */
    if (entry->current_position > entry->object_entry.bytes_in_file) {
        entry->object_entry.bytes_in_file = entry->current_position;
    }

    mon_log(MON_LOG_DEBUG, MON_ID_120B ": OUT: Wrote %zu bytes to file %o block %o, pos=%o",
            bytes_written, file_no, block_no, entry->current_position);

    free(buffer);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
