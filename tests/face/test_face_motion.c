/* Native tests of the actual firmware motion/geometry, no LVGL or ESP-IDF. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bot_face_motion.h"
#include "bot_face_geometry.h"

static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); assert(x); } } while (0)
static int near(float a, float b) { return fabsf(a-b) < 0.001f; }
static bot_face_pose_t at(bot_face_motion_t *m, uint32_t t) {
    bot_face_pose_t p; bot_face_motion_sample(m, t, &p); return p;
}
static void same_pose(bot_face_pose_t a, bot_face_pose_t b) {
#define SAME(f) CHECK(near(a.f,b.f))
    SAME(cx); SAME(cy); SAME(eye_w); SAME(left_h); SAME(right_h);
    SAME(separation); SAME(lid); SAME(tilt); SAME(smile); SAME(cross);
    SAME(pupil); SAME(gaze_x); SAME(gaze_y); SAME(opacity);
#undef SAME
}
static void mappings(void) {
    CHECK(bot_face_for_state(BOT_STATE_IDLE)==BOT_FACE_IDLE);
    CHECK(bot_face_for_state(BOT_STATE_WORKING)==BOT_FACE_WORKING);
    CHECK(bot_face_for_state(BOT_STATE_TOOL)==BOT_FACE_TOOL);
    CHECK(bot_face_for_state(BOT_STATE_WAITING)==BOT_FACE_WAITING);
    CHECK(bot_face_for_state(BOT_STATE_DONE)==BOT_FACE_DONE);
    CHECK(bot_face_for_state(BOT_STATE_ERROR)==BOT_FACE_ERROR);
    CHECK(bot_face_for_state(BOT_STATE_CANCELLED)==BOT_FACE_SLEEP);
    CHECK(bot_face_for_state(BOT_STATE_UNKNOWN)==BOT_FACE_DISCONNECTED);
    CHECK(bot_face_for_state((bot_state_t)99)==BOT_FACE_DISCONNECTED);
}
static void transition_and_heartbeat(void) {
    bot_face_motion_t m; bot_face_motion_init(&m, 7, 100);
    bot_face_pose_t before=at(&m,1000);
    CHECK(bot_face_motion_set(&m,BOT_FACE_WORKING,1,1000));
    same_pose(before,at(&m,1000));
    bot_face_pose_t middle=at(&m,1110), end=at(&m,1220);
    CHECK(middle.lid>0 && middle.lid<end.lid);
    CHECK(end.tilt>0 && end.smile==0);
    bot_face_motion_t copy=m;
    for (uint32_t t=1220;t<9000;t+=33) {
        CHECK(!bot_face_motion_set(&m,BOT_FACE_WORKING,1,t));
        same_pose(at(&m,t),at(&copy,t));
    }
    before=at(&m,9030);
    CHECK(bot_face_motion_set(&m,BOT_FACE_ERROR,2,9030));
    same_pose(before,at(&m,9030));
    before=at(&m,9120);
    CHECK(bot_face_motion_set(&m,BOT_FACE_WAITING,3,9120));
    same_pose(before,at(&m,9120));
    CHECK(at(&m,9450).cross==0); /* interrupted error never returns */
}
static void terminal_states_do_not_replay(void) {
    bot_face_motion_t m; bot_face_motion_init(&m,7,0);
    bot_face_motion_set(&m,BOT_FACE_DONE,10,100);
    CHECK(at(&m,500).smile>.8f);
    bot_face_pose_t held=at(&m,5000);
    CHECK(held.smile>.7f);
    CHECK(!bot_face_motion_set(&m,BOT_FACE_DONE,10,5000));
    same_pose(held,at(&m,5000));
    CHECK(bot_face_motion_set(&m,BOT_FACE_DONE,11,5000));
    same_pose(held,at(&m,5000)); /* new run replays, without a jump */
    bot_face_motion_set(&m,BOT_FACE_ERROR,12,6000);
    CHECK(at(&m,6500).cross>.95f);
    CHECK(at(&m,60000).cross==1); /* no false recovery to idle */
}
static void blink_is_interpolated_and_centered(void) {
    bot_face_motion_t m; bot_face_motion_init(&m,7,0);
    bot_face_motion_set(&m,BOT_FACE_BLINK,1,0);
    bot_face_pose_t open=at(&m,500), half=at(&m,930), shut=at(&m,985), reopen=at(&m,1090);
    CHECK(half.left_h < open.left_h && half.left_h>shut.left_h);
    CHECK(shut.left_h<=10);
    CHECK(reopen.left_h>shut.left_h && reopen.left_h<open.left_h);
    CHECK(fabsf(half.cy-shut.cy)<.02f); /* only subpixel anti-burn-in drift */
    CHECK(near(shut.left_h,shut.right_h));
}
static void gaze_and_touch(void) {
    bot_face_motion_t m; bot_face_motion_init(&m,7,0);
    bot_face_motion_set(&m,BOT_FACE_LOOK_LEFT,1,0);
    CHECK(at(&m,500).gaze_x<-.5f);
    bot_face_motion_set(&m,BOT_FACE_LOOK_RIGHT,2,1000);
    CHECK(at(&m,1500).gaze_x>.5f);
    bot_face_motion_set(&m,BOT_FACE_WAITING,3,2000);
    bot_face_motion_touch(&m,true,465,0,0,2400);
    bot_face_pose_t p=at(&m,2560);
    CHECK(p.cx>233 && p.cy<233 && p.pupil>.5f);
    CHECK(p.smile==0 && p.cross==0); /* touch cannot imply success */
    bot_face_motion_touch(&m,true,465,0,1,2560);
    CHECK(at(&m,2560).left_h < p.left_h); /* hold squeezes, no HUD ring */
    bot_face_motion_touch(&m,false,465,0,0,2600);
    p=at(&m,3200);
    CHECK(fabsf(p.cx-233)<3);
    bot_face_motion_set(&m,BOT_FACE_ERROR,4,3300);
    bot_face_motion_touch(&m,true,0,465,0,3800);
    CHECK(at(&m,4000).cross==1 && at(&m,4000).smile==0);
}
static void rollover_and_determinism(void) {
    bot_face_motion_t a,b;
    uint32_t origin=UINT32_MAX-1000u;
    bot_face_motion_init(&a,31,origin);bot_face_motion_init(&b,31,0);
    bot_face_motion_set(&a,BOT_FACE_TOOL,2,origin+100u);
    bot_face_motion_set(&b,BOT_FACE_TOOL,2,100);
    for(uint32_t t=100;t<12000;t+=37) same_pose(at(&a,origin+t),at(&b,t));
}
static void all_expressions_and_geometry(void) {
    bot_face_motion_t m;bot_face_motion_init(&m,7,0);
    for(int e=0;e<BOT_FACE_COUNT;e++) {
        bot_face_motion_set(&m,(bot_face_t)e,(uint32_t)e,0);
        for(uint32_t t=0;t<12000;t+=33) {
            bot_face_pose_t p=at(&m,t);
            CHECK(isfinite(p.cx) && isfinite(p.cy));
            CHECK(p.eye_w>=50 && p.eye_w<=90);
            CHECK(p.left_h>=6 && p.left_h<=135);
            CHECK(p.right_h>=6 && p.right_h<=135);
            CHECK(p.opacity>=.15f && p.opacity<=1);
            CHECK(p.smile>=0 && p.smile<=1 && p.cross>=0 && p.cross<=1);
            bot_face_geometry_t g;bot_face_geometry_build(&p,&g);
            CHECK(g.count>0 && g.count<=BOT_FACE_MAX_PRIMITIVES);
            for(unsigned i=0;i<g.count;i++) {
                const bot_face_primitive_t *d=&g.items[i];
                CHECK(d->kind>=BOT_FACE_RECT && d->kind<=BOT_FACE_ARC);
                CHECK(d->x1>=BOT_FACE_AREA_X && d->y1>=BOT_FACE_AREA_Y);
                CHECK(d->x2<=BOT_FACE_AREA_X+BOT_FACE_AREA_W);
                CHECK(d->y2<=BOT_FACE_AREA_Y+BOT_FACE_AREA_H);
            }
        }
    }
}
int main(void) {
    mappings();transition_and_heartbeat();terminal_states_do_not_replay();
    blink_is_interpolated_and_centered();gaze_and_touch();
    rollover_and_determinism();all_expressions_and_geometry();
    printf("face motion: %d assertions passed\n",checks);return 0;
}
