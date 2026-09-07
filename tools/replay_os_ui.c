/* Diagnostic replay of the production LVGL UI. No alternative motion engine.
 * Input: time_ms count x0 y0 x1 y1 [cancel]. Outputs PPM + CSV on each row.
 * Transport is the existing host build; account data is deliberately absent. */
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
    for(int y=a->y1;y<=a->y2;y++)memcpy(&pixels[y][a->x1],map+(y-a->y1)*w*4,w*4);
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
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: replay_os_ui output-dir < trace.txt\n");return 2;}
    lv_init();lv_tick_set_cb(get_clock);display=lv_display_create(466,466);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);pointer=lv_indev_create();
    lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_pointer);
    bot_ui_init();
    puts("frame,time_ms,count,phase,card,progress,y,velocity,stable_open,render_cpu_ms,objects");
    char line[256];unsigned index=0;float angle=0;
    while(fgets(line,sizeof(line),stdin)){
        if(sscanf(line,"rotation %f",&angle)==1) {
            bot_motion_view_t v={.available=true,.orientation_valid=true,.sampled_ms=clock_ms,.rotation_deg=angle};
            bot_ui_set_motion(&v);continue;
        }
        unsigned ms,c;int x0=233,y0=233,x1=233,y1=233,cancel=0;
        if(sscanf(line,"%u %u %d %d %d %d %d",&ms,&c,&x0,&y0,&x1,&y1,&cancel)<2)continue;
        advance(ms);bot_touch_frame_t f={.time_ms=ms,.count=(uint8_t)c,.cancelled=cancel,
            .points={{0,x0,y0},{1,x1,y1}}};
        bot_ui_touch_frame(&f);clock_t start=clock();render();
        double elapsed=1000.*(clock()-start)/CLOCKS_PER_SEC;
        const bot_navigation_t *n=bot_ui_navigation();
        printf("%u,%u,%u,%d,%d,%.4f,%.2f,%.2f,%d,%.3f,%u\n",index,ms,c,n->phase,n->card,
            bot_navigation_progress(n),bot_navigation_y(n),n->velocity,n->stable_open,elapsed,
            (unsigned)lv_obj_get_child_count(bot_ui_screen()));snapshot(argv[1],index++);
    }
    return 0;
}
