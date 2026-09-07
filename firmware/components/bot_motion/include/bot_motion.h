#ifndef BOT_MOTION_H
#define BOT_MOTION_H
#include <stdbool.h>
#include <stdint.h>
#define BOT_MOTION_STALE_MS 250u
#define BOT_MOTION_DIZZY_MS 1300u
#define BOT_MOTION_COOLDOWN_MS 3000u

typedef struct { uint32_t ms; float accel[3], gyro[3]; } bot_motion_sample_t;
typedef enum { BOT_REACTION_NONE, BOT_REACTION_ATTENTION, BOT_REACTION_SETTLE, BOT_REACTION_DIZZY } bot_reaction_t;
typedef struct {
    uint32_t sampled_ms, event_ms, event_id;
    float rotation_deg, linear_g, reaction_strength;
    float screen_accel[3]; /* gravity removed, mounted display XY and normal Z, in g */
    bot_reaction_t reaction;
    bool available, orientation_valid, flat, gyro_calibrated;
} bot_motion_view_t;
typedef struct {
    float gravity[3], bias[3], calibration_sum[3], calibration_accel[3], previous_accel[3];
    float peak_dir[3], stable_accel[3], peak_energy, mount_deg, rotation_sign;
    uint32_t last_ms, origin_ms, stable_ms, calibration_ms, calibration_count;
    uint32_t peak_ms, window_ms, quiet_ms, moved_ms, last_dizzy_ms;
    unsigned peaks;
    bool initialized, bias_ready, stable, quiet, moving, peak_high, had_dizzy, shake_armed;
    bot_motion_view_t view;
} bot_motion_t;
/* Sensor right-handed XYZ; accel in g, gyro in degrees/s. Angle mapping is
 * explicit: mount_deg is the sensor gravity angle for a physically upright face.
 * rotation_sign is +1 or -1 according to which side of the PCB the IMU faces. */
void bot_motion_init(bot_motion_t *m,float mount_deg,int rotation_sign);
bool bot_motion_feed(bot_motion_t *m,const bot_motion_sample_t *sample);
bot_motion_view_t bot_motion_view(const bot_motion_t *m,uint32_t now);
float bot_motion_wrap(float degrees);
void bot_motion_unrotate(float angle_deg,int16_t x,int16_t y,int16_t *out_x,int16_t *out_y);
#endif
