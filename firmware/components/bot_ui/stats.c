/* Usage UI: retained widgets and value-level invalidation. A host heartbeat is
 * not a visual change. Only local navigation moves the small content layer. */
#include "bot_ui.h"
#include "bot_link.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "assets/bot_usage_36.inc"
#include "assets/bot_usage_64.inc"
#ifdef CONFIG_BOT_DEV_SIM
void stats_dev_build(lv_obj_t *s);void stats_dev_refresh(void);void stats_dev_press(int,bool);
#endif
extern const lv_image_dsc_t bot_icon_0,bot_icon_1,bot_icon_2,bot_icon_3;
static const lv_image_dsc_t *icons[]={&bot_icon_0,&bot_icon_2,&bot_icon_3,&bot_icon_1};
static uint8_t mono_pixels[4][24*24];
static lv_image_dsc_t mono_icons[4];
static void mono_init(void){for(int k=0;k<4;k++){for(int y=0;y<24;y++)for(int x=0;x<24;x++){unsigned sum=0;for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++){const uint8_t *p=icons[k]->data+(y*4+dy)*192+(x*4+dx)*2;unsigned v=p[0]|p[1]<<8;sum+=(((v>>11)&31)*255/31+((v>>5)&63)*255/63+(v&31)*255/31)/3;}mono_pixels[k][y*24+x]=sum/16;}mono_icons[k]=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A8,.w=24,.h=24,.stride=24},.data_size=576,.data=mono_pixels[k]};}}
static const uint32_t WHITE=0xe8ebef,MUTED=0x8d9199,TRACK=0x29292c;
static const char *subjects[]={"当前 Agent","全部工具","Codex","Cursor","Hermes","WorkBuddy"};
static const char *metrics[]={"额度","Token","费用","缓存"};
static const char *period_names[]={"今日","7天","30天"};
static lv_obj_t *root,*content,*title,*subtitle,*status,*scope,*range,*range_text,*category,*category_text,*back,*info;
static lv_obj_t *mode_pill,*mode_divider,*direct_tabs[3],*active_dot;
static int grid_value_kind[4]={-1,-1,-1,-1};
static lv_obj_t *modes[2],*marker,*grid[4],*grid_icon[4],*grid_name[4],*grid_value[4],*grid_note[4],*grid_arc[4],*dividers[2];
static lv_obj_t *rows[3],*row_name[3],*row_value[3],*row_note[3],*row_arc[3],*row_center[3];
static lv_obj_t *detail,*detail_arc,*detail_value,*detail_note,*detail_reset,*metadata[3],*page_label;
static lv_obj_t *rings[3],*ring_labels[3];
static bool concentric;
static uint8_t depth,dimension,metric,selected_row;
static bool info_open,pending_enter,options_open,diagnostics_open;
static lv_obj_t *options,*options_entry,*options_state,*options_diagnostics,*option_modes[3],*option_sensitivity[3];
static void options_refresh(void);
bool stats_options_open(void){return options_open;}
static int slide_direction;
static uint32_t pending_rev;
static int layout_key=-1;
static bot_usage_t presented;
static bool has_presented;
static lv_obj_t *pressed_object;
/* Cheap diagnostics: calls, text changes, gauge changes, structural layouts. */
static uint32_t counters[4];
void stats_debug_counts(uint32_t out[4]){memcpy(out,counters,sizeof(counters));}
static void visible(lv_obj_t *o,bool yes){bool hidden=lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN);if(hidden==!yes)return;if(yes)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h){lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;}
static void feedback_style(lv_obj_t *o){lv_obj_set_style_border_width(o,1,0);lv_obj_set_style_border_color(o,lv_color_hex(0xa3adb9),0);lv_obj_set_style_border_opa(o,0,0);lv_obj_set_style_radius(o,12,0);}
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,int h,bool big){lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_text_font(o,big?&bot_font_24:&bot_font_22,0);lv_obj_set_style_text_color(o,lv_color_hex(big?WHITE:MUTED),0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;}
static void text(lv_obj_t *o,const char *s){if(strcmp(lv_label_get_text(o),s)){lv_label_set_text(o,s);counters[1]++;}}
static lv_obj_t *chip(lv_obj_t *p,int x,int y,int w,const char *s,lv_obj_t **caption){lv_obj_t *o=box(p,x,y,w,38);feedback_style(o);*caption=label(o,0,3,w,32,false);text(*caption,s);return o;}
static lv_obj_t *arc(lv_obj_t *p,int x,int y,int size,int width,bool full){lv_obj_t *o=lv_arc_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,size,size);lv_obj_set_style_arc_width(o,width,LV_PART_MAIN);lv_obj_set_style_arc_width(o,width,LV_PART_INDICATOR);lv_obj_set_style_arc_color(o,lv_color_hex(TRACK),LV_PART_MAIN);lv_obj_set_style_arc_opa(o,255,LV_PART_MAIN);lv_obj_set_style_arc_opa(o,0,LV_PART_INDICATOR);lv_obj_set_style_arc_rounded(o,true,LV_PART_MAIN);lv_obj_set_style_arc_rounded(o,true,LV_PART_INDICATOR);lv_arc_set_rotation(o,full?270:135);lv_arc_set_bg_angles(o,0,full?360:270);lv_arc_set_range(o,0,10000);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);return o;}
static uint32_t hue(double p){return p<25?0x2563eb:p<50?0x22c55e:p<75?0xeab308:p<90?0xf97316:0xef4444;}
typedef struct {lv_obj_t *object;int value;bool known;} gauge_cache_t;
static gauge_cache_t gauge_cache[11];
static void gauge(lv_obj_t *o,bot_usage_number_t n){int value=n.has?(int)lround(n.value*100):0;gauge_cache_t *c=NULL;for(unsigned i=0;i<11;i++)if(gauge_cache[i].object==o||!gauge_cache[i].object){c=&gauge_cache[i];break;}if(c&&c->object==o&&c->value==value&&c->known==n.has)return;if(c)*c=(gauge_cache_t){o,value,n.has};lv_obj_set_style_arc_color(o,lv_color_hex(n.has?hue(n.value):TRACK),LV_PART_INDICATOR);lv_obj_set_style_arc_opa(o,n.has&&value>0?255:0,LV_PART_INDICATOR);lv_arc_set_value(o,value);counters[2]++;}
static void number(char *b,size_t n,bot_usage_number_t v){if(!v.has)snprintf(b,n,"—");else if(v.value>=1e9)snprintf(b,n,"%.2fB",v.value/1e9);else if(v.value>=1e6)snprintf(b,n,"%.2fM",v.value/1e6);else if(v.value>=10000)snprintf(b,n,"%.0fK",v.value/1000);else if(v.value>=1000)snprintf(b,n,"%.1fK",v.value/1000);else snprintf(b,n,"%.0f",v.value);}
static void percent(char *b,size_t n,bot_usage_number_t v){if(!v.has)snprintf(b,n,"—");else snprintf(b,n,fabs(v.value-round(v.value))<0.001?"%.0f%%":"%.1f%%",v.value);}
static const char *quota_label(const char *s){if(!strcmp(s,"Spark Session"))return "Spark 会话";if(!strcmp(s,"Spark Weekly"))return "Spark 周期";if(!strcmp(s,"Session")||!strcmp(s,"session"))return "会话额度";if(!strcmp(s,"plan"))return "套餐额度";if(!strcmp(s,"on_demand"))return "按量额度";if(!strcmp(s,"team_pool"))return "团队额度";if(strstr(s,"5h")||strstr(s,"5 hour")||strstr(s,"5-hour")||strstr(s,"five_hour"))return "5小时额度";if(strstr(s,"week")||strstr(s,"Week"))return "周期额度";if(strstr(s,"month")||strstr(s,"Month"))return "月度额度";if(strstr(s,"credit")||strstr(s,"Credit"))return "积分额度";return s[0]?s:"额度";}
static const char *coverage(uint8_t c){return c==0?"完整覆盖":c==1?"部分覆盖":"覆盖未知";}
static const char *quota_state(const bot_usage_quota_t *q){return q->stale?"数据已过期":q->availability==2?"需要登录":q->availability==3?"来源读取错误":q->availability!=0?"暂无来源":"额度为已用比例";}
static bot_usage_number_t quota_pct(const bot_usage_quota_t *q){bot_usage_number_t n=q->used_pct;if(q->availability!=0||q->stale)n.has=false;return n;}
static void money(char *b,size_t n,const bot_usage_t *u){if(!u->cost_micros.has||!u->cost_currency[0])snprintf(b,n,"—");else snprintf(b,n,"%.4f %s",u->cost_micros.value/1e6,u->cost_currency);}
static void reset_text(char *b,size_t n,const bot_usage_t *u,const bot_usage_quota_t *q){if(q->stale){snprintf(b,n,"数据已过期");return;}if(!q->reset_ms.has||!u->host_now_ms){snprintf(b,n,"重置时间未知");return;}double remain=q->reset_ms.value-u->host_now_ms;unsigned mins=remain>0?(unsigned)ceil(remain/60000):0;if(!mins)snprintf(b,n,"等待重置");else if(mins>=1440)snprintf(b,n,"%u天后重置",(mins+1439)/1440);else if(mins>=60)snprintf(b,n,"%u小时%u分后重置",mins/60,mins%60);else snprintf(b,n,"%u分后重置",mins);}
static void slide_x(void *object,int32_t x){lv_obj_set_x(object,x);}
static void slide(void){if(!content||!slide_direction)return;lv_anim_delete(content,slide_x);lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,content);lv_anim_set_values(&a,slide_direction*18,0);lv_anim_set_duration(&a,140);lv_anim_set_exec_cb(&a,slide_x);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_start(&a);slide_direction=0;}
static bool request(uint8_t subject,uint8_t period,uint8_t page){bot_model_t *m=&g_ui.model;if(m->usage_pending)return false;if(subject==m->usage_view.subject&&period==m->usage_view.period&&page==m->usage_view.page)return true;return bot_link_usage_request(m,subject,period,page,lv_tick_get());}
uint8_t stats_depth(void){return depth;}
void stats_reset(void){depth=0;info_open=false;options_open=false;selected_row=0;pending_enter=false;slide_direction=0;layout_key=-1;}
static int row_count(const bot_usage_t *u){return metric==0?u->quota_count:metric==1?u->model_count:metric==2?1:2;}
static void layout(void){int key=depth+4*dimension+8*metric+32*info_open+64*concentric;if(layout_key==key)return;layout_key=key;counters[3]++;lv_obj_invalidate(root);
 for(int i=0;i<4;i++){visible(grid[i],depth==0);visible(grid_icon[i],!dimension);lv_obj_set_x(grid_name[i],0);lv_obj_set_y(grid_name[i],dimension?12:27);lv_obj_set_width(grid_name[i],172);}
 for(int i=0;i<2;i++){visible(dividers[i],depth==0);visible(modes[i],depth==0);lv_obj_set_style_text_color(modes[i],lv_color_hex(i==dimension?WHITE:MUTED),0);}
 visible(mode_pill,depth==0);visible(marker,false);
 for(int i=0;i<3;i++){visible(direct_tabs[i],depth>0);lv_obj_set_style_text_color(direct_tabs[i],lv_color_hex(metric==i?WHITE:MUTED),0);}
 visible(active_dot,depth>0&&metric<3);if(depth>0&&metric<3)lv_obj_set_x(active_dot,161+metric*78);
 for(int i=0;i<3;i++){visible(rows[i],depth==1&&!concentric&&metric!=2);visible(rings[i],depth==1&&concentric);visible(ring_labels[i],depth==1&&concentric);visible(row_arc[i],metric==0);visible(row_center[i],metric==0);lv_obj_set_pos(row_name[i],metric==0?84:8,metric==0?7:5);lv_obj_set_size(row_name[i],metric==0?177:330,30);lv_obj_set_style_text_align(row_name[i],LV_TEXT_ALIGN_LEFT,0);lv_obj_set_pos(row_value[i],metric==0?266:8,metric==0?7:40);lv_obj_set_size(row_value[i],metric==0?80:330,32);lv_obj_set_style_text_align(row_value[i],metric==0?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);visible(row_note[i],metric==0);}
 visible(options_entry,depth==0);visible(scope,depth==0&&dimension);visible(range,depth==1&&metric!=0);visible(category,false);visible(back,depth>0);visible(info,depth==2||(depth==1&&metric==2));visible(detail,depth==2||(depth==1&&metric==2));visible(page_label,depth==1&&metric!=2);visible(subtitle,depth==2);
 lv_obj_set_pos(range,174,88);
 visible(detail_arc,depth==2&&metric==0&&!info_open);visible(detail_value,!info_open);visible(detail_note,!info_open&&metric<3);visible(detail_reset,!info_open&&metric==0);
 for(int i=0;i<3;i++)visible(metadata[i],info_open);
 lv_obj_set_style_text_font(detail_value,&bot_usage_64,0);
 lv_obj_set_y(detail_value,metric==0?62:74);
}
void stats_refresh(void){
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){stats_dev_refresh();return;}
#endif
 if(!root)return;
 options_refresh();
 counters[0]++;
 bot_model_t *m=&g_ui.model;
 /* Retain the confirmed content while a request/ACK snapshot is in flight. */
 if(m->has_usage){presented=m->usage;has_presented=true;}else if(m->state!=BOT_MS_ONLINE){has_presented=false;}
 const bot_usage_t *u=&presented;const bot_usage_view_t *view=has_presented?&u->view:&m->usage_view;
 if(pending_enter&&!m->usage_pending){if(!m->usage_rejected&&m->usage_view.usage_rev>=pending_rev&&m->has_usage){depth=1;info_open=false;slide_direction=1;pending_enter=false;}else if(m->usage_rejected||m->state!=BOT_MS_ONLINE)pending_enter=false;}
 concentric=depth==1&&metric==0&&view->subject>=2&&u->quota_count>1;
 layout();char b[128],c[128];
 text(title,depth?subjects[view->subject]:"用量总览");
 text(status,m->usage_pending||pending_enter?"…":m->usage_rejected?"重试":m->state!=BOT_MS_ONLINE?"离线":"");
 if(depth==0&&dimension){snprintf(b,sizeof(b),"范围 · %s  ›",subjects[view->subject]);text(scope,b);}
 if(depth==1){snprintf(b,sizeof(b),"%s  ›",period_names[view->period]);text(range_text,b);}
 if(depth==1){snprintf(b,sizeof(b),"%s  ›",metrics[metric]);text(category_text,b);}
 if(depth==0){
 for(int i=0;i<4;i++){bot_usage_number_t n={0},p={0};char annotation[32];const char *note="",*name;
 if(!dimension){static const char *names[]={"Codex","Cursor","Hermes","WorkBuddy"};name=names[i];if(has_presented){n=u->agents[i].total;p=u->agents[i].used_pct;}if(p.has)note="额度已用";else if(n.has){snprintf(annotation,sizeof(annotation),"%s Token",period_names[view->period]);note=annotation;}else note="暂无来源";}
 else {name=i==0?"总 Token":i==1?"实际费用":i==2?"额度":"缓存读取";if(has_presented){n=i==0?u->total:i==1?u->cost_micros:i==3?u->cache_read:(bot_usage_number_t){0};if(i==2&&u->quota_count)p=quota_pct(&u->quotas[0]);}if(i==0)note="范围合计";else if(i==1)note=u->cost_coverage==1?"部分覆盖":"来源实际费用";else if(i==2)note="额度已用";else note="按来源定义";if(!n.has&&!p.has)note="暂无来源";}
 text(grid_name[i],name);visible(grid_arc[i],p.has);
 int kind=p.has?2:(dimension&&i==1)?0:1;if(grid_value_kind[i]!=kind){grid_value_kind[i]=kind;lv_obj_set_style_text_font(grid_value[i],kind==2?&lv_font_montserrat_20:kind==1?&bot_usage_36:&bot_font_24,0);lv_obj_set_y(grid_value[i],kind==1?65:73);lv_obj_set_height(grid_value[i],kind==1?57:34);}
 if(p.has){gauge(grid_arc[i],p);}
 if(p.has)snprintf(b,sizeof(b),"%.0f%%",p.value);else if(dimension&&i==1&&has_presented){if(u->cost_micros.has){snprintf(b,sizeof(b),"%.2f",u->cost_micros.value/1e6);note=u->cost_currency;}else snprintf(b,sizeof(b),"—");}else if(n.has&&n.value>=10000&&n.value<1e6)snprintf(b,sizeof(b),"%.0fK",n.value/1000);else number(b,sizeof(b),n);text(grid_value[i],b);text(grid_note[i],note);
 }
 }else if(depth==1&&metric!=2){int count=has_presented?row_count(u):0;
 for(int i=0;i<3;i++){visible(rows[i],!concentric&&metric!=2&&(i<count||(i==0&&!count)));visible(rings[i],concentric&&i<count);visible(ring_labels[i],concentric&&i<count);if(i>=count){if(!i){text(row_name[i],metric==0?"暂无额度来源":metric==1?"暂无模型数据":"暂无数据");text(row_value[i],"—");text(row_note[i],"");visible(row_arc[i],false);visible(row_center[i],false);}continue;}
 if(metric==0){const bot_usage_quota_t *q=&u->quotas[i];bot_usage_number_t p=quota_pct(q);visible(row_arc[i],true);gauge(row_arc[i],p);text(row_name[i],bot_ui_agent_name(q->agent_id));percent(b,sizeof(b),p);text(row_value[i],b);text(row_center[i],b);text(row_note[i],q->stale||q->availability?quota_state(q):quota_label(q->label));if(concentric){gauge(rings[i],p);char window[64];snprintf(window,sizeof(window),"%s",quota_label(q->label));char *suffix=strstr(window,"额度");if(suffix)*suffix=0;snprintf(c,sizeof(c),"%.60s  %.24s",window,b);text(ring_labels[i],c);}}
 else {text(row_name[i],metric==1?u->models[i].label:metric==2?"实际费用":i==0?"缓存读取":"缓存写入");if(metric==2)money(b,sizeof(b),u);else number(b,sizeof(b),metric==1?u->models[i].total:i==0?u->cache_read:u->cache_write);text(row_value[i],b);}
 }
 unsigned total=metric==0?u->quota_total:metric==1?u->model_total:0;unsigned pages=(total+2)/3;if(pages>1)snprintf(b,sizeof(b),"%u / %u",view->page+1,pages);else b[0]=0;text(page_label,b);
 if(metric==2){if(u->cost_micros.has)snprintf(b,sizeof(b),"%.2f",u->cost_micros.value/1e6);else snprintf(b,sizeof(b),"—");lv_obj_set_style_text_font(detail_value,strlen(b)>8?&bot_usage_36:&bot_usage_64,0);text(detail_value,b);text(detail_note,u->cost_currency);}
 }else{
 const bot_usage_quota_t *q=selected_row<u->quota_count?&u->quotas[selected_row]:NULL;
 text(subtitle,info_open?"详情":metric==0&&q?quota_label(q->label):metric==1&&selected_row<u->model_count?u->models[selected_row].label:metric==3?(selected_row?"缓存写入":"缓存读取"):metrics[metric]);
 if(info_open){
 if(metric==0){text(metadata[0],q?quota_state(q):"暂无额度来源");if(q)reset_text(b,sizeof(b),u,q);else b[0]=0;text(metadata[1],b);text(metadata[2],q?bot_ui_agent_name(q->agent_id):"");}
 else if(metric==1){number(b,sizeof(b),u->input);snprintf(c,sizeof(c),"范围输入 %s",b);text(metadata[0],c);number(b,sizeof(b),u->output);snprintf(c,sizeof(c),"范围输出 %s",b);text(metadata[1],c);text(metadata[2],coverage(u->coverage));}
 else if(metric==2){text(metadata[0],"来源实际费用");text(metadata[1],coverage(u->cost_coverage));text(metadata[2],!strcmp(u->cost_source,"multiple_native_sources")?"多个原生来源":u->cost_source[0]?u->cost_source:"暂无来源");}
 else {text(metadata[0],"按各来源原始定义");text(metadata[1],coverage(u->coverage));text(metadata[2],"未提供的数据保持空缺");}
 }else if(!has_presented){text(detail_value,"—");text(detail_note,"");text(detail_reset,"");gauge(detail_arc,(bot_usage_number_t){0});}
 else if(metric==0){bot_usage_number_t p=q?quota_pct(q):(bot_usage_number_t){0};gauge(detail_arc,p);percent(b,sizeof(b),p);text(detail_value,b);text(detail_note,"已用");if(q)reset_text(b,sizeof(b),u,q);else snprintf(b,sizeof(b),"暂无额度来源");text(detail_reset,b);}
 else if(metric==1){number(b,sizeof(b),selected_row<u->model_count?u->models[selected_row].total:u->total);text(detail_value,b);text(detail_note,"Token");}
 else if(metric==2){if(u->cost_micros.has)snprintf(b,sizeof(b),"%.2f",u->cost_micros.value/1e6);else snprintf(b,sizeof(b),"—");lv_obj_set_style_text_font(detail_value,strlen(b)>8?&bot_usage_36:&bot_usage_64,0);text(detail_value,b);text(detail_note,u->cost_currency);}
 else {number(b,sizeof(b),selected_row?u->cache_write:u->cache_read);text(detail_value,b);}
 }
 slide();
}
static const char *audio_state_name(bot_audio_service_state_t state){
 static const char *names[]={"已关闭","启动中","校准中","运行","停止中","故障"};
 return (unsigned)state<6?names[state]:"故障";
}
static void options_refresh(void){
 if(!options)return;
 visible(options,options_open);
 if(!options_open)return;
 const bot_audio_config_t *c=bot_ui_audio_config();const bot_audio_view_t *v=bot_ui_audio_view();char b[240];
 for(int i=0;i<3;i++){lv_obj_set_style_text_color(option_modes[i],lv_color_hex(i==(int)c->mode?WHITE:MUTED),0);lv_obj_set_style_border_opa(option_modes[i],i==(int)c->mode?190:0,0);lv_obj_set_style_text_color(option_sensitivity[i],lv_color_hex(i==(int)c->sensitivity?WHITE:MUTED),0);lv_obj_set_style_border_opa(option_sensitivity[i],i==(int)c->sensitivity?190:0,0);}
 snprintf(b,sizeof(b),"%s%s",audio_state_name(v->service_state),v->supported&&!v->verified?" · 待硬件验收":!v->supported?" · 未启用":"");text(options_state,b);
 visible(options_diagnostics,diagnostics_open);
 if(diagnostics_open){
  if(v->available)snprintf(b,sizeof(b),"dBFS %.1f / floor %.1f\nMic %u | age %lu ms | Q %04x\n%s | error %ld",(double)v->rms_dbfs,(double)v->noise_floor_dbfs,v->active_mics,(unsigned long)(lv_tick_get()-v->sampled_ms),v->quality_flags,bot_ui_audio_suppression(),(long)v->last_error);
  else snprintf(b,sizeof(b),"dBFS N/A / floor N/A\nMic N/A | age N/A\n%s | error %ld",v->service_state==BOT_AUDIO_FAULT?"fault":v->service_state==BOT_AUDIO_DISABLED?"disabled":"no valid sample",(long)v->last_error);
  text(options_diagnostics,b);
 }
}
static void options_build(void){
 options_entry=label(root,324,58,68,32,false);text(options_entry,"设置");feedback_style(options_entry);
 options=box(root,52,52,362,362);lv_obj_set_style_bg_color(options,lv_color_hex(0x121214),0);lv_obj_set_style_bg_opa(options,255,0);lv_obj_set_style_radius(options,24,0);
 lv_obj_t *o=label(options,28,14,255,32,true);text(o,"设备选项");o=label(options,302,14,36,32,false);text(o,"×");
 o=label(options,20,56,322,28,false);text(o,"声响应");
 static const char *mode_names[]={"关","自然","节奏"};static const char *sensitivity_names[]={"低","中","高"};
 for(int i=0;i<3;i++){option_modes[i]=label(options,25+i*105,89,102,38,true);text(option_modes[i],mode_names[i]);feedback_style(option_modes[i]);option_sensitivity[i]=label(options,25+i*105,160,102,38,false);text(option_sensitivity[i],sensitivity_names[i]);feedback_style(option_sensitivity[i]);}
 o=label(options,20,128,322,28,false);text(o,"灵敏度");options_state=label(options,12,207,338,30,false);
 o=label(options,80,245,202,28,false);text(o,"诊断  ›");
 options_diagnostics=label(options,12,281,338,76,false);lv_obj_set_style_text_font(options_diagnostics,&lv_font_montserrat_20,0);lv_label_set_long_mode(options_diagnostics,LV_LABEL_LONG_WRAP);
 options_refresh();
}
static void deleted(lv_event_t *e){if(lv_event_get_target_obj(e)==root){root=NULL;options=NULL;options_open=false;layout_key=-1;pressed_object=NULL;memset(gauge_cache,0,sizeof(gauge_cache));}}
void stats_build(lv_obj_t *s){
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){stats_dev_build(s);return;}
#endif
 mono_init();
 root=box(s,0,0,466,466);lv_obj_set_style_bg_color(root,lv_color_black(),0);lv_obj_set_style_bg_opa(root,255,0);lv_obj_add_event_cb(root,deleted,LV_EVENT_DELETE,NULL);
 content=box(root,0,108,466,292);
 title=label(root,92,57,282,34,true);subtitle=label(root,71,91,324,31,false);status=label(root,332,91,55,30,false);
 scope=label(root,86,91,294,32,false);feedback_style(scope);
 for(int i=0;i<4;i++){grid[i]=box(content,57+i%2*180,108+i/2*146,172,144);feedback_style(grid[i]);grid_icon[i]=lv_image_create(grid[i]);lv_image_set_src(grid_icon[i],&mono_icons[i]);lv_obj_set_style_image_recolor(grid_icon[i],lv_color_hex(0xe8ebef),0);lv_obj_set_style_image_recolor_opa(grid_icon[i],255,0);lv_image_set_pivot(grid_icon[i],0,0);lv_obj_set_pos(grid_icon[i],74,0);lv_obj_remove_flag(grid_icon[i],LV_OBJ_FLAG_CLICKABLE);grid_name[i]=label(grid[i],0,27,172,29,false);lv_obj_set_style_text_color(grid_name[i],lv_color_hex(WHITE),0);grid_arc[i]=arc(grid[i],57,58,58,5,true);grid_value[i]=label(grid[i],0,69,172,34,true);grid_note[i]=label(grid[i],0,117,172,25,false);}
 dividers[0]=box(content,233,110,1,284);dividers[1]=box(content,57,251,352,1);for(int i=0;i<2;i++){lv_obj_set_style_bg_color(dividers[i],lv_color_hex(0x29292c),0);lv_obj_set_style_bg_opa(dividers[i],255,0);}
 for(int i=0;i<3;i++){rows[i]=box(content,59,124+i*79,348,77);feedback_style(rows[i]);row_arc[i]=arc(rows[i],5,3,64,6,true);row_center[i]=label(rows[i],7,23,60,29,false);row_name[i]=label(rows[i],80,10,180,30,false);lv_obj_set_style_text_color(row_name[i],lv_color_hex(WHITE),0);row_value[i]=label(rows[i],266,10,80,32,true);row_note[i]=label(rows[i],84,39,254,28,false);lv_obj_set_style_text_align(row_note[i],LV_TEXT_ALIGN_LEFT,0);lv_obj_t *sep=box(rows[i],5,73,338,1);lv_obj_set_style_bg_color(sep,lv_color_hex(TRACK),0);lv_obj_set_style_bg_opa(sep,255,0);}
 for(int i=0;i<3;i++){rings[i]=arc(content,115+i*14,128+i*14,236-i*28,7,true);}
 for(int i=0;i<3;i++){ring_labels[i]=label(content,148,197+i*34,170,32,false);feedback_style(ring_labels[i]);}
 detail=box(content,33,124,400,264);detail_arc=arc(detail,70,2,260,14,false);detail_value=label(detail,30,83,340,76,true);detail_note=label(detail,40,145,320,30,false);detail_reset=label(detail,20,230,360,30,false);
 for(int i=0;i<3;i++)metadata[i]=label(detail,20,44+i*53,360,36,i==0);
 mode_pill=box(root,137,402,192,36);lv_obj_set_style_bg_color(mode_pill,lv_color_hex(0x242426),0);lv_obj_set_style_bg_opa(mode_pill,255,0);lv_obj_set_style_radius(mode_pill,18,0);
 mode_divider=box(mode_pill,95,8,1,20);lv_obj_set_style_bg_color(mode_divider,lv_color_hex(0x77777b),0);lv_obj_set_style_bg_opa(mode_divider,255,0);
 for(int i=0;i<2;i++){modes[i]=label(mode_pill,i*96,2,96,32,false);text(modes[i],i?"维度":"工具");feedback_style(modes[i]);}
 marker=box(root,174,428,20,2);lv_obj_set_style_bg_color(marker,lv_color_hex(WHITE),0);lv_obj_set_style_bg_opa(marker,255,0);
 for(int i=0;i<3;i++){direct_tabs[i]=label(root,130+i*78,393,74,34,false);text(direct_tabs[i],metrics[i]);feedback_style(direct_tabs[i]);}
 active_dot=box(root,161,431,12,3);lv_obj_set_style_bg_color(active_dot,lv_color_hex(WHITE),0);lv_obj_set_style_bg_opa(active_dot,255,0);lv_obj_set_style_radius(active_dot,4,0);
 range=chip(root,174,88,119,"",&range_text);category=chip(root,126,392,115,"",&category_text);
 back=label(root,75,56,44,34,true);text(back,"<");lv_obj_set_style_text_font(back,&lv_font_montserrat_24,0);feedback_style(back);info=label(root,333,56,45,34,false);text(info,"i");feedback_style(info);
 page_label=label(root,191,361,84,28,false);for(uint32_t i=0;i<lv_obj_get_child_count(content);i++){lv_obj_t *child=lv_obj_get_child(content,i);lv_obj_set_y(child,lv_obj_get_style_y(child,0)-108);}layout_key=-1;options_build();stats_refresh();
}
void stats_press(int hit,bool pressed){
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){stats_dev_press(hit,pressed);return;}
#endif
 if(pressed_object){lv_obj_set_style_border_opa(pressed_object,0,0);pressed_object=NULL;}
 if(!pressed||hit<0)return;
 lv_obj_t *o=hit<4?(depth==0?grid[hit]:depth==1&&hit<3?(concentric?ring_labels[hit]:rows[hit]):NULL):hit==20?range:hit==30?scope:hit==31?back:hit==32?info:hit>=10&&hit<=13?(depth==0&&hit<12?modes[hit-10]:depth>0&&hit<13?direct_tabs[hit-10]:NULL):NULL;
 if(o){lv_obj_set_style_border_opa(o,190,0);pressed_object=o;}
}
int stats_hit(int x,int y){
 if(options_open){
  if(x>=346&&x<402&&y>=58&&y<106)return 41;
  if(x>=77&&x<392&&y>=141&&y<179)return 50+(x-77)/105;
  if(x>=77&&x<392&&y>=212&&y<250)return 60+(x-77)/105;
  if(x>=132&&x<334&&y>=290&&y<329)return 42;
  return -1;
 }
 if(depth==0&&x>=324&&x<399&&y>=54&&y<94)return 40;
 if(depth>0&&y>=54&&y<96){if(x>=75&&x<121)return 31;if((depth==2||(depth==1&&metric==2))&&x>=333&&x<378)return 32;}
 if(depth==1&&metric!=0&&y>=88&&y<126&&x>=174&&x<293)return 20;
 if(depth>0&&y>=390&&y<432&&x>=123&&x<357)return 10+(x-123)/78;
 if(!depth&&y>=400&&y<440&&x>=137&&x<329)return 10+(x-137)/96;
 if(!depth&&dimension&&y>=89&&y<124&&x>=86&&x<380)return 30;
 if(!depth&&y>=108&&y<398&&x>=57&&x<409)return (y-108)/146*2+(x-57)/180;
 if(depth==1&&concentric&&x>=115&&x<351&&y>=128&&y<364){int dx=x-233,dy=y-246;double r=sqrt(dx*dx+dy*dy);if(r>=83){int hit=(int)((118-r)/14);return hit<0?0:hit>2?2:hit;}int hit=(y-197)/34;return hit<0?0:hit>2?2:hit;}
 if(depth==1&&y>=124&&y<361&&x>=59&&x<407)return (y-124)/79;
 return -1;
}
void stats_tap(int hit){bot_model_t *m=&g_ui.model;if(hit<0)return;
 if(hit==40){options_open=true;options_refresh();return;}
 if(options_open){bot_audio_config_t c=*bot_ui_audio_config();
  if(hit==41)options_open=false;
  else if(hit==42)diagnostics_open=!diagnostics_open;
  else if(hit>=50&&hit<=52){c.mode=(bot_audio_mode_t)(hit-50);bot_ui_audio_request(c);}
  else if(hit>=60&&hit<=62){c.sensitivity=(bot_audio_sensitivity_t)(hit-60);bot_ui_audio_request(c);}
  options_refresh();return;
 }
 if(hit==31){pending_enter=false;if(info_open)info_open=false;else if(depth)depth--;slide_direction=-1;}
 else if(hit==32&&(depth==2||(depth==1&&metric==2))){info_open=!info_open;slide_direction=info_open?1:-1;}
 else if(hit==20&&depth>0){if(request(m->usage_view.subject,(m->usage_view.period+1)%3,0))selected_row=0;}
 else if(hit==30&&!depth&&dimension){request(m->usage_view.subject==1?0:1,m->usage_view.period,0);}
 else if(hit>=10&&hit<=13){if(!depth){pending_enter=false;if(dimension!=hit-10){dimension=hit-10;slide_direction=1;}}else if(depth>0&&hit<13){metric=hit-10;depth=1;info_open=false;selected_row=0;slide_direction=1;request(m->usage_view.subject,m->usage_view.period,0);}}
 else if(hit<4&&!depth){if(dimension){pending_enter=false;metric=hit==0?1:hit==1?2:hit==2?0:3;depth=1;selected_row=0;slide_direction=1;request(m->usage_view.subject,m->usage_view.period,0);}else if(!m->usage_pending){metric=0;if(request(hit+2,m->usage_view.period,0)){if(m->usage_pending){pending_enter=true;pending_rev=m->usage_view.usage_rev+1;}else {depth=1;slide_direction=1;}}}}
 else if(depth==1&&metric!=2&&hit<row_count(&presented)&&has_presented){selected_row=hit;depth=2;info_open=false;slide_direction=1;}
 stats_refresh();
}
void stats_swipe(bot_gesture_kind_t ev){bot_model_t *m=&g_ui.model;
 if(options_open)return;
 if(ev==BOT_GESTURE_SWIPE_DOWN){pending_enter=false;if(info_open)info_open=false;else if(depth)depth--;slide_direction=-1;stats_refresh();return;}
 if(ev!=BOT_GESTURE_SWIPE_LEFT&&ev!=BOT_GESTURE_SWIPE_RIGHT)return;
 if(!depth){pending_enter=false;dimension=!dimension;slide_direction=ev==BOT_GESTURE_SWIPE_LEFT?1:-1;stats_refresh();return;}
 int direction=ev==BOT_GESTURE_SWIPE_LEFT?1:-1;
 unsigned total=metric==0?presented.quota_total:metric==1?presented.model_total:0;
 int page=m->usage_view.page+direction;
 if(depth==1&&total>3&&page>=0&&page<34&&(unsigned)page*3<total){request(m->usage_view.subject,m->usage_view.period,page);}
 else {metric=((metric<3?metric:1)+direction+3)%3;depth=1;info_open=false;selected_row=0;request(m->usage_view.subject,m->usage_view.period,0);}
 slide_direction=direction;stats_refresh();
}
