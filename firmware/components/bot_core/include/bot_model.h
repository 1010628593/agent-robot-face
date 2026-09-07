/* bot_model.h — single-threaded predictable device model (T05).
 *
 * Contract authority: docs/05_PROTOCOL.md §2/§3/§6/§7.
 *   - Only frames of the CURRENT link with a GREATER seq may mutate the model.
 *     Old link / duplicate seq / wrong rev never partially update anything.
 *   - welcome is accepted only while handshaking; it installs the new link,
 *     clears pending actions and old stats (§3.4).
 *   - focus/stats with older selection_rev are ignored; newer rev is buffered
 *     (one slot) and applied only after ack/welcome confirms the selection.
 */
#ifndef BOT_MODEL_H
#define BOT_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "bot_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOT_MS_LINK_DOWN = 0,
    BOT_MS_HANDSHAKING,
    BOT_MS_ONLINE
} bot_model_state_t;

typedef struct {
    bot_model_state_t state;

    /* current link (valid when state == BOT_MS_ONLINE) */
    char link_id[33];
    uint32_t rx_seq;
    uint64_t last_ping_ms;
    bool has_ping;

    /* authoritative selection (from welcome/ack only) */
    bot_selection_mode_t mode;
    bot_agent_id_t selected_agent;
    uint32_t selection_rev;

    /* latest-wins mailboxes (§9) */
    bool has_catalog;
    bot_catalog_t catalog;
    bool has_focus;
    bot_focus_t focus;
    bool has_usage,usage_pending,usage_rejected;
    bot_usage_t usage; bot_usage_view_t usage_view,requested_usage_view; char pending_usage_id[33];
    bool has_stats;
    bot_stats_t stats;
    bool has_notice;
    bot_notice_t notice;

    /* one-slot buffer for focus/stats carrying a not-yet-confirmed rev */
    bool pending_focus;
    bot_focus_t pending_focus_msg;
    bool pending_stats;
    bot_stats_t pending_stats_msg;

    /* in-flight user action awaiting its ack */
    bool action_pending;
    bool action_rejected;
    char pending_action_id[33];
} bot_model_t;

typedef enum {
    BOT_APPLY_IGNORED = 0, /* old link / old seq / stale rev / wrong state */
    BOT_APPLY_APPLIED,     /* model mutated */
    BOT_APPLY_BUFFERED     /* newer rev: buffered pending ack/welcome */
} bot_apply_result_t;

void bot_model_init(bot_model_t *m);

/* Enter handshaking (new boot / link timeout / re-enumeration, §3.5). */
void bot_model_begin_handshake(bot_model_t *m);

/* Apply one fully validated message. Never partially mutates on rejection. */
bot_apply_result_t bot_model_apply(bot_model_t *m, const bot_msg_t *msg);

/* Register an outgoing user action (device -> host). */
void bot_model_track_action(bot_model_t *m, const char action_id[33]);

const char *bot_model_state_name(bot_model_state_t s);

#ifdef __cplusplus
}
#endif

#endif /* BOT_MODEL_H */
