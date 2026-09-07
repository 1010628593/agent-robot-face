/* Pure-C spatial navigation. No LVGL timer, allocation, hardware or model I/O. */
#include "bot_navigation.h"
#include <math.h>
#include <string.h>
static float clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static float sign(const bot_navigation_t *n){return n->card==BOT_SCR_PICKER?1.f:-1.f;}
static float visual(float p) {
    if(p<0)return -BOT_OS_RUBBER_LIMIT*(-p)/(BOT_OS_RUBBER_SCALE-p);
    if(p>BOT_OS_TRAVEL){float d=p-BOT_OS_TRAVEL;return BOT_OS_TRAVEL+BOT_OS_RUBBER_LIMIT*d/(BOT_OS_RUBBER_SCALE+d);}
    return p;
}
static void settle(bot_navigation_t *n,bool open,float speed,uint32_t now) {
    if((int32_t)(now-n->tick_ms)<0)now=n->tick_ms;
    float p=n->position;
    if(n->phase==BOT_NAV_DRAGGING) {
        float excess=fmaxf(-p,p-BOT_OS_TRAVEL);
        if(excess>0)speed*=(BOT_OS_RUBBER_LIMIT*BOT_OS_RUBBER_SCALE)/((BOT_OS_RUBBER_SCALE+excess)*(BOT_OS_RUBBER_SCALE+excess));
        p=visual(p);
    }
    n->position=p;n->velocity=speed;n->target=open?BOT_OS_TRAVEL:0;
    n->spring_x=p-n->target;n->spring_v=speed;n->spring_ms=now;
    n->phase=BOT_NAV_SETTLING;n->candidate=false;
}
void bot_navigation_init(bot_navigation_t *n){memset(n,0,sizeof(*n));n->card=BOT_SCR_FACE;}
void bot_navigation_tick(bot_navigation_t *n,uint32_t now) {
    if((int32_t)(now-n->tick_ms)<0)return;
    n->tick_ms=now;
    if(n->phase!=BOT_NAV_SETTLING)return;
    /* Release debounce may deliver an older timestamp than the UI clock. */
    if((int32_t)(now-n->spring_ms)<0)return;
    uint32_t age=now-n->spring_ms;
    float t=age/1000.f,w=BOT_OS_SPRING_OMEGA;
    float b=n->spring_v+w*n->spring_x,e=expf(-w*t);
    n->position=n->target+(n->spring_x+b*t)*e;
    n->velocity=(n->spring_v-w*b*t)*e;
    if(age>=BOT_OS_SETTLE_MAX || (fabsf(n->position-n->target)<BOT_OS_POSITION_EPSILON && fabsf(n->velocity)<BOT_OS_VELOCITY_EPSILON)) {
        n->position=n->target;n->velocity=0;n->stable_open=n->target>0;
        n->phase=n->stable_open?BOT_NAV_PANEL:BOT_NAV_FACE;
    }
}
static void sample(bot_navigation_t *n,uint32_t ms,float y) {
    /* Keep the window plus its immediately preceding sample. An unchanged
     * point is still a timing sample, so pausing kills flick velocity. */
    while(n->samples_count>1 && ms-n->samples[1].ms>BOT_OS_SPEED_WINDOW) {
        memmove(n->samples,n->samples+1,(--n->samples_count)*sizeof(n->samples[0]));
    }
    if(n->samples_count==16)memmove(n->samples,n->samples+1,(--n->samples_count)*sizeof(n->samples[0]));
    if(n->samples_count && n->samples[n->samples_count-1].ms==ms) {
        n->samples[n->samples_count-1].y=y;return;
    }
    n->samples[n->samples_count].ms=ms;n->samples[n->samples_count++].y=y;
}
static float speed(const bot_navigation_t *n,uint32_t now) {
    if(n->samples_count<2)return 0;
    unsigned end=n->samples_count-1,start=end;
    if(now-n->samples[end].ms>BOT_OS_SPEED_WINDOW)return 0;
    while(start && now-n->samples[start-1].ms<=BOT_OS_SPEED_WINDOW)start--;
    uint32_t dt=n->samples[end].ms-n->samples[start].ms;
    return dt?1000.f*(n->samples[end].y-n->samples[start].y)/dt:0;
}
static bool valid(const bot_touch_point_t *p) {
    int32_t x=p->x-233,y=p->y-233;
    return p->x>=0 && p->x<466 && p->y>=0 && p->y<466 && x*x+y*y<=233*233;
}
bool bot_navigation_feed(bot_navigation_t *n,const bot_touch_frame_t *f) {
    bot_navigation_tick(n,f->time_ms);
    if(f->cancelled || f->count>2) {
        bool owned=n->owned || n->phase==BOT_NAV_DRAGGING || n->phase==BOT_NAV_SETTLING;
        if(owned)settle(n,n->stable_open,0,f->time_ms);
        n->blocked=true;n->owned=owned;n->session=true;n->candidate=false;
        return owned;
    }
    if(n->blocked) {
        bool owned=n->owned;
        if(!f->count){n->blocked=false;n->session=false;n->owned=false;}
        return owned;
    }
    if(f->count && !n->session) {
        n->session=true;n->owned=false;n->candidate=false;n->samples_count=0;
        const bot_touch_point_t *p=&f->points[0];
        n->track_id=p->id;n->down_x=p->x;n->down_y=p->y;
        bool top=p->y<=BOT_OS_EDGE_TOP,bottom=p->y>=BOT_OS_EDGE_BOTTOM;
        if(valid(p) && (top || bottom)) {
            if(n->phase==BOT_NAV_FACE) {
                n->card=top?BOT_SCR_PICKER:BOT_SCR_STATS;n->opening=true;n->candidate=true;
            } else if(n->phase==BOT_NAV_PANEL) {
                n->opening=false;
                n->candidate=n->card==BOT_SCR_PICKER?bottom:top;
            } else if(n->phase==BOT_NAV_SETTLING) {
                /* Both corresponding physical edges can catch the same card;
                 * another card cannot open until Face has become stable. */
                n->candidate=true;n->opening=n->card==BOT_SCR_PICKER?top:bottom;
            }
        }
        n->owned=n->candidate || n->phase==BOT_NAV_SETTLING;
        sample(n,f->time_ms,p->y);
    }
    if(f->count>1 || (f->count==1 && f->points[0].id!=n->track_id)) {
        if(n->owned)settle(n,n->stable_open,0,f->time_ms);
        n->blocked=true;n->candidate=false;
        return n->owned;
    }
    if((f->count || f->final_position) && n->candidate) {
        float y=f->points[0].y,dy=y-n->down_y,dx=f->points[0].x-n->down_x;
        float direction=sign(n)*(n->opening?1:-1);
        sample(n,f->time_ms,y);
        if(n->phase!=BOT_NAV_DRAGGING && dy*direction>BOT_OS_SLOP && fabsf(dy)>=fabsf(dx)*BOT_OS_AXIS_RATIO) {
            n->origin_y=n->down_y+direction*BOT_OS_SLOP;
            n->origin_position=n->position;n->phase=BOT_NAV_DRAGGING;
        }
        if(n->phase==BOT_NAV_DRAGGING) {
            n->position=n->origin_position+(y-n->origin_y)*sign(n);
            n->velocity=speed(n,f->time_ms)*sign(n);
        }
    }
    if(!f->count && n->session) {
        bool owned=n->owned;
        if(n->phase==BOT_NAV_DRAGGING) {
            if(f->final_position && f->points[0].id==n->track_id) {
                sample(n,f->time_ms,f->points[0].y);
                n->position=n->origin_position+(f->points[0].y-n->origin_y)*sign(n);
            }
            float v=speed(n,f->time_ms)*sign(n);
            float directed=v*(n->opening?1:-1);
            float distance=(n->position-n->origin_position)*(n->opening?1:-1);
            float completed=n->opening?n->position:BOT_OS_TRAVEL-n->position;
            bool finish=completed>=BOT_OS_COMPLETE*BOT_OS_TRAVEL;
            if(distance>=BOT_OS_FLICK_MIN && directed>=BOT_OS_FLICK_SPEED)finish=true;
            if(directed<=-BOT_OS_FLICK_SPEED)finish=false;
            settle(n,finish?n->opening:n->stable_open,v,f->time_ms);
        }
        n->session=false;n->candidate=false;n->owned=false;
        return owned;
    }
    return n->owned;
}
void bot_navigation_dismiss(bot_navigation_t *n,uint32_t now) {
    bot_navigation_tick(n,now);
    if(n->card!=BOT_SCR_FACE && (n->position>0 || n->phase!=BOT_NAV_FACE))settle(n,false,n->velocity,now);
}
void bot_navigation_show(bot_navigation_t *n,bot_screen_t page) {
    bot_navigation_init(n);n->card=page;n->stable_open=page!=BOT_SCR_FACE;
    n->position=n->stable_open?BOT_OS_TRAVEL:0;n->phase=n->stable_open?BOT_NAV_PANEL:BOT_NAV_FACE;
}
float bot_navigation_y(const bot_navigation_t *n) {
    float p=n->phase==BOT_NAV_DRAGGING?visual(n->position):n->position;
    return BOT_OS_INSET-sign(n)*(BOT_OS_TRAVEL-p);
}
float bot_navigation_progress(const bot_navigation_t *n){return clamp(n->position/BOT_OS_TRAVEL,0,1);}
bool bot_navigation_content_enabled(const bot_navigation_t *n) {
    return n->phase==BOT_NAV_PANEL && !n->candidate && !n->owned;
}
