/* bot_types.h — shared C types for Bot Status firmware (T03).
 *
 * Single header consumed by bot_core components (gesture T04, frame decoder
 * T05) and by tests/native (host-compiled, no ESP-IDF dependency).
 * All sizes/bounds mirror contracts/ + design/interaction_tokens.json:
 *   - display 466x466
 *   - gesture tokens: hold_ms=650, swipe_min_px=56 (design/interaction_tokens.json)
 *   - frame budget 8192 bytes
 */
#ifndef BOT_TYPES_H
#define BOT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOT_DISPLAY_W 466
#define BOT_DISPLAY_H 466
#define BOT_FRAME_MAX_BYTES 8192
#define BOT_HOLD_MS 650
#define BOT_SWIPE_MIN_PX 56
#define BOT_LONG_PRESS_CANCEL_PX 13

#define BOT_AGENT_ID_MAX 16
#define BOT_RUN_ID_MAX 64
#define BOT_TOOL_NAME_MAX 24

typedef enum {
    BOT_AGENT_CODEX = 0,
    BOT_AGENT_WORKBUDDY = 1,
    BOT_AGENT_CURSOR = 2,
    BOT_AGENT_HERMES = 3,
    BOT_AGENT_COUNT = 4
} bot_agent_id_t;

typedef enum {
    BOT_STATE_IDLE = 0,
    BOT_STATE_WORKING,
    BOT_STATE_TOOL,
    BOT_STATE_WAITING,
    BOT_STATE_DONE,
    BOT_STATE_ERROR,
    BOT_STATE_CANCELLED,
    BOT_STATE_UNKNOWN
} bot_state_t;

typedef enum {
    BOT_QUALITY_OBSERVED = 0,
    BOT_QUALITY_INFERRED,
    BOT_QUALITY_REPORTED,
    BOT_QUALITY_MANUAL,
    BOT_QUALITY_SIMULATED
} bot_quality_t;

typedef enum {
    BOT_WAIT_NONE = 0,
    BOT_WAIT_APPROVAL,
    BOT_WAIT_INPUT
} bot_wait_reason_t;

/* Raw touch sample from CST9217 (IRQ mode), x/y already in display coords. */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint32_t t_ms;
    bool pressed;
} bot_touch_sample_t;

typedef enum {
    BOT_GESTURE_NONE = 0,
    BOT_GESTURE_TAP,
    BOT_GESTURE_HOLD,
    BOT_GESTURE_SWIPE_LEFT,
    BOT_GESTURE_SWIPE_RIGHT,
    BOT_GESTURE_SWIPE_UP,
    BOT_GESTURE_SWIPE_DOWN,
    BOT_GESTURE_WAKE_ONLY
} bot_gesture_kind_t;

/* Single-contact gesture event: at most one per contact (T04 contract). */
typedef struct {
    bot_gesture_kind_t kind;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t end_x;
    uint16_t end_y;
    uint32_t start_ms;
    uint32_t end_ms;
} bot_gesture_event_t;

#ifdef __cplusplus
}
#endif

#endif /* BOT_TYPES_H */
