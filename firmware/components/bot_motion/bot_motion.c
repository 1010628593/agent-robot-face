/* Gravity prediction in sensor coordinates; display mapping is explicit.
 * This layer owns no bus, GUI object, allocation, permission or account state. */
#include "bot_motion.h"
#include <math.h>
#include <string.h>
#define RAD 0.017453292519943295f
#define DEG 57.29577951308232f
#define SHAKE_HIGH .60f
#define SHAKE_LOW .18f
#define SHAKE_WINDOW_MS 850u
static float norm(const float v[3]) { return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }
static float dot(const float a[3],const float b[3]) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void unit(float v[3]) { float n=norm(v);if(n>.00001f)for(int i=0;i<3;i++)v[i]/=n; }
float bot_motion_wrap(float x) {
    if(!isfinite(x))return 0;
    x=fmodf(x+180,360);if(x<0)x+=360;return x-180;
}
void bot_motion_init(bot_motion_t *m,float mount_deg,int sign) {
    if(!m)return;
    memset(m,0,sizeof(*m));m->mount_deg=bot_motion_wrap(mount_deg);
    m->rotation_sign=sign<0?-1:1;
}
static void predict(float g[3],const float w[3],float dt) {
    float speed=norm(w);if(speed<.00001f)return;
    float k[3]={w[0]/speed,w[1]/speed,w[2]/speed};
    float cross[3]={k[1]*g[2]-k[2]*g[1],k[2]*g[0]-k[0]*g[2],k[0]*g[1]-k[1]*g[0]};
    float c=cosf(speed*RAD*dt),s=sinf(speed*RAD*dt),d=dot(k,g);
    for(int i=0;i<3;i++)g[i]=g[i]*c-cross[i]*s+k[i]*d*(1-c);
    unit(g);
}
static void event(bot_motion_t *m,bot_reaction_t r,uint32_t now) {
    m->view.event_id++;m->view.event_ms=now;m->view.reaction=r;
}
static bool valid(const bot_motion_sample_t *s) {
    for(int i=0;i<3;i++)if(!isfinite(s->accel[i]) || !isfinite(s->gyro[i]) ||
        fabsf(s->accel[i])>=3.98f || fabsf(s->gyro[i])>=1020)return false;
    return norm(s->accel)>.05f;
}
bool bot_motion_feed(bot_motion_t *m,const bot_motion_sample_t *s) {
    if(!m || !s || !valid(s))return false;
    uint32_t elapsed=s->ms-m->last_ms;
    bool fresh=!m->initialized;
    if(!fresh && (elapsed==0 || elapsed>=UINT32_C(0x80000000)))return false;
    float an=norm(s->accel),a[3];for(int i=0;i<3;i++)a[i]=s->accel[i]/an;
    bool gap=!fresh && elapsed>120;
    if(fresh) {
        if(fabsf(an-1)>.25f)return false;
        memcpy(m->gravity,a,sizeof(a));memcpy(m->previous_accel,s->accel,sizeof(a));
        m->initialized=true;m->origin_ms=s->ms;m->stable_ms=s->ms;m->quiet_ms=s->ms;
        m->view.flat=hypotf(a[0],a[1])<.22f;
    }
    float dt=fresh?.01f:(float)elapsed/1000;
    float w[3],da[3];
    for(int i=0;i<3;i++) { w[i]=s->gyro[i]-m->bias[i];da[i]=s->accel[i]-m->previous_accel[i]; }
    float wn=norm(w);
    if(gap) {
        /* Do not integrate across a stall, carry shake peaks or learn a false bias. */
        m->peaks=0;m->peak_high=false;m->shake_armed=false;m->stable=false;m->quiet=false;m->moving=false;
        m->calibration_count=0;m->view.reaction=BOT_REACTION_NONE;
        if(fabsf(an-1)<.12f)memcpy(m->gravity,a,sizeof(a));
        dt=.01f;
    } else if(!fresh)predict(m->gravity,w,dt);
    float linear[3];for(int i=0;i<3;i++)linear[i]=s->accel[i]-m->gravity[i];
    float energy=norm(linear);
    bool still=fabsf(an-1)<.08f && wn<2.0f && norm(da)<.025f;
    if(still) {
        if(!m->stable){m->stable=true;m->stable_ms=s->ms;m->calibration_count=0;}
        if(!m->bias_ready) {
            if(!m->calibration_count) {
                memset(m->calibration_sum,0,sizeof(m->calibration_sum));
                memcpy(m->calibration_accel,a,sizeof(a));m->calibration_ms=s->ms;
            }
            float change[3];for(int i=0;i<3;i++)change[i]=a[i]-m->calibration_accel[i];
            if(norm(change)>.015f) {m->calibration_count=0;m->stable_ms=s->ms;}
            else {
                for(int i=0;i<3;i++)m->calibration_sum[i]+=s->gyro[i];
                m->calibration_count++;
                if(s->ms-m->calibration_ms>=1000 && m->calibration_count>=50) {
                    for(int i=0;i<3;i++)m->bias[i]=m->calibration_sum[i]/m->calibration_count;
                    m->bias_ready=true;
                }
            }
        }
    } else {m->stable=false;m->calibration_count=0;}
    bool settled=still && s->ms-m->stable_ms>=300;
    if(fabsf(an-1)<.12f && (energy<.32f || settled)) {
        if(settled && dot(m->gravity,a)<-.5f)memcpy(m->gravity,a,sizeof(a));
        else {
            float gain=dt/(.18f+dt);
            for(int i=0;i<3;i++)m->gravity[i]+=gain*(a[i]-m->gravity[i]);
            unit(m->gravity);
        }
    }
    float planar=hypotf(m->gravity[0],m->gravity[1]);
    if(m->view.flat) {if(planar>.35f)m->view.flat=false;}
    else if(planar<.22f)m->view.flat=true;
    m->view.orientation_valid=!m->view.flat;
    if(!m->view.flat) {
        float raw=atan2f(m->gravity[0],-m->gravity[1])*DEG;
        float target=bot_motion_wrap(m->rotation_sign*(raw-m->mount_deg));
        if(fresh)m->view.rotation_deg=target;
        else {
            float error=bot_motion_wrap(target-m->view.rotation_deg);
            if(fabsf(error)>.4f) {
                float delta=error*(dt/(.06f+dt)),limit=240*dt;
                delta=fmaxf(-limit,fminf(limit,delta));
                m->view.rotation_deg=bot_motion_wrap(m->view.rotation_deg+delta);
            }
        }
    }
    bool quiet=energy<.14f && wn<12 && fabsf(an-1)<.1f;
    bool cooling=m->had_dizzy && s->ms-m->last_dizzy_ms<BOT_MOTION_COOLDOWN_MS;
    if(quiet) {
        if(!m->quiet){m->quiet=true;m->quiet_ms=s->ms;}
        if(!cooling && s->ms-m->quiet_ms>=500)m->shake_armed=true;
        if(m->moving && s->ms-m->quiet_ms>350) {
            if(!cooling)event(m,BOT_REACTION_SETTLE,s->ms);
            m->moving=false;
        }
    } else {
        if(m->quiet && s->ms-m->quiet_ms>1500 && !cooling) {
            m->moving=true;m->moved_ms=s->ms;event(m,BOT_REACTION_ATTENTION,s->ms);
        }
        m->quiet=false;
    }
    if(s->ms-m->window_ms>SHAKE_WINDOW_MS){m->peaks=0;}
    if(energy<SHAKE_LOW)m->peak_high=false;
    if(m->shake_armed && !cooling && s->ms-m->origin_ms>1000 && energy>SHAKE_HIGH && wn<350 &&
       !m->peak_high && s->ms-m->peak_ms>=70) {
        m->peak_high=true;m->peak_ms=s->ms;
        float dir[3];for(int i=0;i<3;i++)dir[i]=linear[i]/energy;
        if(!m->peaks || dot(dir,m->peak_dir)>-.3f) {m->peaks=1;m->window_ms=s->ms;}
        else m->peaks++;
        memcpy(m->peak_dir,dir,sizeof(dir));
        if(m->peaks>=3) {
            event(m,BOT_REACTION_DIZZY,s->ms);m->last_dizzy_ms=s->ms;m->had_dizzy=true;
            m->peaks=0;m->moving=false;m->shake_armed=false;
        }
    }
    m->last_ms=s->ms;memcpy(m->previous_accel,s->accel,sizeof(m->previous_accel));
    m->view.sampled_ms=s->ms;m->view.available=true;m->view.gyro_calibrated=m->bias_ready;
    m->view.linear_g=energy;return true;
}
bot_motion_view_t bot_motion_view(const bot_motion_t *m,uint32_t now) {
    bot_motion_view_t v={0};if(!m || !m->initialized)return v;
    v=m->view;
    if(now-v.sampled_ms>BOT_MOTION_STALE_MS) {
        v.available=false;v.orientation_valid=false;v.reaction=BOT_REACTION_NONE;return v;
    }
    uint32_t duration=v.reaction==BOT_REACTION_DIZZY?BOT_MOTION_DIZZY_MS:
        (v.reaction==BOT_REACTION_ATTENTION?600u:450u);
    if(now-v.event_ms>=duration)v.reaction=BOT_REACTION_NONE;
    return v;
}
void bot_motion_unrotate(float angle,int16_t x,int16_t y,int16_t *ox,int16_t *oy) {
    float a=bot_motion_wrap(angle)*RAD,c=cosf(a),s=sinf(a);
    float dx=x-233,dy=y-233;
    if(ox)*ox=(int16_t)lroundf(fmaxf(-233,fminf(699,233+c*dx+s*dy)));
    if(oy)*oy=(int16_t)lroundf(fmaxf(-233,fminf(699,233-s*dx+c*dy)));
}
