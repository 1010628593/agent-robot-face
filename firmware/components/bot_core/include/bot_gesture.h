/* bot_gesture.h — independent single-contact gesture recognizer (T04).
 *
 * Contract authority: design/interaction_tokens.json + acceptance/gesture_cases.json.
 * One contact produces AT MOST one GestureEvent. No dynamic allocation, no LVGL
 * dependency — host-compilable for tests/native, linkable into ESP-IDF firmware.
 *
 * Numeric tokens (never scatter magic numbers):
 *   tap_max_ms=250  tap_slop_px=12  hold_ms=650
 *   swipe_min_px=56 swipe_min_ms=120 swipe_max_ms=700 swipe_axis_ratio=1.4
 *   first_wake_touch_consumed=true -> a wake contact emits WAKE_ONLY and nothing else
 */
#ifndef BOT_GESTURE_H
#define BOT_GESTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "bot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOT_GS_IDLE = 0,
    BOT_GS_PRESSED,
    BOT_GS_HOLD_FIRED,
    BOT_GS_CONSUMED
} bot_gesture_state_t;

typedef struct {
    bot_gesture_state_t state;
    uint32_t down_ms;
    int16_t  down_x;
    int16_t  down_y;
    bool     moved_beyond_slop; /* > tap_slop_px cancels hold and tap */
    bool     wake_contact;      /* first touch that wakes the device */
} bot_gesture_t;

/* Reset recognizer to IDLE (e.g. on screen power-cycle). */
void bot_gesture_init(bot_gesture_t *g);

/* Mark the next DOWN as a wake contact (first_wake_touch_consumed=true). */
void bot_gesture_arm_wake(bot_gesture_t *g);

/* Feed one sample. Returns the event produced by THIS sample (usually NONE;
 * at most one event per contact). phase strings mirror gesture_cases.json:
 * "down" / "move" / "tick" / "up" / "cancel". */
typedef enum {
    BOT_TOUCH_DOWN = 0,
    BOT_TOUCH_MOVE,
    BOT_TOUCH_TICK, /* periodic timer tick while contact is held */
    BOT_TOUCH_UP,
    BOT_TOUCH_CANCEL
} bot_touch_phase_t;

bot_gesture_kind_t bot_gesture_feed(bot_gesture_t *g, bot_touch_phase_t phase,
                                     uint32_t t_ms, int16_t x, int16_t y);

const char *bot_gesture_event_name(bot_gesture_kind_t ev);

#ifdef __cplusplus
}
#endif

#endif /* BOT_GESTURE_H */
