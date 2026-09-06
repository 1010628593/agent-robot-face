/* bot_model.c — see bot_model.h for the contract rules. */
#include "bot_model.h"

#include <string.h>

void bot_model_init(bot_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->state = BOT_MS_LINK_DOWN;
    m->selected_agent = BOT_AGENT_CODEX;
    m->selection_rev = 0;
}

void bot_model_begin_handshake(bot_model_t *m)
{
    /* §3.4/§3.5: entering handshaking invalidates the old link. Pending
     * actions from the previous connection must not resurrect. */
    char agent_keep = 0;
    (void)agent_keep;
    bot_agent_id_t sel = m->selected_agent;
    uint32_t rev = m->selection_rev;
    memset(m, 0, sizeof(*m));
    m->state = BOT_MS_HANDSHAKING;
    m->selected_agent = sel; /* selection is a user preference, kept across links */
    m->selection_rev = rev;
}

static void apply_welcome(bot_model_t *m, const bot_msg_t *msg)
{
    const bot_welcome_t *w = &msg->body.welcome;
    memcpy(m->link_id, msg->link_id, sizeof(m->link_id));
    m->rx_seq = msg->seq;
    m->state = BOT_MS_ONLINE;
    m->has_ping = false;
    m->selected_agent = w->selected_agent;
    m->selection_rev = w->selection_rev;
    /* §3.4: clear per-link transient state and old stats/focus */
    m->has_focus = false;
    m->has_stats = false;
    m->has_notice = false;
    m->pending_focus = false;
    m->pending_stats = false;
    m->action_pending = false;
    m->pending_action_id[0] = '\0';
}

/* Apply any buffered focus/stats whose rev now matches the authoritative rev. */
static void promote_buffered(bot_model_t *m)
{
    if (m->pending_focus &&
        m->pending_focus_msg.selection_rev == m->selection_rev &&
        m->pending_focus_msg.agent_id == m->selected_agent) {
        m->focus = m->pending_focus_msg;
        m->has_focus = true;
    }
    m->pending_focus = false;
    if (m->pending_stats &&
        m->pending_stats_msg.selection_rev == m->selection_rev &&
        m->pending_stats_msg.agent_id == m->selected_agent) {
        m->stats = m->pending_stats_msg;
        m->has_stats = true;
    }
    m->pending_stats = false;
}

static bot_apply_result_t apply_online(bot_model_t *m, const bot_msg_t *msg)
{
    /* §2: only the current link with a strictly greater seq may act. */
    if (!msg->has_link_id || strncmp(msg->link_id, m->link_id, 32) != 0) {
        return BOT_APPLY_IGNORED; /* old link never mutates */
    }
    if (msg->seq <= m->rx_seq) {
        return BOT_APPLY_IGNORED; /* duplicate / old seq dropped */
    }
    m->rx_seq = msg->seq;

    switch (msg->type) {
    case BOT_MSG_CATALOG:
        m->catalog = msg->body.catalog;
        m->has_catalog = true;
        return BOT_APPLY_APPLIED;

    case BOT_MSG_FOCUS: {
        const bot_focus_t *f = &msg->body.focus;
        if (f->selection_rev < m->selection_rev) return BOT_APPLY_IGNORED;
        if (f->selection_rev > m->selection_rev) {
            /* newer rev: only after ack/welcome confirms; buffer one slot */
            m->pending_focus_msg = *f;
            m->pending_focus = true;
            return BOT_APPLY_BUFFERED;
        }
        if (f->agent_id != m->selected_agent) return BOT_APPLY_IGNORED;
        m->focus = *f;
        m->has_focus = true;
        return BOT_APPLY_APPLIED;
    }

    case BOT_MSG_STATS: {
        const bot_stats_t *s = &msg->body.stats;
        if (s->selection_rev < m->selection_rev) return BOT_APPLY_IGNORED;
        if (s->selection_rev > m->selection_rev) {
            m->pending_stats_msg = *s;
            m->pending_stats = true;
            return BOT_APPLY_BUFFERED;
        }
        if (s->agent_id != m->selected_agent) return BOT_APPLY_IGNORED;
        m->stats = *s;
        m->has_stats = true;
        return BOT_APPLY_APPLIED;
    }

    case BOT_MSG_ACK: {
        const bot_ack_t *a = &msg->body.ack;
        if (!m->action_pending ||
            strncmp(a->action_id, m->pending_action_id, 32) != 0) {
            return BOT_APPLY_IGNORED; /* ack for unknown/stale action */
        }
        m->action_pending = false;
        m->pending_action_id[0] = '\0';
        if (a->status == 0 /* accepted */) {
            m->selected_agent = a->selected_agent;
            m->selection_rev = a->selection_rev;
            promote_buffered(m);
        } else {
            /* rejected: selection unchanged; buffers for that rev die */
            m->pending_focus = false;
            m->pending_stats = false;
        }
        return BOT_APPLY_APPLIED;
    }

    case BOT_MSG_NOTICE:
        m->notice = msg->body.notice;
        m->has_notice = true;
        return BOT_APPLY_APPLIED;

    case BOT_MSG_PING:
        m->last_ping_ms = msg->body.ping_monotonic_ms;
        m->has_ping = true;
        return BOT_APPLY_APPLIED;

    default:
        return BOT_APPLY_IGNORED; /* welcome/hello not valid online */
    }
}

bot_apply_result_t bot_model_apply(bot_model_t *m, const bot_msg_t *msg)
{
    switch (m->state) {
    case BOT_MS_LINK_DOWN:
        return BOT_APPLY_IGNORED; /* nothing to apply before handshake */

    case BOT_MS_HANDSHAKING:
        /* §3.2: welcome (seq=1, fresh link_id) is the only accepted message */
        if (msg->type == BOT_MSG_WELCOME && msg->has_link_id && msg->seq == 1) {
            apply_welcome(m, msg);
            return BOT_APPLY_APPLIED;
        }
        return BOT_APPLY_IGNORED;

    case BOT_MS_ONLINE:
        return apply_online(m, msg);

    default:
        return BOT_APPLY_IGNORED;
    }
}

void bot_model_track_action(bot_model_t *m, const char action_id[33])
{
    memcpy(m->pending_action_id, action_id, 33);
    m->action_pending = true;
}

const char *bot_model_state_name(bot_model_state_t s)
{
    switch (s) {
    case BOT_MS_LINK_DOWN: return "link_down";
    case BOT_MS_HANDSHAKING: return "handshaking";
    case BOT_MS_ONLINE: return "online";
    default: return "?";
    }
}
