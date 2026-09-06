/* Pure-C, allocation-free face motion. Times share one uint32_t ms clock.
 * Presentation only: never changes an Agent's protocol state or permissions. */
#ifndef BOT_FACE_MOTION_H
#define BOT_FACE_MOTION_H
#include "bot_types.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BOT_FACE_TRANSITION_MS 220u
#define BOT_FACE_FRAME_MS 33u

typedef struct {
    float cx, cy, eye_w, left_h, right_h, separation;
    float lid, tilt, smile, cross;
    float pupil, gaze_x, gaze_y, opacity;
} bot_face_pose_t;

typedef struct {
    bot_face_t expression;
    uint32_t event_key, entered_ms, origin_ms, seed;
    bot_face_pose_t from;
    bool transitioning, touch_active;
    float touch_from_x, touch_from_y, touch_from_gain;
    float touch_x, touch_y, touch_gain, hold;
    uint32_t touch_ms;
} bot_face_motion_t;

bot_face_t bot_face_for_state(bot_state_t state);
void bot_face_motion_init(bot_face_motion_t *m, uint32_t seed, uint32_t now_ms);
/* Returns false for an identical (expression,event_key): keep phase on heartbeat.
 * A caller can change event_key for a new run even if its state is still DONE. */
bool bot_face_motion_set(bot_face_motion_t *m, bot_face_t expression,
                         uint32_t event_key, uint32_t now_ms);
void bot_face_motion_sample(const bot_face_motion_t *m, uint32_t now_ms,
                            bot_face_pose_t *out);
/* Coordinates are display coordinates. hold is 0..1; no ring is drawn.
 * Released contacts ease out. Caller owns gesture arbitration/navigation. */
void bot_face_motion_touch(bot_face_motion_t *m, bool pressed, int16_t x,
                          int16_t y, float hold, uint32_t now_ms);
#ifdef __cplusplus
}
#endif
#endif
