/* Real LVGL software rendering of production UI with virtual physical I/O.
 * Each CTest scenario starts in a new process; account data remains SIM. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_ui.h"
#include "bot_face.h"
#include "bot_link.h"
#include "lvgl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t clock_ms;
static lv_display_t *display;
static lv_indev_t *pointer;
static lv_point_t contact={233,233};
static bool down;
static bool motion_enabled;
static float motion_angle;
static uint32_t motion_event_ms,motion_event_id;
static bot_reaction_t motion_reaction;
static uint32_t pixels[466][466], draw_buffer[466*32];
extern lv_obj_t *bot_ui_screen(void);
static uint32_t get_clock(void) { return clock_ms; }
lv_indev_t *bsp_display_get_input_dev(void) { return pointer; }
static void read_pointer(lv_indev_t *dev,lv_indev_data_t *data)
{
    (void)dev;data->point=contact;
    data->state=down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
    data->continue_reading=false;
}
static void flush(lv_display_t *dev,const lv_area_t *a,uint8_t *map)
{
    assert(a->x1>=0 && a->y1>=0 && a->x2<466 && a->y2<466);
    size_t width=(size_t)(a->x2-a->x1+1);
    for(int y=a->y1;y<=a->y2;y++)memcpy(&pixels[y][a->x1],map+(size_t)(y-a->y1)*width*4,width*4);
    lv_display_flush_ready(dev);
}
static void tick(uint32_t elapsed)
{
    for(uint32_t i=0;i<elapsed;i+=10) {
        clock_ms+=10;g_ui.sim_cycle_ms=clock_ms;
        if(motion_enabled) {
            bot_motion_view_t v={.available=true,.orientation_valid=true,.sampled_ms=clock_ms,
                .rotation_deg=motion_angle,.reaction=motion_reaction,.event_ms=motion_event_ms,.event_id=motion_event_id};
            bot_ui_set_motion(&v);
        }
        lv_indev_read(pointer);bot_ui_poll();
        lv_obj_update_layout(bot_ui_screen());lv_refr_now(display);
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
        lv_obj_t *found=label(lv_obj_get_child(root,(int32_t)i),text);if(found)return found;
    }
    return NULL;
}
static void fits(lv_obj_t *root,const char *text)
{
    lv_obj_t *l=label(root,text);assert(l);
    lv_point_t size;
    lv_text_get_size(&size,text,lv_obj_get_style_text_font(l,0),
        lv_obj_get_style_text_letter_space(l,0),0,10000,LV_TEXT_FLAG_NONE);
    assert(size.x<=lv_obj_get_width(l) && size.y<=lv_obj_get_height(l));
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
/* Read the actual face coordinate mapping, independent of LVGL layer styles. */
static int rotation(void)
{
    int16_t x,y;
    face_map_input(down,333,233,&x,&y);
    int angle=(int)lroundf(atan2f(233-y,x-233)*572.95779513f);
    return (angle+3600)%3600;
}
int main(int argc,char **argv)
{
    assert(argc==2);lv_init();lv_tick_set_cb(get_clock);
    display=lv_display_create(466,466);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    pointer=lv_indev_create();assert(pointer);
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);
    bot_ui_init();tick(500);lv_obj_t *root=bot_ui_screen();
    if(!strcmp(argv[1],"production")) {
        g_ui.dev_sim=false;bot_model_init(&g_ui.model);bot_link_start();
        bot_ui_show_face();tick(60);assert(!label(root,"SIM"));snapshot("production-face-disconnected");
        show_stats(BOT_AGENT_CODEX,0);assert(label(root,"连接已断开"));snapshot("production-disconnected");
        static bot_frame_parser_t parser;static bj_node_t nodes[640];static char pool[8193];static bot_msg_t msg;
        bot_frame_init(&parser,nodes,640,pool,sizeof(pool));
        const char *welcome="@bot {\"v\":2,\"type\":\"welcome\",\"link_id\":\"33333333333333333333333333333333\",\"seq\":1,\"body\":{\"bridge_epoch\":\"44444444444444444444444444444444\",\"mode\":\"auto\",\"selected_agent\":\"codex\",\"selection_rev\":1,\"heartbeat_ms\":2000,\"offline_after_ms\":6000,\"max_frame_bytes\":8192,\"demo\":false}}\n";
        bool applied=false;for(const char *p=welcome;*p;p++)if(bot_frame_feed(&parser,*p,&msg)==BOT_FRAME_MSG){assert(bot_model_apply(&g_ui.model,&msg)==BOT_APPLY_APPLIED);applied=true;}assert(applied);
        bot_ui_project(clock_ms);g_ui.screen=BOT_SCR_PICKER;g_ui.picker_preview=4;bot_ui_show_picker();tick(40);assert(label(root,"自动"));snapshot("production-picker-auto");
        for(int i=0;i<4;i++){g_ui.picker_preview=i;bot_ui_show_picker();tick(30);char n[40];snprintf(n,sizeof(n),"production-picker-%d",i);snapshot(n);}
        g_ui.picker_preview=2;tap(233,233);assert(g_ui.model.action_pending && g_ui.selected==BOT_AGENT_CODEX);snapshot("production-picker-pending");
        msg=(bot_msg_t){.type=BOT_MSG_ACK,.has_link_id=true,.seq=2};strcpy(msg.link_id,g_ui.model.link_id);strcpy(msg.body.ack.action_id,g_ui.model.pending_action_id);msg.body.ack.mode=BOT_SELECTION_PINNED;msg.body.ack.selected_agent=BOT_AGENT_CURSOR;msg.body.ack.selection_rev=2;
        assert(bot_model_apply(&g_ui.model,&msg)==BOT_APPLY_APPLIED);bot_ui_project(clock_ms);assert(g_ui.selected==BOT_AGENT_CURSOR && g_ui.model.mode==BOT_SELECTION_PINNED);
        show_stats(BOT_AGENT_CURSOR,0);snapshot("production-task-empty");show_stats(BOT_AGENT_CURSOR,1);snapshot("production-today-empty");show_stats(BOT_AGENT_CURSOR,2);snapshot("production-quota-empty");
        g_ui.model.has_focus=true;g_ui.model.focus=(bot_focus_t){.agent_id=BOT_AGENT_CURSOR,.selection_rev=2,.state=BOT_STATE_WAITING,.reason=BOT_REASON_APPROVAL,.quality=BOT_QUALITY_OBSERVED,.has_run_id=true,.run_elapsed_ms=125000};
        strcpy(g_ui.model.focus.run_id,"observed-run-1234");g_ui.model.focus.active_sessions=2;
        show_stats(BOT_AGENT_CURSOR,0);snapshot("production-task-waiting");
        tap(342,112);assert(g_ui.stats_tab==2);tap(126,112);assert(g_ui.stats_tab==0);
        g_ui.model.focus.stale=true;show_stats(BOT_AGENT_CURSOR,0);assert(label(root,"数据已过期"));snapshot("production-task-stale");
        g_ui.model.has_stats=true;g_ui.model.stats=(bot_stats_t){.agent_id=BOT_AGENT_CURSOR,.selection_rev=2,.sent_at_ms=100000,.metric_count=1,.quota_count=1};g_ui.stats_received_ms=clock_ms;
        g_ui.model.stats.metrics[0]=(bot_metric_t){.key=BOT_MKEY_TURNS,.quality=BOT_MQUAL_UNAVAILABLE,.coverage=BOT_COV_UNKNOWN,.stale_after_ms=60000};strcpy(g_ui.model.stats.metrics[0].source,"cursor_adapter");
        show_stats(BOT_AGENT_CURSOR,1);snapshot("production-today-missing");
        g_ui.model.stats.quotas[0]=(bot_quota_t){.kind=BOT_QKIND_UNKNOWN,.quality=BOT_MQUAL_UNAVAILABLE,.availability=BOT_QAVAIL_ERROR,.stale_after_ms=60000};strcpy(g_ui.model.stats.quotas[0].label,"Account");strcpy(g_ui.model.stats.quotas[0].source,"cursor_adapter");
        show_stats(BOT_AGENT_CURSOR,2);assert(label(root,"配额读取错误"));snapshot("production-quota-error");
        /* Host selected this same triple before the matching device ACK. */
        const char same_id[33]="66666666666666666666666666666666";
        bot_model_track_action(&g_ui.model,same_id);msg=(bot_msg_t){.type=BOT_MSG_ACK,.has_link_id=true,.seq=3};strcpy(msg.link_id,g_ui.model.link_id);strcpy(msg.body.ack.action_id,same_id);msg.body.ack.mode=BOT_SELECTION_PINNED;msg.body.ack.selected_agent=BOT_AGENT_CURSOR;msg.body.ack.selection_rev=2;
        assert(bot_model_apply(&g_ui.model,&msg)==BOT_APPLY_APPLIED);assert(g_ui.model.has_focus && g_ui.model.has_stats);
        tick(6100);assert(g_ui.model.state==BOT_MS_HANDSHAKING);snapshot("production-timeout");
    } else
    if(!strcmp(argv[1],"smoke")) {
        assert(label(root,"SIM"));assert(lit(100,140,365,319)>1000);snapshot("face");
        contact=(lv_point_t){233,233};down=true;tick(660);down=false;tick(50);
        assert(g_ui.screen==BOT_SCR_PICKER);snapshot("picker");
        tap(233,233);assert(g_ui.screen==BOT_SCR_FACE);
        swipe(435,233,320,233);assert(g_ui.screen==BOT_SCR_STATS);snapshot("usage");
        swipe(233,300,233,190);assert(g_ui.stats_tab==1);
        fits(root,"SHORT WINDOW");fits(root,"LONG WINDOW");fits(root,"73% LEFT");snapshot("quota");
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
        show_stats(BOT_AGENT_HERMES,1);fits(root,"UNLIMITED");assert(bars(root)==0);snapshot("quota-unlimited");
        g_ui.agents[BOT_AGENT_CODEX].quota[0].used_pct=-1;g_ui.agents[BOT_AGENT_CODEX].quota[1].label="";
        show_stats(BOT_AGENT_CODEX,1);assert(label(root,"N/A") && bars(root)==0);
    } else if(!strcmp(argv[1],"quota_full")) {
        g_ui.agents[BOT_AGENT_CODEX].quota[0].used_pct=100;g_ui.agents[BOT_AGENT_CODEX].quota[1].label="";
        show_stats(BOT_AGENT_CODEX,1);assert(label(root,"0% LEFT") && !label(root,"OVER"));
        g_ui.agents[BOT_AGENT_CODEX].quota[0].used_pct=101;show_stats(BOT_AGENT_CODEX,1);assert(label(root,"OVER"));
    } else if(!strcmp(argv[1],"picker_target")) {
        g_ui.picker_return=BOT_SCR_FACE;g_ui.picker_preview=BOT_AGENT_CURSOR;
        g_ui.screen=BOT_SCR_PICKER;bot_ui_show_picker();tick(50);
        tap(50,225);assert(g_ui.screen==BOT_SCR_PICKER && g_ui.selected==BOT_AGENT_CODEX);
        tap(233,75);assert(g_ui.screen==BOT_SCR_PICKER);
        contact=(lv_point_t){158,200};down=true;tick(20);
        contact=(lv_point_t){164,200};down=false;tick(20);assert(g_ui.screen==BOT_SCR_PICKER);
        tap(233,233);assert(g_ui.screen==BOT_SCR_FACE && g_ui.selected==BOT_AGENT_CURSOR);
    } else if(!strcmp(argv[1],"terminal_resume")) {
        set_state(BOT_AGENT_CODEX,BOT_STATE_DONE);tick(2000);assert(lit(150,250,187,275)==0);
        g_ui.selected=BOT_AGENT_CURSOR;set_state(BOT_AGENT_CURSOR,BOT_STATE_ERROR);tick(1000);
        assert(lit(150,250,187,275)>10);
        g_ui.selected=BOT_AGENT_CODEX;tick(100);assert(lit(150,250,187,275)==0);
    } else if(!strcmp(argv[1],"terminal_hidden")) {
        show_stats(BOT_AGENT_CODEX,0);set_state(BOT_AGENT_CODEX,BOT_STATE_DONE);tick(2000);
        swipe(190,233,310,233);assert(g_ui.screen==BOT_SCR_FACE);
        assert(lit(150,250,187,275)==0);
    } else if(!strcmp(argv[1],"cycling")) {
        for(unsigned i=0;i<100;i++) {
            show_stats(i%BOT_AGENT_COUNT,i%2);
            g_ui.picker_return=BOT_SCR_STATS;g_ui.picker_preview=i%BOT_AGENT_COUNT;
            g_ui.screen=BOT_SCR_PICKER;bot_ui_show_picker();tick(50);
            tap(233,233);assert(g_ui.screen==BOT_SCR_FACE);
            set_state(g_ui.selected,(bot_state_t)(i%8));tick(300);
        }
    } else if(!strcmp(argv[1],"motion_rotation")) {
        set_state(BOT_AGENT_CODEX,BOT_STATE_IDLE);tick(250);
        motion_enabled=true;motion_angle=90;tick(600);
        assert(rotation()==900);
        assert(lit(140,210,160,256)==0 && lit(212,135,255,175)>200);snapshot("motion-90");
        assert(label(root,"SIM"));
        contact=(lv_point_t){233,296};down=true;tick(50);
        motion_angle=-90;tick(300);
        assert(rotation()==900);
        down=false;tick(900);
        assert(rotation()==2700);
        motion_angle=45;tick(900);snapshot("motion-45");
        motion_enabled=false;tick(500);
        assert(rotation()==450);
    } else if(!strcmp(argv[1],"motion_reaction")) {
        motion_enabled=true;motion_reaction=BOT_REACTION_DIZZY;
        motion_event_ms=clock_ms;motion_event_id=1;tick(500);snapshot("motion-dizzy");
        assert(g_ui.agents[BOT_AGENT_CODEX].state==BOT_STATE_WORKING);
        set_state(BOT_AGENT_CODEX,BOT_STATE_WAITING);tick(300);snapshot("motion-waiting-priority");
        set_state(BOT_AGENT_CODEX,BOT_STATE_WORKING);tick(300);
        assert((pixels[233][170]&0xffffff)==0); /* centered opaque pupil, no delayed dizzy gaze */
        show_stats(BOT_AGENT_CODEX,0);motion_event_id=2;motion_event_ms=clock_ms;tick(200);
        g_ui.screen=BOT_SCR_FACE;bot_ui_show_face();tick(300);
        assert((pixels[233][170]&0xffffff)==0); /* hidden reaction consumed, ordinary pupil remains */
    } else if(!strcmp(argv[1],"motion_preview")) {
        /* Reproducible code-rendered frames, not hardware data or a new UI. */
        set_state(BOT_AGENT_CODEX,BOT_STATE_IDLE);tick(250);
        motion_enabled=true;motion_reaction=BOT_REACTION_NONE;
        for(unsigned action=0;action<7;action++) {
            face_pet(action>=4,clock_ms);
            tick(300);char name[48];snprintf(name,sizeof(name),"pet-%u",action);snapshot(name);
            tick(1700);
        }
        set_state(BOT_AGENT_CODEX,BOT_STATE_CANCELLED);tick(300);snapshot("cancelled");
        set_state(BOT_AGENT_CODEX,BOT_STATE_IDLE);tick(300);
        for(unsigned f=0;f<60;f++) {
            motion_angle=f<30?(float)f*3:(float)(60-f)*3;
            tick(40);char name[48];snprintf(name,sizeof(name),"level-%03u",f);snapshot(name);
        }
        motion_angle=0;tick(500);motion_reaction=BOT_REACTION_DIZZY;
        motion_event_ms=clock_ms;motion_event_id=1;
        for(unsigned f=0;f<70;f++) {
            tick(40);char name[48];snprintf(name,sizeof(name),"dizzy-%03u",f);snapshot(name);
        }
    } else {fprintf(stderr,"Unknown case: %s\n",argv[1]);return 2;}
    /* Release owned devices using their correctly typed APIs before global
     * teardown. This also exercises actual widget deletion callbacks. */
    lv_indev_delete(pointer);
    pointer=NULL;
    lv_display_delete(display);
    display=NULL;
    lv_deinit();
    puts("PASS real LVGL UI scenario");
    return 0;
}
