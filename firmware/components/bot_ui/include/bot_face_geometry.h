/* Shared drawing primitives: firmware and preview use the same geometry. */
#ifndef BOT_FACE_GEOMETRY_H
#define BOT_FACE_GEOMETRY_H
#include "bot_face_motion.h"
#define BOT_FACE_AREA_X 72
#define BOT_FACE_AREA_Y 108
#define BOT_FACE_AREA_W 322
#define BOT_FACE_AREA_H 244
#define BOT_FACE_MAX_PRIMITIVES 96
#define BOT_FACE_EYE_COLOR 0xC9DCFF

typedef enum { BOT_FACE_RECT, BOT_FACE_LINE, BOT_FACE_TRIANGLE, BOT_FACE_ARC } bot_face_primitive_kind_t;
typedef struct {
    bot_face_primitive_kind_t kind;
    float x1,y1,x2,y2,x3,y3,radius,width,start_angle,end_angle;
    uint8_t opacity;
    bool dark;
} bot_face_primitive_t;
typedef struct {
    unsigned count;
    bot_face_primitive_t items[BOT_FACE_MAX_PRIMITIVES];
} bot_face_geometry_t;
void bot_face_geometry_build(const bot_face_pose_t *pose,bot_face_geometry_t *out);
#endif
