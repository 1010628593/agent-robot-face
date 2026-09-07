/* Diagnostic replay of the production LVGL UI. No alternative motion engine.
 * Input: time_ms count x0 y0 x1 y1 [cancel]. Outputs PPM + CSV on each row.
 * Transport is the existing host build. Explicit fixture command uses labeled review samples, never device production data. */
#include "bot_ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint32_t clock_ms,pixels[466][466],draw_buffer[466*32];
static lv_display_t *display;
static lv_indev_t *pointer;
extern lv_obj_t *bot_ui_screen(void);
static uint32_t get_clock(void){return clock_ms;}
lv_indev_t *bsp_display_get_input_dev(void){return pointer;}
static void read_pointer(lv_indev_t *d,lv_indev_data_t *v){(void)d;v->state=LV_INDEV_STATE_RELEASED;v->continue_reading=false;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *map){
    size_t w=(size_t)(a->x2-a->x1+1);
    for(int y=a->y1;y<=a->y2;y++)memcpy(&pixels[y][a->x1],map+(y-a->y1)*lv_draw_buf_width_to_stride(w,LV_COLOR_FORMAT_XRGB8888),w*4);
    lv_display_flush_ready(d);
}
static void render(void){bot_ui_poll();lv_timer_handler();lv_obj_update_layout(bot_ui_screen());lv_refr_now(display);}
static void advance(uint32_t until){while(clock_ms<until){clock_ms+=(until-clock_ms>10?10:until-clock_ms);render();}}
static void snapshot(const char *dir,unsigned index){
    if(getenv("BOT_REPLAY_NO_IMAGES"))return;
    char path[1024];snprintf(path,sizeof(path),"%s/%04u.ppm",dir,index);
    FILE *f=fopen(path,"wb");if(!f){perror(path);exit(2);}fprintf(f,"P6\n466 466\n255\n");
    for(int y=0;y<466;y++)for(int x=0;x<466;x++){
        uint32_t p=pixels[y][x];if((x-233)*(x-233)+(y-233)*(y-233)>233*233)p=0;
        unsigned char rgb[]={p>>16,p>>8,p};fwrite(rgb,1,3,f);
    }
    fclose(f);
}
extern void stats_refresh(void);extern void stats_tap(int);extern uint8_t stats_depth(void);
static bot_usage_number_t num(double value){return (bot_usage_number_t){true,value};}
static void fixture(void){
 bot_model_t *m=&g_ui.model;m->state=BOT_MS_ONLINE;m->has_usage=true;
 bot_usage_t *u=&m->usage;memset(u,0,sizeof(*u));u->view=(bot_usage_view_t){0,0,0,1};m->usage_view=u->view;
 u->data_rev=7;u->host_now_ms=1788768000000ULL;u->as_of_ms=num(1788768000000.);u->summary_agent=2;u->total=num(1846000);u->input=num(1200000);u->output=num(646000);u->cache_read=num(820000);u->cache_write=num(64000);u->cost_micros=num(1834500);strcpy(u->cost_currency,"USD");strcpy(u->cost_source,"multiple_native_sources");u->cost_coverage=1;
 bot_agent_id_t ids[]={BOT_AGENT_CODEX,BOT_AGENT_CURSOR,BOT_AGENT_HERMES,BOT_AGENT_WORKBUDDY};
 for(int i=0;i<4;i++){u->agents[i].id=ids[i];u->agents[i].available=i<3;}
 u->agents[0].used_pct=num(21);u->agents[1].used_pct=num(73);u->agents[2].total=num(846000);
 u->quota_count=u->quota_total=3;const char *labels[]={"5hour","week","month"};double p[]={21,73,86};
 for(int i=0;i<3;i++){snprintf(u->quotas[i].id,33,"%032u",i+1);strcpy(u->quotas[i].label,labels[i]);u->quotas[i].agent_id=i==1?BOT_AGENT_CURSOR:BOT_AGENT_CODEX;u->quotas[i].used_pct=num(p[i]);u->quotas[i].reset_ms=num(u->as_of_ms.value+8280000);}
 u->model_count=u->model_total=3;const char *models[]={"gpt-5.4","claude-sonnet-4.6","gpt-5.4-mini"};for(int i=0;i<3;i++){strcpy(u->models[i].label,models[i]);u->models[i].total=num(846000-i*125000);}
 stats_refresh();
}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: replay_os_ui output-dir < trace.txt\n");return 2;}
    lv_init();lv_tick_set_cb(get_clock);display=lv_display_create(466,466);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);pointer=lv_indev_create();
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);
    bot_ui_init();
    puts("frame,time_ms,count,phase,card,progress,y,velocity,stable_open,render_cpu_ms,objects,usage_depth");
    char line[256];unsigned index=0;float angle=0;
    while(fgets(line,sizeof(line),stdin)){
        if(sscanf(line,"rotation %f",&angle)==1) {
            bot_motion_view_t v={.available=true,.orientation_valid=true,.sampled_ms=clock_ms,.rotation_deg=angle};
            bot_ui_set_motion(&v);continue;
        }
        if(!strncmp(line,"fixture",7)){fixture();continue;}
        if(!strncmp(line,"stats",5)){bot_ui_show_stats();continue;}
        if(!strncmp(line,"ack",3)){bot_model_t *m=&g_ui.model;if(m->usage_pending){m->usage_view=m->requested_usage_view;m->usage_view.usage_rev++;m->usage.view=m->usage_view;m->usage_pending=false;}stats_refresh();continue;}
        int hit;if(sscanf(line,"tap %d",&hit)==1){stats_tap(hit);continue;}
        if(sscanf(line,"percent %d",&hit)==1){for(int i=0;i<3;i++)g_ui.model.usage.quotas[i].used_pct=num(hit);stats_refresh();continue;}
        if(!strncmp(line,"unknown",7)){g_ui.model.usage.quotas[0].used_pct.has=false;stats_refresh();continue;}
        unsigned ms,c;int x0=233,y0=233,x1=233,y1=233,cancel=0;
        if(sscanf(line,"%u %u %d %d %d %d %d",&ms,&c,&x0,&y0,&x1,&y1,&cancel)<2)continue;
        advance(ms);bot_touch_frame_t f={.time_ms=ms,.count=(uint8_t)c,.cancelled=cancel,
            .points={{0,x0,y0},{1,x1,y1}}};
        bot_ui_touch_frame(&f);clock_t start=clock();render();
        double elapsed=1000.*(clock()-start)/CLOCKS_PER_SEC;
        const bot_navigation_t *n=bot_ui_navigation();
        printf("%u,%u,%u,%d,%d,%.4f,%.2f,%.2f,%d,%.3f,%u,%u\n",index,ms,c,n->phase,n->card,
            bot_navigation_progress(n),bot_navigation_y(n),n->velocity,n->stable_open,elapsed,
            (unsigned)lv_obj_get_child_count(bot_ui_screen()),stats_depth());snapshot(argv[1],index++);
    }
    return 0;
}
