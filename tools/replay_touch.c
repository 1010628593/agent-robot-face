/* Offline diagnostic: replay complete display-coordinate touch frames through
 * the actual firmware recognizer and face renderer. No assertions or fixtures.
 * stdin: time_ms count x0 y0 x1 y1; count -1 cancels, 0 releases.
 * stdout: JSON lines containing emitted event, pose and drawing primitives.
 * Build: cc -std=c99 -Ifirmware/components/bot_core/include \
 *   -Ifirmware/components/bot_ui/include tools/replay_touch.c \
 *   firmware/components/bot_core/gesture.c \
 *   firmware/components/bot_ui/face_motion.c \
 *   firmware/components/bot_ui/face_geometry.c -lm -o /tmp/replay_touch
 * Optional argv[1] selects the actual bot_face_t expression (default IDLE).
 */
#include "bot_face_geometry.h"
#include "bot_gesture.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
int main(int argc,char **argv)
{
    bot_face_motion_t m;bot_gesture_t g;
    bot_gesture_init(&g);bot_gesture_set_face_mode(&g,true);
    bot_face_motion_init(&m,0xB07FACEu,0);
    if(argc>1)bot_face_motion_set(&m,(bot_face_t)atoi(argv[1]),1,0);
    unsigned t;int count,x0,y0,x1,y1;
    while(scanf("%u %d %d %d %d %d",&t,&count,&x0,&y0,&x1,&y1)==6) {
        if(count>2 || count< -1)return 2;
        bot_touch_frame_t f={.time_ms=t,.count=count>0?count:0,.cancelled=count<0,
            .points={{.id=0,.x=x0,.y=y0},{.id=1,.x=x1,.y=y1}}};
        bot_gesture_kind_t ev=bot_gesture_feed_frame(&g,&f);
        if(ev==BOT_GESTURE_TAP && g.pet_contact)bot_face_motion_poke(&m,g.down_x,g.down_y,t);
        if(!g.pet_contact || g.blocked || g.state!=BOT_GS_PRESSED)f.count=0;
        float comfort=g.stroking?fminf(1,(float)(t-g.stroke_ms)/1500):0;
        bot_face_motion_interact(&m,&f,comfort,t-g.down_ms);
        bot_face_pose_t p;bot_face_motion_sample(&m,t,&p);
        bot_face_geometry_t geo;bot_face_geometry_build(&p,&geo);
        printf("{\"t\":%u,\"event\":\"%s\",\"multi\":%s,\"pose\":[%.3f,%.3f,%.3f,%.3f,%.3f],\"gaze\":[%.3f,%.3f],\"tilt\":%.3f,\"roll\":%.4f,\"streak\":%u,\"alternating\":%u,\"cupped\":%s,\"geometry\":[",
            t,bot_gesture_event_name(ev),g.multi_contact?"true":"false",p.cx,p.cy,p.left_h,p.right_h,p.separation,p.gaze_x,p.gaze_y,p.tilt,p.roll,m.tap_streak,m.alternating,m.contact_was_dual?"true":"false");
        for(unsigned i=0;i<geo.count;i++) {
            bot_face_primitive_t *d=&geo.items[i];
            printf("%s[%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d]",i?",":"",
                d->kind,d->x1,d->y1,d->x2,d->y2,d->x3,d->y3,d->radius,d->width,d->dark);
        }
        puts("]}");
    }
    return ferror(stdin)?1:0;
}
