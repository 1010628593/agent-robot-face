#ifndef BOT_FACE_REACTION_H
#define BOT_FACE_REACTION_H
#include "bot_face_motion.h"
#include "bot_motion.h"
typedef struct {
    float position[4], velocity[4]; /* face XY then gaze XY */
    float spiral_gain, amplitude, intensity, fatigue, startle, previous_energy, phase;
    uint32_t last_ms, quiet_ms, grade_ms;
    unsigned grade, pending_grade;
    bool initialized, recovering;
} bot_face_inertia_t;
void bot_face_inertia_apply(bot_face_inertia_t *motion,const bot_motion_view_t *view,
                            float rotation,bool enabled,uint32_t now,bot_face_pose_t *pose);
bool bot_face_reaction_allowed(bot_state_t state);
/* Changes only the supplied render pose. Never changes Agent state/clocks. */
void bot_face_reaction_apply(const bot_motion_view_t *view,bot_state_t state,
                             uint32_t now,bot_face_pose_t *pose);
#endif
