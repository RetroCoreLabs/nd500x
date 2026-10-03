/*
 * nd_diag_budget.h - a bounded diagnostic that says when it stops
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef ND_DIAG_BUDGET_H
#define ND_DIAG_BUDGET_H

#include <stdio.h>

/*
 * THE PROBLEM THIS SOLVES, MEASURED.
 *
 * Bounded diagnostics are everywhere in this tree and in nd100x's bridge,
 * written as a local counter:
 *
 *     static unsigned n = 0;
 *     if (n++ < 40u) { ... print ... }
 *
 * That is the right shape - an unbounded per-instruction print is useless and
 * slow - but it is SILENT when it runs out, and a silent instrument is worse
 * than no instrument. Its silence is indistinguishable from the thing it was
 * watching never happening, and that silence has repeatedly been read as a
 * fact about the machine.
 *
 * The incident: a diagnostic on a CALL instruction spent its budget of 40 at
 * log line 3327, while the process under investigation did not start until
 * line 98586. The absence of any line for that process was read as "this code
 * path is never entered", and a theory was built on it. The budget had simply
 * run out 95,000 lines earlier.
 *
 * ND_DIAG_BUDGET prints one line when the budget is exhausted, naming itself
 * and its limit, so the log says "I stopped reporting here" instead of saying
 * nothing. See docs/INVESTIGATION-TRAPS.md section 2.
 *
 * USAGE
 *
 *     ND_DIAG_BUDGET(calls, 40);          // declare the budget, once per site
 *     if (ND_DIAG_TAKE(calls)) {
 *         fprintf(stderr, "[CALLDBG] ...\n");
 *     }
 *
 * The declaration is a block-scope static, so the budget is per call SITE, not
 * per caller - the same as the hand-written counters it replaces.
 *
 * NOT FOR src/ndbus/. That directory links nothing from the emulator and must
 * stay liftable into its own repository - tools/check_ndbus_isolation.sh
 * enforces it. This header's NAME slips past that pattern, but using it there
 * would couple the directories for no linkage benefit, so the bounded watches
 * in src/ndbus keep their own counters.
 *
 * WHAT IT DOES NOT DO. It does not decide WHERE to aim an instrument, and a
 * budget is the wrong gate when the interesting event is by definition the one
 * a count stops printing. For that, gate on the address or the subject instead;
 * a count cannot be made correct by announcing itself.
 */

/** @brief Declare a named diagnostic budget at a call site. */
#define ND_DIAG_BUDGET(name, limit)                                            \
    static unsigned long nd_diag_##name##_used = 0ul;                          \
    static const unsigned long nd_diag_##name##_limit = (unsigned long)(limit)

/**
 * @brief Consume one unit of a declared budget.
 *
 * @return Nonzero while the budget has room. On the transition to exhausted it
 *         prints one line naming the budget and its limit, then returns zero
 *         for every later call without printing again.
 */
#define ND_DIAG_TAKE(name)                                                     \
    (nd_diag_##name##_used < nd_diag_##name##_limit                            \
         ? (nd_diag_##name##_used++, 1)                                        \
         : (nd_diag_##name##_used == nd_diag_##name##_limit                    \
                ? (nd_diag_##name##_used++,                                    \
                   fprintf(stderr,                                             \
                           "[DIAG] %s: budget of %lu spent at %s:%d - later "  \
                           "events are NOT logged, so silence past this point "\
                           "is not evidence\n",                                \
                           #name, nd_diag_##name##_limit, __FILE__, __LINE__), \
                   fflush(stderr), 0)                                          \
                : 0))

/** @brief How much of a declared budget has been consumed, for a summary line. */
#define ND_DIAG_USED(name) (nd_diag_##name##_used)

/** @brief Nonzero once a declared budget has been exhausted. */
#define ND_DIAG_SPENT(name) (nd_diag_##name##_used > nd_diag_##name##_limit)

#endif /* ND_DIAG_BUDGET_H */
