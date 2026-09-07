/* Build-only hardware integration replay. Never enabled in the shipping build.
 * Exercises real LVGL/display DMA with production pages and real host data;
 * browsing does not alter Agent business focus. */
#ifdef BOT_DEVICE_SOAK
extern void stats_tap(int hit);
extern void stats_reset(void);
static void device_soak_tick(uint32_t now)
{
    static int previous=-1;
    if(now<10000)return;
    int step=(int)((now-10000)/1000);
    if(step==previous||step>96)return;
    previous=step;
    if(step==96){bot_ui_show_face();return;}
    switch(step%12){
    case 0:stats_reset();bot_ui_show_stats();break;
    case 2:stats_tap(1);break;
    case 4:stats_tap(0);break;
    case 6:stats_tap(11);break;
    case 8:stats_tap(12);break;
    case 10:bot_ui_show_face();break;
    default:break;
    }
}
#endif
