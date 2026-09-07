/* CST9217 two-point input through the existing adapter's custom read hook.
 * Protocol reference: Waveshare 10_Touch_CST9217 / SensorLib CST92xx.
 * Read D000 (15 bytes), acknowledge D000 AB, records at offsets 0 and 7.
 * One LVGL owner reads and drains this queue; no ISR writes or second poller.
 */
#include "bot_touch_input.h"
#include "esp_lv_adapter_input.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include <string.h>

#define QUEUE_SIZE 16
static bot_touch_frame_t queue[QUEUE_SIZE];
static unsigned head,tail;
static bool releasing;
static bot_touch_frame_t release_frame;
/* Absorb a brief all-up dropout without turning a two-finger interaction into
 * a fresh one-finger click. Keep the physical release timestamp for TAP. */
#define RELEASE_SETTLE_MS 35u
static uint32_t reads,dual_reads,errors,overflows,last_log;
static bot_touch_input_debug_t debug;
static void enqueue(bot_touch_frame_t f)
{
    unsigned next=(head+1)%QUEUE_SIZE;
    if(next==tail) {
        /* Missing a DOWN/UP may otherwise invent a tap. Cancel, then retain
         * the latest frame so a real all-up can safely re-arm the session. */
        tail=head=0;overflows++;
        queue[head++]=(bot_touch_frame_t){.time_ms=f.time_ms,.cancelled=true};
        next=(head+1)%QUEUE_SIZE;
    }
    queue[head]=f;head=next;
}
static esp_err_t read_frame(esp_lcd_touch_handle_t tp,esp_lcd_touch_point_data_t *points,
                            uint8_t *count,uint8_t max_count,void *ctx)
{
    (void)points;(void)max_count;(void)ctx;
    /* Native LVGL clicks are intentionally disabled. All selection goes
     * through the same release arbitration as face gestures. */
    *count=0;
    bot_touch_frame_t f={.time_ms=lv_tick_get()};
    uint8_t data[15]={0},address=0;
    esp_err_t err=esp_lcd_panel_io_tx_param(tp->io,0xD0,&address,1);
    if(err==ESP_OK)err=esp_lcd_panel_io_rx_param(tp->io,-1,data,sizeof(data));
    if(err==ESP_OK) {
        uint8_t ack[2]={0x00,0xAB};
        err=esp_lcd_panel_io_tx_param(tp->io,0xD0,ack,sizeof(ack));
    }
    reads++;
    if(err!=ESP_OK || data[6]!=0xAB)f.cancelled=true;
    else if(data[0]!=0xAB && !(data[4]&0x80)) {
        unsigned n=data[5]&0x7F;
        if(n>2)f.cancelled=true;
        bot_touch_point_t released={0};bool has_release=false;
        for(unsigned i=0;i<n && i<2 && !f.cancelled;i++) {
            const uint8_t *p=data+(i?7:0);
            uint8_t id=p[0]>>4,event=p[0]&15;
            if(event!=6 && event!=0){f.cancelled=true;break;}
            int x=(p[1]<<4)|(p[3]>>4),y=(p[2]<<4)|(p[3]&15);
            if(id>=2 || (f.count && f.points[0].id==id) || x>=466 || y>=466) {
                f.cancelled=true;break;
            }
            /* Match the BSP/software coordinate convention before the face's
             * frozen inverse rotation. This BSP has no custom adjustment. */
            if(tp->config.flags.mirror_x)x=tp->config.x_max-x;
            if(tp->config.flags.mirror_y)y=tp->config.y_max-y;
            if(tp->config.flags.swap_xy){int tmp=x;x=y;y=tmp;}
            bot_touch_point_t point={.id=id,.x=x,.y=y};
            if(event==6)f.points[f.count++]=point;
            else {released=point;has_release=true;}
        }
        if(!f.count && has_release && n==1){f.final_position=true;f.points[0]=released;}
        if(f.count==2 && f.points[0].id>f.points[1].id) {
            bot_touch_point_t p=f.points[0];f.points[0]=f.points[1];f.points[1]=p;
        }
    } else if(data[4]&0x80)f.cancelled=true; /* palm/cover is not a tap */
    if(f.cancelled){errors++;f.count=0;}
    if(f.count==2)dual_reads++;
    debug=(bot_touch_input_debug_t){.reads=reads,.dual=dual_reads,.errors=errors,
        .overflows=overflows,.count=f.count,.header=data[0],.wire_count=data[5],
        .record0=data[0],.record1=data[7]};
    if(releasing && f.time_ms-release_frame.time_ms>=RELEASE_SETTLE_MS) {
        enqueue(release_frame);releasing=false;
    }
    if(!f.count && !f.cancelled) {
        if(!releasing){release_frame=f;releasing=true;}
    } else {releasing=false;enqueue(f);}
    if(f.time_ms-last_log>=5000) {
        last_log=f.time_ms;
        ESP_LOGI("bot_touch","frames=%lu dual=%lu errors=%lu overflow=%lu count=%u ids=%u,%u xy=%d,%d/%d,%d",
            (unsigned long)reads,(unsigned long)dual_reads,(unsigned long)errors,(unsigned long)overflows,
            f.count,f.points[0].id,f.points[1].id,f.points[0].x,f.points[0].y,f.points[1].x,f.points[1].y);
    }
    return err;
}
bool bot_touch_input_start(lv_indev_t *indev)
{
    head=tail=0;releasing=false;reads=dual_reads=errors=overflows=last_log=0;
    esp_lv_adapter_touch_callbacks_t callbacks={.custom_touch_read=read_frame};
    bool ok=indev && esp_lv_adapter_set_touch_callbacks(indev,&callbacks)==ESP_OK;
    if(ok)ESP_LOGI("bot_touch","CST9217 two-point frame reader attached; UI owns all clicks");
    return ok;
}
bool bot_touch_input_pop(bot_touch_frame_t *f)
{
    if(releasing && lv_tick_get()-release_frame.time_ms>=RELEASE_SETTLE_MS) {
        enqueue(release_frame);releasing=false;
    }
    if(head==tail)return false;
    *f=queue[tail];tail=(tail+1)%QUEUE_SIZE;return true;
}

void bot_touch_input_debug(bot_touch_input_debug_t *out){*out=debug;}
