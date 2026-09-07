/* Real LVGL software renderer, production three-screen code, virtual hardware.
 * Each CTest case is a fresh process. No ESP-IDF, USB or Agent account access. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_ui.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t clock_ms;
static lv_display_t *display;
static lv_indev_t *pointer;
static lv_point_t contact = {233,233};
static bool down;
static uint32_t pixels[466][466];
static uint32_t draw_buffer[466 * 32];
extern lv_obj_t *bot_ui_screen(void);
static uint32_t get_clock(void) { return clock_ms; }
lv_indev_t *bsp_display_get_input_dev(void) { return pointer; }
static void read_pointer(lv_indev_t *dev, lv_indev_data_t *data)
{
    (void)dev;
    data->point=contact;
    data->state=down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading=false;
}
static void flush(lv_display_t *dev, const lv_area_t *a, uint8_t *map)
{
    assert(a->x1>=0 && a->y1>=0 && a->x2<466 && a->y2<466);
    size_t width=(size_t)(a->x2-a->x1+1);
    for(int y=a->y1;y<=a->y2;y++)
        memcpy(&pixels[y][a->x1],map+(size_t)(y-a->y1)*width*4,width*4);
    lv_display_flush_ready(dev);
}
static void tick(uint32_t elapsed)
{
    for(uint32_t i=0;i<elapsed;i+=10) {
        clock_ms+=10;
        g_ui.sim_cycle_ms=clock_ms; /* Suppress only automatic demo state advances. */
        lv_indev_read(pointer);
        bot_ui_poll();
        lv_obj_update_layout(bot_ui_screen());
        lv_refr_now(display);
    }
}
static void tap(int x,int y)
{
    contact=(lv_point_t){x,y};down=true;tick(50);down=false;tick(50);
}
static void swipe(int x1,int y1,int x2,int y2)
{
    contact=(lv_point_t){x1,y1};down=true;tick(10);
    contact=(lv_point_t){x2,y2};tick(170);down=false;tick(20);
}
static lv_obj_t *label(lv_obj_t *root,const char *text)
{
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),text))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *found=label(lv_obj_get_child(root,(int32_t)i),text);
        if(found)return found;
    }
    return NULL;
}
static unsigned bars(lv_obj_t *root)
{
    unsigned n=lv_obj_check_type(root,&lv_bar_class)?1:0;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);i++)n+=bars(lv_obj_get_child(root,(int32_t)i));
    return n;
}
static unsigned lit(int x1,int y1,int x2,int y2)
{
    unsigned n=0;
    for(int y=y1;y<=y2;y++)for(int x=x1;x<=x2;x++)n+=(pixels[y][x]&0xffffff)!=0;
    return n;
}
static void show_stats(uint8_t agent,uint8_t tab)
{
    g_ui.selected=agent;g_ui.stats_tab=tab;g_ui.screen=BOT_SCR_STATS;
    bot_ui_show_stats();tick(50);
}
static void snapshot(const char *name)
{
    const char *dir=getenv("BOT_SNAPSHOT_DIR");if(!dir)return;
    char path[1024];int len=snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);
    assert(len>0 && (size_t)len<sizeof(path));
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n466 466\n255\n");
    for(int y=0;y<466;y++)for(int x=0;x<466;x++) {
        uint32_t p=pixels[y][x];unsigned char rgb[]={p>>16,p>>8,p};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(fclose(f)==0);
}
static void set_state(uint8_t agent,bot_state_t state)
{
    g_ui.agents[agent].state=state;g_ui.agents[agent].transition_id++;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    lv_init();lv_tick_set_cb(get_clock);
    display=lv_display_create(466,466);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    pointer=lv_indev_create();assert(pointer);
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);
    bot_ui_init();tick(500);
    lv_obj_t *root=bot_ui_screen();
    if(!strcmp(argv[1],"smoke")) {
        assert(label(root,"SIM"));assert(lit(100,140,365,319)>1000);snapshot("face");
        /* The first DOWN is sampled after 10ms; 660ms gives a full observed hold. */
        contact=(lv_point_t){233,233};down=true;tick(660);down=false;tick(50);
        assert(g_ui.screen==BOT_SCR_PICKER);snapshot("picker");
        tap(233,233);assert(g_ui.screen==BOT_SCR_FACE);
        swipe(310,233,200,233);assert(g_ui.screen==BOT_SCR_STATS);snapshot("usage");
        swipe(233,300,233,190);assert(g_ui.stats_tab==1);snapshot("quota");
        swipe(190,233,310,233);assert(g_ui.screen==BOT_SCR_FACE);
        set_state(BOT_AGENT_CODEX,BOT_STATE_DONE);tick(1800);snapshot("done");
        set_state(BOT_AGENT_CODEX,BOT_STATE_ERROR);tick(800);snapshot("error");
        set_state(BOT_AGENT_CODEX,BOT_STATE_WAITING);tick(500);snapshot("waiting");
        set_state(BOT_AGENT_CODEX,BOT_STATE_TOOL);tick(500);snapshot("tool");
    } else if(!strcmp(argv[1],"stats_tab")) {
        show_stats(BOT_AGENT_CODEX,0);assert(label(root,"USAGE"));
        swipe(233,300,233,190);assert(g_ui.stats_tab==1);
        assert(label(root,"QUOTA") && !label(root,"USAGE"));
        swipe(233,190,233,300);assert(label(root,"USAGE") && !label(root,"QUOTA"));
    } else if(!strcmp(argv[1],"quota_unknown")) {
        show_stats(BOT_AGENT_WORKBUDDY,1);assert(label(root,"N/A"));assert(bars(root)==0);snapshot("quota-na");
        show_stats(BOT_AGENT_HERMES,1);assert(label(root,"UNLIMITED"));assert(bars(root)==0);snapshot("quota-unlimited");
        g_ui.agents[BOT_AGENT_CODEX].quota[0].used_pct=-1;
        g_ui.agents[BOT_AGENT_CODEX].quota[1].label="";
        show_stats(BOT_AGENT_CODEX,1);assert(label(root,"N/A") && bars(root)==0);
    } else if(!strcmp(argv[1],"quota_full")) {
        g_ui.agents[BOT_AGENT_CODEX].quota[0].used_pct=100;
        g_ui.agents[BOT_AGENT_CODEX].quota[1].label="";
        show_stats(BOT_AGENT_CODEX,1);assert(label(root,"0% LEFT") && !label(root,"OVER"));
        g_ui.agents[BOT_AGENT_CODEX].quota[0].used_pct=101;
        show_stats(BOT_AGENT_CODEX,1);assert(label(root,"OVER"));
    } else if(!strcmp(argv[1],"picker_target")) {
        g_ui.picker_return=BOT_SCR_FACE;g_ui.picker_preview=BOT_AGENT_CURSOR;
        g_ui.screen=BOT_SCR_PICKER;bot_ui_show_picker();tick(50);
        tap(50,225);assert(g_ui.screen==BOT_SCR_PICKER && g_ui.selected==BOT_AGENT_CODEX);
        tap(233,75);assert(g_ui.screen==BOT_SCR_PICKER);
        /* Starting outside and drifting inside is not a deliberate confirmation. */
        contact=(lv_point_t){158,200};down=true;tick(20);
        contact=(lv_point_t){164,200};down=false;tick(20);assert(g_ui.screen==BOT_SCR_PICKER);
        tap(233,233);assert(g_ui.screen==BOT_SCR_FACE && g_ui.selected==BOT_AGENT_CURSOR);
    } else if(!strcmp(argv[1],"terminal_resume")) {
        set_state(BOT_AGENT_CODEX,BOT_STATE_DONE);tick(2000);
        assert(lit(150,250,187,275)==0); /* smile has no lower pill body */
        g_ui.selected=BOT_AGENT_CURSOR;set_state(BOT_AGENT_CURSOR,BOT_STATE_IDLE);tick(500);
        assert(lit(150,250,187,275)>100);
        g_ui.selected=BOT_AGENT_CODEX;tick(100);
        assert(lit(150,250,187,275)==0); /* selection must not replay the old DONE */
    } else if(!strcmp(argv[1],"terminal_hidden")) {
        show_stats(BOT_AGENT_CODEX,0);set_state(BOT_AGENT_CODEX,BOT_STATE_DONE);tick(2000);
        swipe(190,233,310,233);assert(g_ui.screen==BOT_SCR_FACE);
        assert(lit(150,250,187,275)==0); /* event age advances while Stats is visible */
    } else if(!strcmp(argv[1],"cycling")) {
        for(unsigned i=0;i<100;i++) {
            show_stats(i%BOT_AGENT_COUNT,i%2);
            g_ui.picker_return=BOT_SCR_STATS;g_ui.picker_preview=i%BOT_AGENT_COUNT;
            g_ui.screen=BOT_SCR_PICKER;bot_ui_show_picker();tick(50);
            tap(233,233);assert(g_ui.screen==BOT_SCR_FACE);
            set_state(g_ui.selected,(bot_state_t)(i%8));tick(300);
        }
    } else {fprintf(stderr,"Unknown case: %s\n",argv[1]);return 2;}
    puts("PASS real LVGL UI scenario");lv_deinit();return 0;
}
