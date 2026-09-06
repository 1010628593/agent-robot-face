/* test_bot_types.c — T03 minimal native compile check.
 * Verifies the shared header is self-contained, C99-clean, and that the
 * constant bounds match the contract tokens. Not a gesture test (that's T04).
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "bot_types.h"

int main(void) {
    /* compile-time layout sanity */
    bot_touch_sample_t s = {.x = 100, .y = 200, .t_ms = 42, .pressed = true};
    assert(s.x == 100 && s.y == 200 && s.t_ms == 42 && s.pressed);

    bot_gesture_event_t g = {0};
    g.kind = BOT_GESTURE_TAP;
    assert(g.kind == BOT_GESTURE_TAP);

    /* contract tokens */
    assert(BOT_DISPLAY_W == 466 && BOT_DISPLAY_H == 466);
    assert(BOT_FRAME_MAX_BYTES == 8192);
    assert(BOT_HOLD_MS == 650);
    assert(BOT_SWIPE_MIN_PX == 56);

    /* enum coverage: exactly four agents, in catalog order */
    assert(BOT_AGENT_COUNT == 4);
    assert(BOT_AGENT_CODEX == 0 && BOT_AGENT_WORKBUDDY == 1);
    assert(BOT_AGENT_CURSOR == 2 && BOT_AGENT_HERMES == 3);

    printf("test_bot_types OK\n");
    return 0;
}
