#include "bot_link.h"
#include <stdio.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "driver/usb_serial_jtag.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
static StreamBufferHandle_t rx;
static bool installed;
static volatile bool dropped;
static volatile uint32_t rx_dropped,rx_chunks,rx_bytes;
static uint32_t last_poll_ms,poll_gap_max_ms;
static uint32_t bad_lines,queue_restarts,timeout_restarts,tx_failed;
typedef struct { uint16_t n; uint8_t bytes[256]; } chunk_t;
static void receive(void *unused) {
    (void)unused;chunk_t c;
    for(;;) {
        int n=usb_serial_jtag_read_bytes(c.bytes,sizeof(c.bytes),pdMS_TO_TICKS(50));
        if(n>0) {rx_chunks++;rx_bytes+=(uint32_t)n;if(xStreamBufferSend(rx,c.bytes,n,pdMS_TO_TICKS(50))!=(size_t)n){dropped=true;rx_dropped++;}}
    }
}
#endif
static bot_frame_parser_t parser;
static bj_node_t nodes[640];
static char strings[8193];
#ifdef ESP_PLATFORM
static bot_msg_t incoming;
#endif
static char boot[33],handshake[33];
static uint32_t tx_seq,last_hello,last_rx,action_since,counter;
static bool mismatch;
static uint32_t usage_since;
static const char *agents[]={"codex","workbuddy","cursor","hermes"};
static void identifier(char out[33]) {
#ifdef ESP_PLATFORM
    snprintf(out,33,"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
#else
    snprintf(out,33,"%032lx",(unsigned long)++counter);
#endif
}
static bool send_line(const char *s) {
#ifdef ESP_PLATFORM
    if(!installed)return false;
    size_t n=strlen(s);
    /* Single UI TX owner, bounded nonblocking write. Retry via fresh handshake
       after partial write; the newline ensures peer resynchronization. */
    int sent=usb_serial_jtag_write_bytes(s,n,0);
    if(sent!=(int)n) {tx_failed++;usb_serial_jtag_write_bytes("\n",1,0);return false;}
#else
    (void)s;
#endif
    return true;
}
void bot_link_trace(const char *line) { if(line && strlen(line)<1024)send_line(line); }
void bot_link_start(void) {
    bot_frame_init(&parser,nodes,640,strings,sizeof(strings));
    identifier(boot);identifier(handshake);last_hello=0;last_rx=0;counter=0;
#ifdef ESP_PLATFORM
    usb_serial_jtag_driver_config_t cfg={.rx_buffer_size=4096,.tx_buffer_size=4096};
    if(usb_serial_jtag_driver_install(&cfg)!=ESP_OK)return;
    rx=xStreamBufferCreate(8192,1);if(!rx)return;
    installed=xTaskCreate(receive,"bot_usb_rx",4096,NULL,4,NULL)==pdPASS;
#endif
}
static void restart(bot_model_t *m,uint32_t now) {
    bot_model_begin_handshake(m);identifier(handshake);tx_seq=0;
    last_hello=now-2000;last_rx=now;
}
bool bot_link_version_mismatch(void){return mismatch;}
bool bot_link_poll(bot_model_t *m,uint32_t now) {
    bool changed=false;
    if(m->state==BOT_MS_LINK_DOWN){restart(m,now);changed=true;}
#ifdef ESP_PLATFORM
    if(last_poll_ms && now-last_poll_ms>poll_gap_max_ms)poll_gap_max_ms=now-last_poll_ms;
    last_poll_ms=now;
    if(dropped){queue_restarts++;dropped=false;bot_frame_init(&parser,nodes,640,strings,sizeof(strings));parser.state=BOT_FR_SKIP;restart(m,now);changed=true;}
    chunk_t c;
    for(int budget=0;budget<8 && rx;budget++) {
        c.n=(uint16_t)xStreamBufferReceive(rx,c.bytes,sizeof(c.bytes),0);if(!c.n)break;
        for(unsigned i=0;i<c.n;i++) {
            bot_frame_result_t r=bot_frame_feed(&parser,c.bytes[i],&incoming);
            if(r==BOT_FRAME_BAD_LINE)bad_lines++;
            if(r==BOT_FRAME_BAD_LINE && parser.last_json_err==BJ_OK) {
                double version=0;int n=bj_obj_get(&parser.doc,0,"v");
                if(n>=0 && parser.doc.nodes[n].type==BJ_NUM) version=parser.doc.nodes[n].num;
                if(version!=0 && version!=3)mismatch=true;
            }
            if(r!=BOT_FRAME_MSG)continue;
            /* Demo packets cannot become production facts. */
            if(incoming.type==BOT_MSG_WELCOME && incoming.body.welcome.demo)continue;
            uint32_t before=m->rx_seq;
            bot_apply_result_t applied=bot_model_apply(m,&incoming);
            if(applied!=BOT_APPLY_IGNORED) {last_rx=now;changed=true;mismatch=false;}
            if(incoming.type==BOT_MSG_PING && m->state==BOT_MS_ONLINE && m->rx_seq!=before) {
                char line[240];snprintf(line,sizeof(line),"@bot {\"v\":3,\"type\":\"pong\",\"link_id\":\"%s\",\"seq\":%lu,\"body\":{\"monotonic_ms\":%llu}}\n",m->link_id,(unsigned long)++tx_seq,(unsigned long long)incoming.body.ping_monotonic_ms);send_line(line);
            }
        }
    }
#endif
    if(m->state==BOT_MS_ONLINE && (now-last_rx>=6000 || (m->action_pending && now-action_since>=6000))) {
#ifdef ESP_PLATFORM
        timeout_restarts++;
#endif
        restart(m,now);changed=true;}
    if(m->usage_pending && now-usage_since>=6000){m->usage_pending=false;m->usage_rejected=true;changed=true;}
    if(m->state==BOT_MS_HANDSHAKING && now-last_hello>=2000) {
        char line[512];last_hello=now;
        snprintf(line,sizeof(line),"@bot {\"v\":3,\"type\":\"hello\",\"link_id\":null,\"seq\":0,\"body\":{\"device_id\":\"agent-robot-face\",\"boot_id\":\"%s\",\"firmware\":\"3.2.7\",\"display\":{\"width\":466,\"height\":466},\"min_version\":3,\"max_version\":3,\"handshake_id\":\"%s\"}}\n",boot,handshake);
        send_line(line);
    }
    return changed;
}
bool bot_link_select(bot_model_t *m,bot_selection_mode_t mode,bot_agent_id_t agent,uint32_t now) {
    if(m->state!=BOT_MS_ONLINE || m->action_pending || agent>=BOT_AGENT_COUNT)return false;
    char id[33],line[512];identifier(id);
    snprintf(line,sizeof(line),"@bot {\"v\":3,\"type\":\"action\",\"link_id\":\"%s\",\"seq\":%lu,\"body\":{\"action_id\":\"%s\",\"kind\":\"select\",\"mode\":\"%s\",\"agent_id\":\"%s\",\"expected_selection_rev\":%lu}}\n",m->link_id,(unsigned long)++tx_seq,id,mode==BOT_SELECTION_AUTO?"auto":"pinned",agents[agent],(unsigned long)m->selection_rev);
    if(!send_line(line))return false;
    bot_model_track_action(m,id);action_since=now;return true;
}

/* Diagnostic enum names are compile-time ASCII allowlists, never source text. */
static const char *display_state_name(bot_state_t state) {
    static const char *names[]={"idle","working","tool","waiting","done","error","cancelled","unknown"};
    return (unsigned)state<sizeof(names)/sizeof(names[0])?names[state]:"unknown";
}
static bool safe_run_hash(const char *run) {
    for(unsigned i=0;i<32;i++)if(!((run[i]>='0' && run[i]<='9') || (run[i]>='a' && run[i]<='f')))return false;
    return run[32]=='\0';
}
void bot_link_diagnostics(uint32_t updates,uint32_t window_ms,uint32_t avg_us,uint32_t max_us,
                          const bot_model_t *model,bot_state_t displayed_state,bot_screen_t screen,uint8_t usage_level) {
    char line[768],projection[320]="",usage[200]="";
    if(model && (unsigned)model->selected_agent<BOT_AGENT_COUNT) {
        char run[35]="null",focus_rev[16]="null";
        if(model->has_focus) {
            snprintf(focus_rev,sizeof(focus_rev),"%lu",(unsigned long)model->focus.selection_rev);
            if(model->focus.has_run_id && safe_run_hash(model->focus.run_id))snprintf(run,sizeof(run),"\"%.32s\"",model->focus.run_id);
        }
        const char *page=screen==BOT_SCR_PICKER?"picker":screen==BOT_SCR_STATS?"stats":"face";
        snprintf(projection,sizeof(projection),",\"projection\":{\"agent_id\":\"%s\",\"mode\":\"%s\",\"selection_rev\":%lu,\"focus_selection_rev\":%s,\"state\":\"%s\",\"run_id\":%s,\"screen\":\"%s\"}",
                 agents[model->selected_agent],model->mode==BOT_SELECTION_AUTO?"auto":"pinned",(unsigned long)model->selection_rev,focus_rev,display_state_name(displayed_state),run,page);
    }
    if(model && model->has_usage && model->usage_view.subject<6 && model->usage_view.period<3) {
        static const char *subjects[]={"current","all","codex","cursor","hermes","workbuddy"},*periods[]={"today","7d","30d"};
        snprintf(usage,sizeof(usage),",\"usage\":{\"usage_rev\":%lu,\"data_rev\":%lu,\"subject\":\"%s\",\"period\":\"%s\",\"page\":%u,\"level\":%u}",(unsigned long)model->usage_view.usage_rev,(unsigned long)model->usage.data_rev,subjects[model->usage_view.subject],periods[model->usage_view.period],model->usage_view.page,usage_level<=2?usage_level:0);
    }
    snprintf(line,sizeof(line),"@diag {\"render_updates\":%lu,\"window_ms\":%lu,\"avg_us\":%lu,\"max_us\":%lu%s%s}\n",(unsigned long)updates,(unsigned long)window_ms,(unsigned long)avg_us,(unsigned long)max_us,projection,usage);send_line(line);
}

bool bot_link_usage_request(bot_model_t *m,uint8_t subject,uint8_t period,uint8_t page,uint32_t now) {
 static const char *subjects[]={"current","all","codex","cursor","hermes","workbuddy"};static const char *periods[]={"today","7d","30d"};
 if(m->state!=BOT_MS_ONLINE || m->usage_pending || subject>5 || period>2 || page>33)return false;
 char id[33],line[512];identifier(id);
 snprintf(line,sizeof(line),"@bot {\"v\":3,\"type\":\"usage_request\",\"link_id\":\"%s\",\"seq\":%lu,\"body\":{\"request_id\":\"%s\",\"expected_usage_rev\":%lu,\"subject\":\"%s\",\"period\":\"%s\",\"page\":%u}}\n",m->link_id,(unsigned long)++tx_seq,id,(unsigned long)m->usage_view.usage_rev,subjects[subject],periods[period],page);
 if(!send_line(line))return false;
 memcpy(m->pending_usage_id,id,33);m->requested_usage_view=(bot_usage_view_t){subject,period,page,m->usage_view.usage_rev};m->usage_pending=true;m->usage_rejected=false;usage_since=now;return true;
}

void bot_link_health_trace(void) {
#ifdef ESP_PLATFORM
 char line[384];snprintf(line,sizeof(line),"@link {\"rx_dropped\":%lu,\"bad_lines\":%lu,\"queue_restarts\":%lu,\"timeout_restarts\":%lu,\"tx_failed\":%lu,\"rx_pending_bytes\":%u,\"rx_chunks\":%lu,\"rx_bytes\":%lu,\"poll_gap_max_ms\":%lu}\n",
 (unsigned long)rx_dropped,(unsigned long)bad_lines,(unsigned long)queue_restarts,(unsigned long)timeout_restarts,(unsigned long)tx_failed,rx?(unsigned)xStreamBufferBytesAvailable(rx):0,(unsigned long)rx_chunks,(unsigned long)rx_bytes,(unsigned long)poll_gap_max_ms);send_line(line);
#endif
}
