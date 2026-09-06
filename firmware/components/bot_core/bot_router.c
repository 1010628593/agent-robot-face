/* bot_router.c — the single gesture table, docs/02_UI_UX.md §5. */
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
            e.detail = true; /* gaze shift + 2s detail overlay */
            break;
        case BOT_GESTURE_SWIPE_LEFT:
        case BOT_GESTURE_SWIPE_RIGHT:
            e.nav = BOT_NAV_TO_STATS; /* last subpage; first time usage */
            break;
        case BOT_GESTURE_HOLD:
            e.nav = BOT_NAV_TO_PICKER;
            break;
        default:
            break; /* up/down: no navigation on face */
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
        case BOT_GESTURE_HOLD:
        case BOT_GESTURE_SWIPE_DOWN:
            e.nav = BOT_NAV_PICKER_CANCEL; /* cancel, back to entry screen */
            break;
        default:
            break; /* up: no action */
        }
        break;

    case BOT_SCR_STATS:
        switch (gesture) {
        case BOT_GESTURE_TAP:
            e.detail = true; /* metric detail overlay */
            break;
        case BOT_GESTURE_SWIPE_UP:
        case BOT_GESTURE_SWIPE_DOWN:
            e.stats_toggle = true; /* usage <-> quota */
            break;
        case BOT_GESTURE_SWIPE_RIGHT:
            e.nav = BOT_NAV_TO_FACE;
            break;
        case BOT_GESTURE_SWIPE_LEFT:
            e.edge_bump = true; /* boundary feedback only, NO navigation */
            break;
        case BOT_GESTURE_HOLD:
            e.nav = BOT_NAV_TO_PICKER;
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
