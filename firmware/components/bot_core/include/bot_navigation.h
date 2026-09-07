#ifndef BOT_NAVIGATION_H
#define BOT_NAVIGATION_H
#include "bot_gesture.h"
#include "bot_router.h"
#include "bot_os_motion.h"
typedef enum {BOT_NAV_FACE, BOT_NAV_DRAGGING, BOT_NAV_SETTLING, BOT_NAV_PANEL} bot_nav_phase_t;
typedef struct {
    bot_nav_phase_t phase;
    bot_screen_t card;
    float position,velocity,target,spring_x,spring_v;
    uint32_t spring_ms,tick_ms;
    bool stable_open,session,owned,candidate,blocked,opening;
    uint8_t track_id;
    int16_t down_x,down_y;
    float origin_y,origin_position;
    struct {uint32_t ms;float y;} samples[16];
    uint8_t samples_count;
} bot_navigation_t;
void bot_navigation_init(bot_navigation_t *n);
void bot_navigation_tick(bot_navigation_t *n,uint32_t now);
/* True consumes this whole input session, including its final UP. */
bool bot_navigation_feed(bot_navigation_t *n,const bot_touch_frame_t *f);
void bot_navigation_dismiss(bot_navigation_t *n,uint32_t now);
/* Explicit synchronous setup for initial state / offline inspectors. */
void bot_navigation_show(bot_navigation_t *n,bot_screen_t page);
float bot_navigation_y(const bot_navigation_t *n);
float bot_navigation_progress(const bot_navigation_t *n);
bool bot_navigation_content_enabled(const bot_navigation_t *n);
#endif
