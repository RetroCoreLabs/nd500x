/*
 * MON 117B (79 decimal): ReadFromFile (RFILE)
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
        mon_log(MON_LOG_WARN, "MON 117B RFILE: Missing parameters (need 5, got %u)",
                ctx->arg_count);
        mon_set_error(ctx, 52);  /* Invalid parameter */
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

    mon_log(MON_LOG_DEBUG, "MON 117B RFILE: FileNo=%u, WaitFlag=%u, BuffAddr=0x%08X, BlockNo=%u, NoOfBytes=%u",
            file_no, wait_flag, buffer_addr, block_no, num_bytes);

    /* Validate file number is in mass storage range */
    if (!is_mass_storage_file(file_no)) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: Invalid file number %u (must be 64-127)", file_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Look up file in open file table */
    OpenFileEntry* entry = mon_file_table_get((int)file_no);
    if (!entry || !entry->in_use) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: File %u not open", file_no);
        mon_set_error(ctx, 53);  /* File not open */
        return MON_ERROR;
    }

    /* Check file is not mapped as segment (per SINTRAN docs:
     * "You may not use ReadFromFile on a file which is connected as a segment") */
    if (entry->mapped_as_segment) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: File %u is mapped as segment %u - use segment access",
                file_no, entry->mapped_segment_no);
        mon_set_error(ctx, 52);  /* Invalid parameter */
        return MON_ERROR;
    }

    /* Check access mode allows random reading */
    if (entry->access_mode != ACCESS_RAND_READ &&
        entry->access_mode != ACCESS_RAND_WRITE &&
        entry->access_mode != ACCESS_RAND_RDWR &&
        entry->access_mode != ACCESS_RAND_COMMON &&
        entry->access_mode != ACCESS_RAND_EXTEND) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: File %u not open for random read (access=%u)",
                file_no, entry->access_mode);
        mon_set_error(ctx, 52);  /* Invalid parameter (wrong access mode) */
        return MON_ERROR;
    }

    if (!entry->host_file) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: File %u has no host file handle", file_no);
        mon_set_error(ctx, 53);
        return MON_ERROR;
    }

    /* Validate read size */
    if (num_bytes == 0) {
        /* Nothing to read */
        mon_set_success(ctx);
        return MON_SUCCESS;
    }

    if (num_bytes > MAX_READ_SIZE) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: Read size %u exceeds max %u", num_bytes, MAX_READ_SIZE);
        mon_set_error(ctx, 52);
        return MON_ERROR;
    }

    /* Calculate file offset from block number
     * Use 64-bit arithmetic to avoid overflow with large block numbers */
    uint32_t block_size = entry->block_size ? entry->block_size : DEFAULT_BLOCK_SIZE;
    long long file_offset = (long long)block_no * block_size;

    /* Seek to the block position */
    if (fseek(entry->host_file, (long)file_offset, SEEK_SET) != 0) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: Seek to block %u (offset %lld) failed",
                block_no, file_offset);
        mon_set_error(ctx, 55);  /* End of file / seek error */
        return MON_ERROR;
    }

    /* Allocate temporary buffer and read from file */
    uint8_t* buffer = (uint8_t*)malloc(num_bytes);
    if (!buffer) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: Failed to allocate %u byte buffer", num_bytes);
        mon_set_error(ctx, 52);
        return MON_ERROR;
    }

    size_t bytes_read = fread(buffer, 1, num_bytes, entry->host_file);

    if (bytes_read == 0 && ferror(entry->host_file)) {
        mon_log(MON_LOG_WARN, "MON 117B RFILE: Read error on file %u", file_no);
        free(buffer);
        clearerr(entry->host_file);
        mon_set_error(ctx, 55);
        return MON_ERROR;
    }

    /* Write data to emulator memory */
    for (size_t i = 0; i < bytes_read; i++) {
        ctx->write_byte(ctx->cpu, buffer_addr + (uint32_t)i, buffer[i]);
    }

    /* Update file position */
    entry->current_position = (uint32_t)ftell(entry->host_file);

    mon_log(MON_LOG_DEBUG, "MON 117B RFILE: Read %zu bytes from file %u block %u, pos=%u",
            bytes_read, file_no, block_no, entry->current_position);

    free(buffer);

    /* If we read less than requested and hit EOF, still succeed but could set flag */
    if (bytes_read < num_bytes) {
        mon_log(MON_LOG_DEBUG, "MON 117B RFILE: Short read - requested %u, got %zu (EOF)",
                num_bytes, bytes_read);
    }

    mon_set_success(ctx);
    return MON_SUCCESS;
}
