/* Real ui.c and gesture/router, with a fake display/indev (not a panel test). */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_ui.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
void picker_build(lv_obj_t *s){(void)s;}
void picker_refresh(void){}
void stats_build(lv_obj_t *s){(void)s;}
void stats_refresh(void){}
#ifdef OLD_FACE_STUB
void face_build(lv_obj_t *s){(void)s;}
void face_tick(uint32_t t){(void)t;}
#endif
static void poll(uint32_t t,bool down,int x,int y){
    fake_now=t;fake_indev.state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
    fake_indev.p=(lv_point_t){x,y};bot_ui_poll();
}
int main(void){
    bot_ui_init();
    lv_obj_t *badge=fake_find_label("SIM");
    assert(badge && badge->x>=200 && badge->y+badge->h<58); /* above picker heading */
    int clean=fake_cleans,created=fake_creates;
    for(uint32_t t=10;t<=7200;t+=10)poll(t,false,233,233);
    assert(fake_cleans==clean && fake_creates==created); /* state change does NOT rebuild */
    poll(8000,true,233,233);poll(8649,true,233,233);
    assert(g_ui.screen==BOT_SCR_FACE);
    poll(8650,true,260,233); /* latest sample moved at exact HOLD boundary */
    assert(g_ui.screen==BOT_SCR_FACE);
    poll(8700,false,260,233);
    poll(9000,true,233,233);poll(9650,true,233,233);
    assert(g_ui.screen==BOT_SCR_PICKER);
    poll(9670,false,233,233);assert(g_ui.screen==BOT_SCR_PICKER); /* release isn't tap */
    poll(10000,true,320,233);poll(10180,true,200,233);poll(10200,false,200,233);
    assert(g_ui.picker_preview==BOT_AGENT_WORKBUDDY);
    poll(10300,true,233,233);poll(10400,false,233,233);
    assert(g_ui.screen==BOT_SCR_FACE && g_ui.selected==BOT_AGENT_WORKBUDDY);
    poll(11000,true,320,233);poll(11180,true,200,233);poll(11200,false,200,233);
    assert(g_ui.screen==BOT_SCR_STATS);
    poll(11300,true,233,320);poll(11480,true,233,200);poll(11500,false,233,200);
    assert(g_ui.stats_tab==1);
    poll(11600,true,200,233);poll(11780,true,320,233);poll(11800,false,320,233);
    assert(g_ui.screen==BOT_SCR_FACE);
    clean=fake_cleans;created=fake_creates;
    for(uint32_t t=11810;t<18000;t+=10)poll(t,false,233,233);
    assert(fake_cleans==clean && fake_creates==created);
#ifndef OLD_FACE_STUB
    assert(fake_draws>0 && fake_invalidations>0);
    /* 100 Hz poll must not cause 100 Hz face invalidations. */
    assert(fake_invalidations<650);
#endif
    puts("UI lifecycle: no rebuilds, move-before-hold, picker/stats routes, bounded redraw passed");
    return 0;
}
