#ifndef BOT_LINK_H
#define BOT_LINK_H
#include "bot_model.h"
#include "bot_router.h"
/* All model/transport calls except RX task execute in the single UI owner. */
void bot_link_start(void);
/* Bounded UI-owner diagnostic line; separate from the v2 message envelope. */
void bot_link_trace(const char *line);
bool bot_link_poll(bot_model_t *model,uint32_t now);
bool bot_link_select(bot_model_t *model,bot_selection_mode_t mode,bot_agent_id_t agent,uint32_t now);
bool bot_link_usage_request(bot_model_t *model,uint8_t subject,uint8_t period,uint8_t page,uint32_t now);
bool bot_link_version_mismatch(void);
void bot_link_health_trace(void);
/* UI timer only; bounded optional render metadata, never model facts. */
void bot_link_diagnostics(uint32_t updates,uint32_t window_ms,uint32_t avg_us,uint32_t max_us,
                          const bot_model_t *model,bot_state_t displayed_state,bot_screen_t screen,uint8_t usage_level);
#endif
