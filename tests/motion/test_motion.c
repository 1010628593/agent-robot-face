#ifdef NDEBUG
#undef NDEBUG
#endif
#include "bot_motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PI 3.14159265358979323846f
static uint32_t now;
static bot_motion_view_t step(bot_motion_t *m,float x,float y,float z,float gz) {
    now+=10;bot_motion_sample_t s={.ms=now,.accel={x,y,z},.gyro={0,0,gz}};
    assert(bot_motion_feed(m,&s));return bot_motion_view(m,now);
}
static void rest(bot_motion_t *m,unsigned n) { for(unsigned i=0;i<n;i++)step(m,0,-1,0,0); }
static void setup(bot_motion_t *m) { now=0;bot_motion_init(m,0,1);rest(m,180); }
static void test_upright(void) {
    bot_motion_t m;setup(&m);bot_motion_view_t v=bot_motion_view(&m,now);
    assert(v.available && v.orientation_valid && v.gyro_calibrated && fabsf(v.rotation_deg)<.1f);
    assert(v.reaction==BOT_REACTION_NONE);
}
static void test_rotation(void) {
    bot_motion_t m;setup(&m);
    for(int i=1;i<=200;i++) {
        float theta=i*.9f*PI/180;
        bot_motion_view_t v=step(&m,-sinf(theta),-cosf(theta),0,90);
        assert(v.reaction!=BOT_REACTION_DIZZY);
    }
    bot_motion_view_t v={0};for(int i=0;i<200;i++)v=step(&m,0,1,0,0);
    assert(fabsf(bot_motion_wrap(v.rotation_deg-180))<1.5f);
    float prev=v.rotation_deg;
    for(int i=1;i<=20;i++) {
        float theta=(180+i*.5f)*PI/180;v=step(&m,-sinf(theta),-cosf(theta),0,50);
        assert(fabsf(bot_motion_wrap(v.rotation_deg-prev))<5);prev=v.rotation_deg;
    }
}
static void test_flat_and_stale(void) {
    bot_motion_t m;setup(&m);bot_motion_view_t v;
    for(int i=0;i<200;i++)v=step(&m,-1,0,0,0);
    assert(fabsf(v.rotation_deg+90)<2);float saved=v.rotation_deg;
    for(int i=0;i<200;i++)v=step(&m,0,0,1,0);
    assert(v.flat && !v.orientation_valid);
    float angle=v.rotation_deg;
    for(int i=0;i<100;i++)v=step(&m,.002f*sinf((float)i),.002f*cosf((float)i),1,0);
    assert(fabsf(bot_motion_wrap(v.rotation_deg-angle))<.1f);
    assert(fabsf(bot_motion_wrap(angle-saved))<5);
    v=bot_motion_view(&m,now+300);assert(!v.available && v.reaction==BOT_REACTION_NONE);
}
static void pulse(bot_motion_t *m,float x) { for(int i=0;i<8;i++)step(m,x,-1,0,0);rest(m,9); }
static void test_shake(void) {
    bot_motion_t m;setup(&m);
    pulse(&m,.9f);assert(bot_motion_view(&m,now).reaction!=BOT_REACTION_DIZZY);
    pulse(&m,-.9f);pulse(&m,.9f);
    bot_motion_view_t v=bot_motion_view(&m,now);assert(v.reaction==BOT_REACTION_DIZZY);
    uint32_t id=v.event_id;
    for(int i=0;i<8;i++)pulse(&m,i%2?-.9f:.9f);
    assert(bot_motion_view(&m,now).event_id==id);
    rest(&m,800);v=bot_motion_view(&m,now);assert(v.reaction==BOT_REACTION_NONE);
    pulse(&m,.9f);pulse(&m,-.9f);pulse(&m,.9f);
    assert(bot_motion_view(&m,now).reaction==BOT_REACTION_DIZZY);
    assert(bot_motion_view(&m,now).event_id!=id);
}
static void test_continuous_shake_cannot_rearm(void) {
    bot_motion_t m;setup(&m);pulse(&m,.9f);pulse(&m,-.9f);pulse(&m,.9f);
    uint32_t id=bot_motion_view(&m,now).event_id;
    for(int i=0;i<60;i++)pulse(&m,i%2?-.9f:.9f);
    assert(bot_motion_view(&m,now).event_id==id);
}
static void test_bad_samples(void) {
    bot_motion_t m;setup(&m);bot_motion_sample_t s={.ms=now,.accel={0,-1,0}};
    assert(!bot_motion_feed(&m,&s));s.ms=now-10;assert(!bot_motion_feed(&m,&s));
    s.ms=now+10;s.accel[0]=NAN;assert(!bot_motion_feed(&m,&s));
    s.accel[0]=100;assert(!bot_motion_feed(&m,&s));
    s.accel[0]=0;s.gyro[1]=INFINITY;assert(!bot_motion_feed(&m,&s));
    assert(!bot_motion_feed(NULL,&s));assert(!bot_motion_feed(&m,NULL));
}
static void test_bias(void) {
    bot_motion_t m;now=0;bot_motion_init(&m,0,1);
    for(int i=0;i<250;i++)step(&m,0,-1,0,.4f);
    assert(m.bias_ready && fabsf(m.bias[2]-.4f)<.01f);
    bot_motion_init(&m,0,1);now=0;
    for(int i=0;i<200;i++)step(&m,.3f*sinf((float)i),-1,0,30);
    assert(!m.bias_ready);
}
static void test_time_wrap(void) {
    bot_motion_t a,b;bot_motion_init(&a,0,1);bot_motion_init(&b,0,1);
    uint32_t shift=UINT32_MAX-800;
    for(uint32_t t=0;t<6000;t+=10) {
        bot_motion_sample_t s={.ms=t,.accel={.2f,-.98f,0},.gyro={0,0,0}};
        assert(bot_motion_feed(&a,&s));s.ms=t+shift;assert(bot_motion_feed(&b,&s));
        bot_motion_view_t x=bot_motion_view(&a,t),y=bot_motion_view(&b,t+shift);
        assert(fabsf(bot_motion_wrap(x.rotation_deg-y.rotation_deg))<.01f);
    }
}
static void test_mount_and_tilt(void) {
    bot_motion_t m;bot_motion_init(&m,90,-1);
    bot_motion_sample_t s={.ms=1,.accel={1,0,0}};assert(bot_motion_feed(&m,&s));
    assert(fabsf(bot_motion_view(&m,1).rotation_deg)<.1f);
    setup(&m);
    for(int i=1;i<=100;i++) {
        float theta=(float)i*.9f*PI/180;
        now+=10;s=(bot_motion_sample_t){.ms=now,.accel={0,-cosf(theta),sinf(theta)},.gyro={90,0,0}};
        assert(bot_motion_feed(&m,&s));assert(bot_motion_view(&m,now).reaction!=BOT_REACTION_DIZZY);
    }
    assert(bot_motion_view(&m,now).flat);
}
static void test_gap_and_direction(void) {
    bot_motion_t m;setup(&m);
    for(int i=0;i<6;i++)pulse(&m,.9f);
    assert(bot_motion_view(&m,now).reaction!=BOT_REACTION_DIZZY);
    rest(&m,100);pulse(&m,.9f);pulse(&m,-.9f);
    now+=400;rest(&m,1);pulse(&m,.9f);
    assert(bot_motion_view(&m,now).reaction!=BOT_REACTION_DIZZY);
    rest(&m,80);assert(bot_motion_view(&m,now).available);
}
static void test_inverse(void) {
    int16_t x,y;bot_motion_unrotate(90,233,296,&x,&y);assert(abs(x-296)<=1 && abs(y-233)<=1);
    bot_motion_unrotate(-90,233,170,&x,&y);assert(abs(x-296)<=1 && abs(y-233)<=1);
}
int main(void) {
    test_upright();test_rotation();test_flat_and_stale();test_shake();
    test_continuous_shake_cannot_rearm();test_bad_samples();test_bias();test_time_wrap();test_inverse();test_mount_and_tilt();test_gap_and_direction();
    puts("motion: 11 test groups passed");return 0;
}
