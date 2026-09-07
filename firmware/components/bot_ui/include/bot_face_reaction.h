#ifndef BOT_FACE_REACTION_H
#define BOT_FACE_REACTION_H
#include "bot_face_motion.h"
#include "bot_motion.h"
bool bot_face_reaction_allowed(bot_state_t state);
/* Changes only the supplied render pose. Never changes Agent state/clocks. */
void bot_face_reaction_apply(const bot_motion_view_t *view,bot_state_t state,
                             uint32_t now,bot_face_pose_t *pose);
#endif
