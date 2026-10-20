/**
 * @brief A DECORATE weapon block that ends in "stop" removes the psprite,
 * as in ZDoom, instead of freezing on its last frame. HACX's pistol flash
 * ends "PISF A 0 bright A_Light0; stop": frozen, that 0-tic frame pointed
 * at itself and P_SetPsprite spun on it forever at the first shot; a
 * timed one would have left the flash on screen.
 */

/**
 * @brief The parser marks a "stop"/"fail" terminator apart from a goto it
 * could not resolve (both were goto_dst -1); negative still means no goto
 * to thread.
 *
 * @patch src/u_decorate.c 1055-1055
 */
    { a->wst[idx].loops = 0; a->wst[idx].goto_dst = -2; break; }
/* @endpatch */

/**
 * @brief The last state of a stopped block goes to S_NULL.
 *
 * @patch src/u_decorate.c 4371-4371
 */
    st->nextstate = last ? (a->wst[wi].loops ? first
                            : a->wst[wi].goto_dst == -2 ? S_NULL : cur)
                         : cur + 1;
/* @endpatch */
