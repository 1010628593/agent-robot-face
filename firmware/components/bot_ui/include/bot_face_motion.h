/* Pure-C, allocation-free face motion. Times share one uint32_t ms clock.
 * Presentation only: never changes an Agent's protocol state or permissions. */
#ifndef BOT_FACE_MOTION_H
#define BOT_FACE_MOTION_H
#include "bot_types.h"
#include "bot_gesture.h"
#ifdef __cplusplus
extern "C" {
#endif
#define BOT_FACE_TRANSITION_MS 220u
#define BOT_FACE_FRAME_MS 20u

typedef struct {
    float cx, cy, eye_w, left_h, right_h, separation;
    float lid, tilt, smile, cross;
    float pupil, gaze_x, gaze_y, opacity, convergence;
    float spiral, spiral_phase; /* high-intensity dizziness: 0..1 and radians */
    float roll; /* head inclination: eye centers rotate around the face center */
} bot_face_pose_t;

typedef struct {
    bot_face_t expression;
    uint32_t event_key, entered_ms, origin_ms, seed;
    bot_face_pose_t from;
    bool transitioning, touch_active;
    float touch_from_x, touch_from_y, touch_from_gain;
    float touch_x, touch_y, touch_gain, hold;
    uint32_t touch_ms;
    uint32_t pet_ms, pet_duration, pet_until;
    uint8_t pet_action, tap_bag[4], stroke_bag[3], tap_left, stroke_left, last_tap, last_stroke;
    bool pet_started;
    float interaction[5], interaction_from[5]; /* L/R eyelid, pinch, comfort, tilt */
    uint32_t interaction_ms, interaction_duration, tap_ms, last_tap_ms;
    float dual_distance;
    uint8_t touch_count, tap_region, tap_streak;
    bool tapped;
    int16_t tap_x,tap_y;
    /* Touch-driven body follows behind the eyes; analytic damped motion is
     * independent of input/report frequency and is sampled on the face clock. */
    float body[2], body_velocity[2], body_target[2];
    uint32_t body_ms, contact_ms, released_ms;
    float travel_speed, last_contact_x, last_contact_y;
    uint32_t contact_sample_ms;
    uint8_t alternating;
    float tap_closed_from[2];
    float contact_velocity[2]; /* filtered signed px/s, drives stroke sway */
    bool contact_was_dual;
    uint16_t release_hold_ms;
    uint32_t attention_ms; /* first-contact curiosity continues through release */

} bot_face_motion_t;

void bot_face_motion_interact(bot_face_motion_t *m,const bot_touch_frame_t *f,
                              float comfort,uint32_t held_ms);
void bot_face_motion_poke(bot_face_motion_t *m,int16_t x,int16_t y,uint32_t now);
void bot_face_motion_clear_touch(bot_face_motion_t *m,uint32_t now);
bool bot_face_motion_pet(bot_face_motion_t *m,bool stroke,uint32_t now_ms);
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
