/* test_router.c — T05 router acceptance tests.
 * The whole gesture table of docs/02_UI_UX.md §5, plus plan T05 specifics:
 *   Face swipe -> Stats; Stats up/down toggles tab; Stats right -> Face;
 *   Picker hold cancels; Stats left never navigates.
 */
#include <stdio.h>

#include "bot_router.h"

static int failures = 0;

static void expect(const char *name, bot_effect_t got, bot_effect_t want)
{
    if (got.nav == want.nav && got.picker_delta == want.picker_delta &&
        got.stats_toggle == want.stats_toggle &&
        got.picker_confirm == want.picker_confirm && got.poke == want.poke &&
        got.detail == want.detail && got.edge_bump == want.edge_bump) {
        printf("PASS %s\n", name);
    } else {
        printf("FAIL %s: nav %d->%d delta %d->%d toggle %d->%d confirm %d->%d "
               "poke %d->%d detail %d->%d edge %d->%d\n",
               name, got.nav, want.nav, got.picker_delta, want.picker_delta,
               got.stats_toggle, want.stats_toggle, got.picker_confirm,
               want.picker_confirm, got.poke, want.poke, got.detail, want.detail,
               got.edge_bump, want.edge_bump);
        failures++;
    }
}

#define FX(...) ((bot_effect_t){ __VA_ARGS__ })

int main(void)
{
    /* FACE */
    expect("face tap -> poke+detail",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_TAP),
           FX(.poke = true, .detail = true));
    expect("face swipe_left -> stats",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_SWIPE_LEFT),
           FX(.nav = BOT_NAV_TO_STATS));
    expect("face swipe_right -> stats",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_SWIPE_RIGHT),
           FX(.nav = BOT_NAV_TO_STATS));
    expect("face hold -> picker",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_HOLD),
           FX(.nav = BOT_NAV_TO_PICKER));
    expect("face swipe_up -> none",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_SWIPE_UP), FX(0));
    expect("face swipe_down -> none",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_SWIPE_DOWN), FX(0));
    expect("face wake_only -> none",
           bot_route(BOT_SCR_FACE, BOT_GESTURE_WAKE_ONLY), FX(0));

    /* PICKER */
    expect("picker tap -> confirm",
           bot_route(BOT_SCR_PICKER, BOT_GESTURE_TAP),
           FX(.picker_confirm = true));
    expect("picker swipe_left -> next",
           bot_route(BOT_SCR_PICKER, BOT_GESTURE_SWIPE_LEFT),
           FX(.picker_delta = +1));
    expect("picker swipe_right -> prev",
           bot_route(BOT_SCR_PICKER, BOT_GESTURE_SWIPE_RIGHT),
           FX(.picker_delta = -1));
    expect("picker hold -> cancel",
           bot_route(BOT_SCR_PICKER, BOT_GESTURE_HOLD),
           FX(.nav = BOT_NAV_PICKER_CANCEL));
    expect("picker swipe_down -> cancel",
           bot_route(BOT_SCR_PICKER, BOT_GESTURE_SWIPE_DOWN),
           FX(.nav = BOT_NAV_PICKER_CANCEL));
    expect("picker swipe_up -> none",
           bot_route(BOT_SCR_PICKER, BOT_GESTURE_SWIPE_UP), FX(0));

    /* STATS */
    expect("stats tap -> detail",
           bot_route(BOT_SCR_STATS, BOT_GESTURE_TAP), FX(.detail = true));
    expect("stats swipe_up -> toggle tab",
           bot_route(BOT_SCR_STATS, BOT_GESTURE_SWIPE_UP),
           FX(.stats_toggle = true));
    expect("stats swipe_down -> toggle tab",
           bot_route(BOT_SCR_STATS, BOT_GESTURE_SWIPE_DOWN),
           FX(.stats_toggle = true));
    expect("stats swipe_right -> face",
           bot_route(BOT_SCR_STATS, BOT_GESTURE_SWIPE_RIGHT),
           FX(.nav = BOT_NAV_TO_FACE));
    expect("stats swipe_left -> edge bump, NO nav",
           bot_route(BOT_SCR_STATS, BOT_GESTURE_SWIPE_LEFT),
           FX(.edge_bump = true));
    expect("stats hold -> picker",
           bot_route(BOT_SCR_STATS, BOT_GESTURE_HOLD),
           FX(.nav = BOT_NAV_TO_PICKER));

    if (failures) {
        printf("test_router: %d FAILURE(S)\n", failures);
        return 1;
    }
    printf("test_router: all passed\n");
    return 0;
}
