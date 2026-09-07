/* Sparse/coalesced touch input must not rely on the caller synthesising MOVE.
 * These regressions exercise production bot_gesture_feed, not a mock. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_gesture.h"
#include <assert.h>
#include <stdio.h>

static void begin(bot_gesture_t *g, uint32_t t, int16_t x, int16_t y)
{
    bot_gesture_init(g);
    assert(bot_gesture_feed(g, BOT_TOUCH_DOWN, t, x, y) == BOT_GESTURE_NONE);
}

static void tick_displacement_cancels_hold(void)
{
    bot_gesture_t g;
    begin(&g, 0, 233, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 650, 246, 233) == BOT_GESTURE_NONE);
    assert(g.moved_beyond_slop);
    /* Returning to the starting point cannot re-arm this contact. */
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 690, 233, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 700, 233, 233) == BOT_GESTURE_NONE);
}
static void up_displacement_is_not_a_tap(void)
{
    bot_gesture_t g;
    begin(&g, 0, 233, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 100, 253, 233) == BOT_GESTURE_NONE);
}
static void final_sample_can_be_a_swipe(void)
{
    bot_gesture_t g;
    begin(&g, 0, 300, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 200, 210, 233) == BOT_GESTURE_SWIPE_LEFT);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 220, 210, 233) == BOT_GESTURE_NONE);
}
static void cancelled_hold_can_finish_as_swipe(void)
{
    bot_gesture_t g;
    begin(&g, 0, 300, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 650, 287, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 700, 210, 233) == BOT_GESTURE_SWIPE_LEFT);
}
static void exact_slop_preserves_stationary_hold(void)
{
    bot_gesture_t g;
    begin(&g, 0, 233, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 649, 245, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 650, 245, 233) == BOT_GESTURE_HOLD);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 680, 245, 233) == BOT_GESTURE_NONE);
}
static void vertical_displacement_cancels_hold(void)
{
    bot_gesture_t g;
    begin(&g, 0, 233, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 650, 233, 220) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 690, 233, 220) == BOT_GESTURE_NONE);
}
static void hold_survives_clock_wrap(void)
{
    bot_gesture_t g;
    uint32_t t = UINT32_MAX - 200;
    begin(&g, t, 233, 233);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, t + 649, 233, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, t + 650, 233, 233) == BOT_GESTURE_HOLD);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, t + 700, 233, 233) == BOT_GESTURE_NONE);
}
static void wake_contact_remains_consumed(void)
{
    bot_gesture_t g;
    bot_gesture_init(&g);
    bot_gesture_arm_wake(&g);
    assert(bot_gesture_feed(&g, BOT_TOUCH_DOWN, 0, 233, 233) == BOT_GESTURE_WAKE_ONLY);
    assert(bot_gesture_feed(&g, BOT_TOUCH_TICK, 650, 310, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 690, 330, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_DOWN, 800, 233, 233) == BOT_GESTURE_NONE);
    assert(bot_gesture_feed(&g, BOT_TOUCH_UP, 900, 233, 233) == BOT_GESTURE_TAP);
}
int main(void)
{
#define RUN(test) do { test(); puts("PASS " #test); } while (0)
    RUN(tick_displacement_cancels_hold);
    RUN(up_displacement_is_not_a_tap);
    RUN(final_sample_can_be_a_swipe);
    RUN(cancelled_hold_can_finish_as_swipe);
    RUN(exact_slop_preserves_stationary_hold);
    RUN(vertical_displacement_cancels_hold);
    RUN(hold_survives_clock_wrap);
    RUN(wake_contact_remains_consumed);
#undef RUN
    puts("gesture_coalesced: 8 cases passed");
    return 0;
}
