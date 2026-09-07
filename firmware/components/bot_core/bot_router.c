/* Navigation events have already passed ownership arbitration.
 * Current contract: docs/touch-interactions.md. No long-press routes. */
#include "bot_router.h"

bot_effect_t bot_route(bot_screen_t screen, bot_gesture_kind_t gesture)
{
    bot_effect_t e = {
        .nav = BOT_NAV_NONE,
        .picker_delta = 0,
        .stats_toggle = false,
        .picker_confirm = false,
        .poke = false,
        .detail = false,
        .edge_bump = false,
    };

    switch (screen) {
    case BOT_SCR_FACE:
        switch (gesture) {
        case BOT_GESTURE_TAP:
            e.poke = true;
            /* Face taps are consumed by the interaction layer. */
            break;
        default:
            break;
        }
        break;

    case BOT_SCR_PICKER:
        switch (gesture) {
        case BOT_GESTURE_TAP:
            e.picker_confirm = true;
            break;
        case BOT_GESTURE_SWIPE_LEFT:
            e.picker_delta = +1; /* next agent (wraps) */
            break;
        case BOT_GESTURE_SWIPE_RIGHT:
            e.picker_delta = -1; /* previous agent (wraps) */
            break;
        default:
            break;
        }
        break;

    case BOT_SCR_STATS:
        switch (gesture) {
        case BOT_GESTURE_TAP:
            e.detail = true; /* metric detail overlay */
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
    return e;
}
