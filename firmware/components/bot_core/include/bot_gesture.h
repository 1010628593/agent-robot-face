/* Allocation-free touch arbitration. Complete frames are the production API.
 * DOWN fixes ownership until all fingers leave. Long holds never navigate.
 * Single-sample API remains available for existing native callers. */
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

/* One complete hardware sample; cancelled never means a normal release. */
typedef struct { uint8_t id; int16_t x,y; } bot_touch_point_t;
typedef struct {
    uint32_t time_ms;
    uint8_t count;
    bool cancelled;
    bool final_position; /* points[0] carries a trustworthy last UP coordinate */
    bot_touch_point_t points[2];
} bot_touch_frame_t;

typedef struct {
    bot_gesture_state_t state;
    uint32_t down_ms, stroke_ms;
    bool stroking;
    int16_t  down_x;
    int16_t  down_y;
    bool     moved_beyond_slop; /* > tap_slop_px permanently cancels tap */
    int16_t last_x, last_y, extreme;
    float path_px;
    uint8_t reversals;
    int8_t direction, stroke_axis;
    bool pet_contact, face_mode;
    uint8_t edge, contact_count, primary_id;
    bool multi_contact, frame_active, blocked;
    bot_touch_frame_t frame;
    bool     wake_contact;      /* first touch that wakes the device */
} bot_gesture_t;

/* Reset recognizer to IDLE (e.g. on screen power-cycle). */
void bot_gesture_init(bot_gesture_t *g);

/* Mark the next DOWN as a wake contact (first_wake_touch_consumed=true). */
/* Set only before DOWN; central ownership is latched until release. */
void bot_gesture_set_face_mode(bot_gesture_t *g,bool enabled);
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

/* Whole-contact arbitration, including two -> one and cancellation. */
bot_gesture_kind_t bot_gesture_feed_frame(bot_gesture_t *g,const bot_touch_frame_t *frame);

const char *bot_gesture_event_name(bot_gesture_kind_t ev);

#ifdef __cplusplus
}
#endif

#endif /* BOT_GESTURE_H */
