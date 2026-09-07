#include "bot_ui.h"
#include "bot_ui_motion.h"
#include <stdio.h>
#ifdef CONFIG_BOT_DEV_SIM
void picker_dev_build(lv_obj_t *s);void picker_dev_refresh(void);
#endif
extern const lv_image_dsc_t bot_icon_0,bot_icon_1,bot_icon_2,bot_icon_3;
static const lv_image_dsc_t *icons[]={&bot_icon_0,&bot_icon_1,&bot_icon_2,&bot_icon_3};
static lv_obj_t *cards[3],*name,*health,*hint,*mode,*preview;
static lv_obj_t *label(lv_obj_t *s,const char *t,int y,int size) {
 lv_obj_t *l=lv_label_create(s);lv_label_set_text(l,t);lv_obj_set_style_text_font(l,size==24?&bot_font_24:&bot_font_22,0);
 lv_obj_set_style_text_color(l,lv_color_hex(0xddeaf2),0);lv_obj_set_size(l,326,32);lv_obj_set_pos(l,70,y);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);return l;
}
void picker_refresh(void) {
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){picker_dev_refresh();return;}
#endif
 if(!preview)return;
 for(unsigned j=0;j<3;j++) {
  unsigned i=(g_ui.picker_preview+4+j)%5;lv_obj_clean(cards[j]);
  if(i<4){lv_obj_t *im=lv_image_create(cards[j]);lv_image_set_src(im,icons[i]);if(j!=1)lv_image_set_scale(im,128);lv_obj_center(im);}
  else {lv_obj_t *l=lv_label_create(cards[j]);lv_label_set_text(l,LV_SYMBOL_SHUFFLE);lv_obj_set_style_text_font(l,&lv_font_montserrat_24,0);lv_obj_set_style_text_color(l,lv_color_white(),0);lv_obj_center(l);}
 }
 char text[160];snprintf(text,sizeof(text),"%s · %s",g_ui.model.mode==BOT_SELECTION_AUTO?"自动关注":"锁定关注",bot_ui_agent_name(g_ui.model.selected_agent));lv_label_set_text(mode,text);
 lv_label_set_text(name,bot_ui_agent_name(g_ui.picker_preview));
 lv_label_set_text(health,g_ui.picker_preview==4?"优先关注等待与错误":bot_ui_health(g_ui.picker_preview));
 if(g_ui.picker_preview<4 && g_ui.model.state==BOT_MS_ONLINE && g_ui.model.has_catalog) {
  for(unsigned i=0;i<g_ui.model.catalog.count;i++) {
   const bot_catalog_agent_t *a=&g_ui.model.catalog.agents[i];
   if(a->id==g_ui.picker_preview && (a->health==BOT_HEALTH_READY || a->health==BOT_HEALTH_PARTIAL) && a->cap_state==BOT_CAP_OBSERVED){snprintf(text,sizeof(text),"%s\n已观测活跃任务 %u",bot_ui_health(g_ui.picker_preview),a->active_sessions);lv_label_set_text(health,text);}
  }
 }
 lv_label_set_text(hint,g_ui.picker_selecting?"正在等待确认":g_ui.model.action_rejected?"切换失败 · 请重试":g_ui.model.state!=BOT_MS_ONLINE?"连接后可切换":"轻点选择");
}
void picker_build(lv_obj_t *s) {
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){picker_dev_build(s);return;}
#endif
 label(s,"关注谁", 62,24);mode=label(s,"",102,22);
 preview=lv_obj_create(s);lv_obj_remove_style_all(preview);lv_obj_set_size(preview,466,466);
 lv_obj_remove_flag(preview,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
 for(unsigned j=0;j<3;j++) {
  cards[j]=lv_obj_create(preview);lv_obj_remove_style_all(cards[j]);lv_obj_set_size(cards[j],j==1?144:64,j==1?144:64);
  lv_obj_set_pos(cards[j],j==0?70:j==1?161:332,j==1?154:190);
  lv_obj_set_style_bg_color(cards[j],lv_color_hex(BOT_OS_PRESS_COLOR),0);lv_obj_set_style_bg_opa(cards[j],0,0);
  lv_obj_set_style_border_color(cards[j],lv_color_hex(j==1?0x9aa6b3:0x30353b),0);lv_obj_set_style_border_width(cards[j],1,0);lv_obj_set_style_radius(cards[j],22,0);
 }
 name=label(preview,"",300,24);health=label(preview,"",334,22);lv_obj_set_height(health,44);hint=label(s,"",380,22);lv_obj_set_height(hint,24);picker_refresh();
}

void picker_animate(int direction) {
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){extern void picker_dev_animate(int);picker_dev_animate(direction);return;}
#endif
 bot_ui_preview_slide(preview,direction);
}
void picker_press(bool pressed) {
#ifdef CONFIG_BOT_DEV_SIM
 if(g_ui.dev_sim){extern void picker_dev_press(bool);picker_dev_press(pressed);return;}
#endif
 bot_ui_press_feedback(cards[1],pressed);
}
