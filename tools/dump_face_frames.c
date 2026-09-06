/* Export the actual firmware primitives for an offline animation review. */
#include "bot_face_geometry.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static int argument(const char *s,int lo,int hi) {
    char *end;errno=0;long n=strtol(s,&end,10);
    if(errno || *end || end==s || n<lo || n>hi){fprintf(stderr,"Invalid argument: %s\n",s);exit(2);}
    return (int)n;
}
int main(int argc,char **argv) {
    if(argc!=4){fprintf(stderr,"Usage: dump_face_frames EXPRESSION(0..17) FRAMES(1..300) STEP_MS(1..1000)\n");return 2;}
    int expression=argument(argv[1],0,BOT_FACE_COUNT-1);
    int frames=argument(argv[2],1,300),step=argument(argv[3],1,1000);
    bot_face_motion_t m;bot_face_motion_init(&m,0xB07FACEu,0);
    bot_face_motion_set(&m,(bot_face_t)expression,1,0);
    printf("[");
    for(int f=0;f<frames;f++) {
        bot_face_pose_t p;bot_face_motion_sample(&m,(uint32_t)(f*step),&p);
        bot_face_geometry_t g;bot_face_geometry_build(&p,&g);
        printf("%s[",f?",":"");
        for(unsigned i=0;i<g.count;i++) {
            const bot_face_primitive_t *d=&g.items[i];
            printf("%s[%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%d]",
                i?",":"",(int)d->kind,d->x1,d->y1,d->x2,d->y2,d->x3,d->y3,
                d->radius,d->width,d->start_angle,d->end_angle,(unsigned)d->opacity,d->dark?1:0);
        }
        printf("]");
    }
    puts("]");return 0;
}
