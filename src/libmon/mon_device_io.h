/*
 * Shared device string I/O cores for the SINTRAN DVxxx monitor calls.
 *
 * MON 503B DVINST (input), MON 504B DVOUTS (output) and MON 511B DVIO
 * (fused output-then-input) are three views of the same two operations.
 * These cores hold the behaviour once so 511B can REUSE 503B/504B rather
 * than duplicate them.
 *
 * ARGUMENT-INDEX NOTE (why the cores take explicit values instead of
 * reading everything from ctx): DVINST and DVIO agree on argument indices
 * 0 and 3..13, but DVIO relocates DVINST's MaxNo and returned-count to
 * indices 14 and 15 in order to free indices 1..2 for the output phase.
 * Established from the ND LINKER's own call sites - see mon_511B_DVIO.c.
 *
 *   DVINST: 0 DevNo, 1 MaxNo, 2 @retcount, 3 @buf, 4..13 strategies+tables
 *   DVIO:   0 DevNo, 1 NoOfBytes, 2 @outbuf, 3 @buf, 4..13 strategies+tables,
 *           14 MaxNo, 15 @retcount
 *
 * The strategy/table arguments (4..13) are read from ctx at those FIXED
 * indices by mon_dvinst_read(), which is correct for BOTH calls.
 */

#ifndef MON_DEVICE_IO_H
#define MON_DEVICE_IO_H

#include "mon_types.h"

/*
 * DVOUTS core: write num_bytes from buffer_addr to device_no.
 * Routes by device class (character/terminal -> console, mass storage -> file).
 * On failure sets the MON error on ctx and returns MON_ERROR.
 * Does NOT set success on ctx - the caller owns the final status, because
 * 511B must run the input phase afterwards.
 */
MonResult mon_dvouts_write(MonContext* ctx, uint32_t device_no,
                           uint32_t num_bytes, uint32_t buffer_addr);

/*
 * DVINST core: read up to max_bytes from device_no into buffer_addr, then
 * write the byte count to argument index ret_count_param_idx.
 *
 * Break/echo strategies and the user tables are read from ctx at the fixed
 * indices 4..13 (identical for DVINST and DVIO - see note above).
 *
 * May request a blocking wait (ctx->wait_requested) when the device has no
 * input; in that case the MON call is left UNCOMMITTED and the CPU rewinds
 * to the CALLG, so the caller must return MON_SUCCESS immediately without
 * committing any state.
 *
 * Does NOT set success on ctx - the caller owns the final status.
 */
MonResult mon_dvinst_read(MonContext* ctx, uint32_t device_no,
                          uint32_t max_bytes, int ret_count_param_idx,
                          uint32_t buffer_addr);

#endif /* MON_DEVICE_IO_H */
