/* test_gesture.c — table-driven acceptance test for the gesture FSM (T04).
 *
 * The table is GENERATED from Bot_Status_v1_Handoff/acceptance/gesture_cases.json
 * via tools/gen_gesture_cases.py (gesture_cases.inc). Do not hand-edit cases here.
 *
 * Contract boundaries under test (docs/10_IMPLEMENTATION_PLAN.md T04):
 *   G02 649ms is NOT hold; G03 650ms IS hold (fired by TICK, not by UP)
 *   G04 13px movement cancels hold; G03-after-hold UP produces no TAP
 *   G10 wake contact produces exactly WAKE_ONLY; G11 cancel produces nothing
 *   G05–G08 four swipe directions; G09 diagonal rejected by axis ratio;
 *   G12 55px below swipe_min_px rejected.
 */
#include <stdint.h>
#include <stdio.h>

#include "bot_gesture.h"

typedef struct {
    bot_touch_phase_t phase;
    uint32_t t_ms;
    int16_t x;
    int16_t y;
} gesture_sample_t;

typedef struct {
    const char *id;
    const gesture_sample_t *samples;
    uint32_t sample_count;
    const bot_gesture_kind_t *expected;
    uint32_t expected_count;
    bool wake_only;
} gesture_case_t;

#include "gesture_cases.inc"

static int failures = 0;

static void run_case(const gesture_case_t *c)
{
    bot_gesture_t g;
    bot_gesture_init(&g);
    if (c->wake_only) {
        bot_gesture_arm_wake(&g);
    }

    bot_gesture_kind_t got[8];
    uint32_t got_count = 0;

    for (uint32_t i = 0; i < c->sample_count; i++) {
        const gesture_sample_t *s = &c->samples[i];
        bot_gesture_kind_t ev = bot_gesture_feed(&g, s->phase, s->t_ms, s->x, s->y);
        if (ev != BOT_GESTURE_NONE) {
            if (got_count < sizeof(got) / sizeof(got[0])) {
                got[got_count++] = ev;
            } else {
                printf("FAIL %s: more than %zu events (event overflow)\n",
                       c->id, sizeof(got) / sizeof(got[0]));
                failures++;
                return;
            }
        }
    }

    if (got_count != c->expected_count) {
        printf("FAIL %s: expected %u event(s), got %u (", c->id,
               c->expected_count, got_count);
        for (uint32_t i = 0; i < got_count; i++) {
            printf("%s%s", i ? "," : "", bot_gesture_event_name(got[i]));
        }
        printf(")\n");
        failures++;
        return;
    }
    for (uint32_t i = 0; i < got_count; i++) {
        if (got[i] != c->expected[i]) {
            printf("FAIL %s: event[%u] expected %s, got %s\n", c->id, i,
                   bot_gesture_event_name(c->expected[i]),
                   bot_gesture_event_name(got[i]));
            failures++;
            return;
        }
    }
    printf("PASS %s\n", c->id);
}

int main(void)
{
    for (uint32_t i = 0; i < GESTURE_CASE_COUNT; i++) {
        run_case(&GESTURE_CASES[i]);
    }
    if (failures) {
        printf("test_gesture: %d FAILURE(S)\n", failures);
        return 1;
    }
    printf("test_gesture: all %u cases passed\n", GESTURE_CASE_COUNT);
    return 0;
}
