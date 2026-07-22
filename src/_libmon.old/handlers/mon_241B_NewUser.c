/*
 * MON 241B (161 decimal): NewUser (SUSCN)
 *
 * Switches the user name you are logged in under. The command is similar to logging out and then logging in as another user. Your program continues under this user name.
 * 
 * - Restore the old user name with OldUser.
 * - OldUser always resets the first user name. From the ND-100 you may execute NewUser more than once without OldUser in between. This is not the case from the ND-500.
 * - If originally logged in as user RT, it is not possible to log in as user SYSTEM.
 *
 * Parameters:
 *   [I] UserName (STRING): input
 *   [I] UserPassword (INTEGER2): input
 *   [I] ProjectPassword (STRING): input
 *   [O] UserType (INTEGER2): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_241B_NewUser(MonContext* ctx) {
    /* TODO: Implement NewUser (SUSCN) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "UserName");
    MON_LOG_IN_WORD(ctx, 1, "UserPassword");
    MON_LOG_IN_WORD(ctx, 2, "ProjectPassword");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
