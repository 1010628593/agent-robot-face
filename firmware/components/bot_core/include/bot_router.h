/* bot_router.h — gesture -> navigation/UI effect router (T05).
 *
 * Contract authority: docs/02_UI_UX.md §5 (唯一手势表). Pure function of
 * (current screen, gesture): no hidden state, so navigation is testable on
 * host. Selection writes never happen here — picker confirm produces an
 * effect that the app layer turns into an action message.
 */
#ifndef BOT_ROUTER_H
#define BOT_ROUTER_H

#include <stdbool.h>

#include "bot_gesture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOT_SCR_FACE = 0,
    BOT_SCR_PICKER,
    BOT_SCR_STATS
} bot_screen_t;

typedef enum {
    BOT_NAV_NONE = 0,
    BOT_NAV_TO_STATS,      /* enter stats at last-used subpage (first: usage) */
    BOT_NAV_TO_FACE,
    BOT_NAV_TO_PICKER,
    BOT_NAV_PICKER_CANCEL  /* cancel picker, return to the entry screen */
} bot_nav_t;

typedef struct {
    bot_nav_t nav;
    int picker_delta;    /* -1 prev / +1 next agent in picker carousel */
    bool stats_toggle;   /* switch usage <-> quota subpage */
    bool picker_confirm; /* tap on picker center: confirm preview agent */
    bool poke;           /* face poke: gaze shift + 2s detail overlay */
    bool detail;         /* stats tap: show metric detail overlay */
    bool edge_bump;      /* stats swipe_left: boundary feedback only, no nav */
} bot_effect_t;

/* The single gesture table (02_UI_UX §5):
 *             FACE                PICKER              STATS
 *   tap       poke+detail         confirm             detail
 *   left      ->STATS             next agent          edge bump (NO nav)
 *   right     ->STATS             prev agent          ->FACE
 *   up/down   none                down:cancel up:none toggle usage/quota
 *   hold      ->PICKER            cancel+return       ->PICKER
 */
bot_effect_t bot_route(bot_screen_t screen, bot_gesture_kind_t gesture);

#ifdef __cplusplus
}
#endif

#endif /* BOT_ROUTER_H */
