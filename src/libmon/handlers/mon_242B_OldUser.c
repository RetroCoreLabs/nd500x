/*
 * MON 242B (162 decimal): OldUser (RUSCN)
 *
 * Switches back to the user name you were logged in under before NewUser. The command is similar to logging out and then log in as another user. Your program continues under the old user.
 * 
 * - The monitor call has no function if NewUser has not been executed.
 * - You may execute NewUser more than once without OldUser in between. OldUser always reset the first user name.
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_242B_OldUser(MonContext* ctx) {
    /* TODO: Implement OldUser (RUSCN) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
