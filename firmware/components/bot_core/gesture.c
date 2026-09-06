/* gesture.c — single-contact gesture recognizer (T04).
 *
 * Contract authority: design/interaction_tokens.json, validated by
 * acceptance/gesture_cases.json (table-driven in tests/native/test_gesture.c).
 *
 * Rules implemented (see bot_gesture.h for token values):
 *   - DOWN on a wake contact      -> WAKE_ONLY immediately, contact CONSUMED
 *   - movement > tap_slop_px      -> cancels HOLD and TAP for this contact
 *   - TICK at t-down >= hold_ms   -> HOLD (649ms must NOT fire; fired by TICK
 *                                    only, never by UP)
 *   - UP within tap_max_ms and within slop -> TAP (never after HOLD fired)
 *   - UP with dominant axis displacement >= swipe_min_px, axis ratio >= 1.4,
 *     duration within [swipe_min_ms, swipe_max_ms] -> directional SWIPE
 *   - CANCEL ends the contact silently; later samples are ignored
 *   - at most one event per contact
 */
#include "bot_gesture.h"

#include <stdlib.h>

/* interaction_tokens.json */
#define TAP_MAX_MS 250u
#define TAP_SLOP_PX 12
#define HOLD_MS BOT_HOLD_MS /* 650 */
#define SWIPE_MIN_PX BOT_SWIPE_MIN_PX /* 56 */
#define SWIPE_MIN_MS 120u
#define SWIPE_MAX_MS 700u
/* swipe_axis_ratio = 1.4 -> dominant*10 >= other*14 (integer math, no float) */
#define AXIS_RATIO_NUM 14
#define AXIS_RATIO_DEN 10

void bot_gesture_init(bot_gesture_t *g)
{
    g->state = BOT_GS_IDLE;
    g->down_ms = 0;
    g->down_x = 0;
    g->down_y = 0;
    g->moved_beyond_slop = false;
    g->wake_contact = false;
}

void bot_gesture_arm_wake(bot_gesture_t *g)
{
    g->wake_contact = true;
}

static int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static bot_gesture_kind_t classify_swipe(int32_t dx, int32_t dy)
{
    int32_t adx = iabs32(dx);
    int32_t ady = iabs32(dy);
    if (adx >= SWIPE_MIN_PX && adx * AXIS_RATIO_DEN >= ady * AXIS_RATIO_NUM) {
        return dx < 0 ? BOT_GESTURE_SWIPE_LEFT : BOT_GESTURE_SWIPE_RIGHT;
    }
    if (ady >= SWIPE_MIN_PX && ady * AXIS_RATIO_DEN >= adx * AXIS_RATIO_NUM) {
        return dy < 0 ? BOT_GESTURE_SWIPE_UP : BOT_GESTURE_SWIPE_DOWN;
    }
    return BOT_GESTURE_NONE;
}

bot_gesture_kind_t bot_gesture_feed(bot_gesture_t *g, bot_touch_phase_t phase,
                                     uint32_t t_ms, int16_t x, int16_t y)
{
    switch (g->state) {
    case BOT_GS_IDLE:
        if (phase != BOT_TOUCH_DOWN) {
            return BOT_GESTURE_NONE;
        }
        g->down_ms = t_ms;
        g->down_x = x;
        g->down_y = y;
        g->moved_beyond_slop = false;
        if (g->wake_contact) {
            /* first_wake_touch_consumed=true: wake contact never navigates */
            g->wake_contact = false;
            g->state = BOT_GS_CONSUMED;
            return BOT_GESTURE_WAKE_ONLY;
        }
        g->state = BOT_GS_PRESSED;
        return BOT_GESTURE_NONE;

    case BOT_GS_PRESSED: {
        int32_t dx = (int32_t)x - (int32_t)g->down_x;
        int32_t dy = (int32_t)y - (int32_t)g->down_y;
        uint32_t elapsed = t_ms - g->down_ms;

        /* TICK/UP can contain the first displaced sample. Check it before
         * classifying HOLD/TAP, even if a MOVE was coalesced by the driver. */
        if ((phase == BOT_TOUCH_MOVE || phase == BOT_TOUCH_TICK ||
             phase == BOT_TOUCH_UP) &&
            (iabs32(dx) > TAP_SLOP_PX || iabs32(dy) > TAP_SLOP_PX)) {
            g->moved_beyond_slop = true;
        }

        switch (phase) {
        case BOT_TOUCH_MOVE:
            return BOT_GESTURE_NONE;

        case BOT_TOUCH_TICK:
            /* HOLD fires on TICK only; 649ms must not fire, 650ms must. */
            if (!g->moved_beyond_slop && elapsed >= HOLD_MS) {
                g->state = BOT_GS_HOLD_FIRED;
                return BOT_GESTURE_HOLD;
            }
            return BOT_GESTURE_NONE;

        case BOT_TOUCH_UP:
            g->state = BOT_GS_CONSUMED;
            if (!g->moved_beyond_slop && elapsed <= TAP_MAX_MS) {
                return BOT_GESTURE_TAP;
            }
            if (elapsed >= SWIPE_MIN_MS && elapsed <= SWIPE_MAX_MS) {
                return classify_swipe(dx, dy);
            }
            return BOT_GESTURE_NONE;

        case BOT_TOUCH_CANCEL:
            g->state = BOT_GS_CONSUMED;
            return BOT_GESTURE_NONE;

        case BOT_TOUCH_DOWN:
        default:
            return BOT_GESTURE_NONE; /* duplicate DOWN mid-contact: ignore */
        }
    }

    case BOT_GS_HOLD_FIRED:
        /* After HOLD, UP must not produce TAP (or anything else). */
        if (phase == BOT_TOUCH_UP || phase == BOT_TOUCH_CANCEL) {
            g->state = BOT_GS_CONSUMED;
        }
        return BOT_GESTURE_NONE;

    case BOT_GS_CONSUMED:
    default:
        /* Contact finished (or wake-consumed): swallow everything until the
         * recognizer is re-armed by a fresh IDLE->DOWN. A DOWN here starts a
         * new contact so multi-touch sequencing still works. */
        if (phase == BOT_TOUCH_DOWN) {
            g->state = BOT_GS_IDLE;
            return bot_gesture_feed(g, phase, t_ms, x, y);
        }
        return BOT_GESTURE_NONE;
    }
}

const char *bot_gesture_event_name(bot_gesture_kind_t ev)
{
    switch (ev) {
    case BOT_GESTURE_TAP: return "tap";
    case BOT_GESTURE_HOLD: return "hold";
    case BOT_GESTURE_SWIPE_LEFT: return "swipe_left";
    case BOT_GESTURE_SWIPE_RIGHT: return "swipe_right";
    case BOT_GESTURE_SWIPE_UP: return "swipe_up";
    case BOT_GESTURE_SWIPE_DOWN: return "swipe_down";
    case BOT_GESTURE_WAKE_ONLY: return "wake_only";
    case BOT_GESTURE_NONE:
    default: return "none";
    }
}
