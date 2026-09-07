#include "bot_gesture.h"
#include <math.h>

#include <stdlib.h>
#include <string.h>

/* Production touch tokens; see docs/touch-interactions.md. */
#define TAP_MAX_MS 250u
#define TAP_SLOP_PX 12

#define SWIPE_MIN_PX BOT_SWIPE_MIN_PX /* 56 */

/* swipe_axis_ratio = 1.4 -> dominant*10 >= other*14 (integer math, no float) */
#define AXIS_RATIO_NUM 14
#define AXIS_RATIO_DEN 10

void bot_gesture_init(bot_gesture_t *g)
{
    memset(g,0,sizeof(*g));
    g->state = BOT_GS_IDLE;
    g->down_ms = 0;
    g->down_x = 0;
    g->down_y = 0;
    g->moved_beyond_slop = false;
    g->wake_contact = false;
}

void bot_gesture_set_face_mode(bot_gesture_t *g,bool enabled) { g->face_mode=enabled; }

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
        g->down_ms = t_ms;g->stroking=false;g->stroke_ms=0;
        g->down_x = x;
        g->down_y = y;
        g->last_x=x;g->last_y=y;g->path_px=0;g->reversals=0;g->direction=0;g->stroke_axis=0;
        int32_t rx=x-233,ry=y-233;
        bool valid=x>=0 && x<466 && y>=0 && y<466 && rx*rx+ry*ry<=233*233;
        g->edge=g->face_mode?(y<=56?1:y>=410?2:(x<=56 || x>=410)?3:0):0;
        g->pet_contact=g->face_mode && valid && !g->edge;
        if(!valid) {g->state=BOT_GS_CONSUMED;return BOT_GESTURE_NONE;}
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
         * classifying TAP, even if a MOVE was coalesced by the driver. */
        if ((phase == BOT_TOUCH_MOVE || phase == BOT_TOUCH_TICK ||
             phase == BOT_TOUCH_UP) &&
            (iabs32(dx) > TAP_SLOP_PX || iabs32(dy) > TAP_SLOP_PX)) {
            g->moved_beyond_slop = true;
        }

        if(g->pet_contact && (phase==BOT_TOUCH_MOVE || phase==BOT_TOUCH_UP)) {
            int32_t sx=x-g->last_x,sy=y-g->last_y;
            g->path_px+=sqrtf((float)sx*sx+(float)sy*sy);
            g->last_x=x;g->last_y=y;
            if(!g->stroke_axis && (iabs32(dx)>=8 || iabs32(dy)>=8)) {
                g->stroke_axis=iabs32(dx)>=iabs32(dy)?1:2;
                int32_t d=g->stroke_axis==1?dx:dy;g->direction=d>0?1:-1;
                g->extreme=g->stroke_axis==1?x:y;
            } else if(g->stroke_axis) {
                int16_t v=g->stroke_axis==1?x:y;
                int32_t d=v-g->extreme;
                if(d*g->direction>0)g->extreme=v;
                else if(d*g->direction<=-8) { if(g->reversals<255)g->reversals++;g->direction=-g->direction;g->extreme=v; }
            }
        }
        if(g->pet_contact && !g->stroking && g->path_px>=40 && g->reversals>=2) {
            g->stroking=true;g->stroke_ms=t_ms;
        }
        switch (phase) {
        case BOT_TOUCH_MOVE:
            return BOT_GESTURE_NONE;

        case BOT_TOUCH_TICK:
            /* Stationary contact is interaction, never navigation. */
            return BOT_GESTURE_NONE;

        case BOT_TOUCH_UP:
            g->state = BOT_GS_CONSUMED;
            if(g->pet_contact && g->path_px>=40 && g->reversals>=2)return BOT_GESTURE_STROKE;
            if (!g->moved_beyond_slop && elapsed <= TAP_MAX_MS && !g->edge)
                return BOT_GESTURE_TAP;
            if(g->pet_contact)return BOT_GESTURE_NONE;
            bot_gesture_kind_t ev=classify_swipe(dx,dy);
            if(g->face_mode) {
                if(g->edge==1 && ev==BOT_GESTURE_SWIPE_DOWN)return ev;
                if(g->edge==2 && ev==BOT_GESTURE_SWIPE_UP)return ev;
                return BOT_GESTURE_NONE;
            }
            return ev;

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
    case BOT_GESTURE_STROKE:return "stroke";
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

/* Errors consume the entire session until a reliable all-up sample. Track IDs
 * protect single-finger navigation from silent hardware primary replacement. */
bot_gesture_kind_t bot_gesture_feed_frame(bot_gesture_t *g,const bot_touch_frame_t *f)
{
    if(f->cancelled || f->count>2) {
        g->blocked=true;g->pet_contact=false;g->state=BOT_GS_CONSUMED;
        g->frame=*f;g->frame.count=0;return BOT_GESTURE_NONE;
    }
    if(g->blocked) {
        if(!f->count){g->blocked=false;g->frame_active=false;g->contact_count=0;}
        g->frame=*f;return BOT_GESTURE_NONE;
    }
    bot_gesture_kind_t ev=BOT_GESTURE_NONE;
    if(f->count && !g->frame_active) {
        g->frame_active=true;g->multi_contact=false;g->primary_id=f->points[0].id;
        ev=bot_gesture_feed(g,BOT_TOUCH_DOWN,f->time_ms,f->points[0].x,f->points[0].y);
    }
    if(f->count>1 || (f->count==1 && g->contact_count && f->points[0].id!=g->primary_id))
        g->multi_contact=true;
    if(g->frame_active && f->count) {
        bot_gesture_feed(g,BOT_TOUCH_MOVE,f->time_ms,f->points[0].x,f->points[0].y);
    } else if(g->frame_active) {
        if(!g->multi_contact) {
            if(f->final_position && f->points[0].id==g->primary_id) {
                ev=bot_gesture_feed(g,BOT_TOUCH_UP,f->time_ms,f->points[0].x,f->points[0].y);
                g->last_x=f->points[0].x;g->last_y=f->points[0].y;
            } else ev=bot_gesture_feed(g,BOT_TOUCH_UP,f->time_ms,g->last_x,g->last_y);
        }
        else g->state=BOT_GS_CONSUMED;
        g->frame_active=false;
    }
    /* Keep the last single coordinate even for edge/panel contacts. */
    if(f->count){g->last_x=f->points[0].x;g->last_y=f->points[0].y;}
    g->contact_count=f->count;g->frame=*f;
    return ev;
}
