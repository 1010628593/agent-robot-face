/* test_device_model.c — T05 model acceptance tests.
 *
 * Covers docs/10_IMPLEMENTATION_PLAN.md T05:
 *   old link / old seq / wrong rev never update; welcome only while
 *   handshaking; new welcome clears pending action and old stats;
 *   newer rev buffered one slot and promoted only after ack confirms.
 */
#include <stdio.h>
#include <string.h>

#include "bot_model.h"

static int failures = 0;

#define CHECK(cond, name)                                  \
    do {                                                   \
        if (cond) {                                        \
            printf("PASS %s\n", name);                     \
        } else {                                           \
            printf("FAIL %s (line %d)\n", name, __LINE__); \
            failures++;                                    \
        }                                                  \
    } while (0)

#define LINK_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define LINK_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

static bot_msg_t make_welcome(const char *link, uint32_t seq, bot_agent_id_t agent,
                              uint32_t rev)
{
    bot_msg_t m;
    memset(&m, 0, sizeof(m));
    m.type = BOT_MSG_WELCOME;
    m.has_link_id = true;
    memcpy(m.link_id, link, 33);
    m.seq = seq;
    m.body.welcome.selected_agent = agent;
    m.body.welcome.selection_rev = rev;
    return m;
}

static bot_msg_t make_focus(const char *link, uint32_t seq, bot_agent_id_t agent,
                            uint32_t rev, bot_state_t state)
{
    bot_msg_t m;
    memset(&m, 0, sizeof(m));
    m.type = BOT_MSG_FOCUS;
    m.has_link_id = true;
    memcpy(m.link_id, link, 33);
    m.seq = seq;
    m.body.focus.agent_id = agent;
    m.body.focus.selection_rev = rev;
    m.body.focus.state = state;
    m.body.focus.reason = BOT_REASON_NONE;
    return m;
}

static bot_msg_t make_stats(const char *link, uint32_t seq, bot_agent_id_t agent,
                            uint32_t rev)
{
    bot_msg_t m;
    memset(&m, 0, sizeof(m));
    m.type = BOT_MSG_STATS;
    m.has_link_id = true;
    memcpy(m.link_id, link, 33);
    m.seq = seq;
    m.body.stats.agent_id = agent;
    m.body.stats.selection_rev = rev;
    m.body.stats.metric_count = 1;
    m.body.stats.metrics[0].key = BOT_MKEY_TURNS;
    m.body.stats.metrics[0].has_value = true;
    m.body.stats.metrics[0].value = 42;
    return m;
}

static bot_msg_t make_ack(const char *link, uint32_t seq, const char *action_id,
                          uint8_t status, uint8_t reason, bot_agent_id_t agent,
                          uint32_t rev)
{
    bot_msg_t m;
    memset(&m, 0, sizeof(m));
    m.type = BOT_MSG_ACK;
    m.has_link_id = true;
    memcpy(m.link_id, link, 33);
    m.seq = seq;
    m.body.ack.status = status;
    m.body.ack.reason = reason;
    m.body.ack.selected_agent = agent;
    m.body.ack.selection_rev = rev;
    memcpy(m.body.ack.action_id, action_id, 33);
    return m;
}

int main(void)
{
    bot_model_t m;
    bot_model_init(&m);

    /* LINK_DOWN: nothing applies */
    bot_msg_t f = make_focus(LINK_A, 5, BOT_AGENT_CODEX, 0, BOT_STATE_WORKING);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_IGNORED, "link_down: focus ignored");

    /* HANDSHAKING: only welcome(seq=1) accepted */
    bot_model_begin_handshake(&m);
    CHECK(m.state == BOT_MS_HANDSHAKING, "begin_handshake -> handshaking");
    bot_msg_t cat;
    memset(&cat, 0, sizeof(cat));
    cat.type = BOT_MSG_CATALOG;
    cat.has_link_id = true;
    memcpy(cat.link_id, LINK_A, 33);
    cat.seq = 2;
    CHECK(bot_model_apply(&m, &cat) == BOT_APPLY_IGNORED,
          "handshaking: catalog ignored");
    bot_msg_t w_bad = make_welcome(LINK_A, 2, BOT_AGENT_CODEX, 3);
    CHECK(bot_model_apply(&m, &w_bad) == BOT_APPLY_IGNORED,
          "handshaking: welcome seq!=1 ignored");
    bot_msg_t w = make_welcome(LINK_A, 1, BOT_AGENT_CODEX, 3);
    CHECK(bot_model_apply(&m, &w) == BOT_APPLY_APPLIED &&
              m.state == BOT_MS_ONLINE &&
              strncmp(m.link_id, LINK_A, 32) == 0 && m.rx_seq == 1 &&
              m.selection_rev == 3,
          "welcome installs link, ONLINE");

    /* old link ignored */
    f = make_focus(LINK_B, 9, BOT_AGENT_CODEX, 3, BOT_STATE_WORKING);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_IGNORED && !m.has_focus,
          "old link focus ignored");

    /* old/duplicate seq ignored */
    f = make_focus(LINK_A, 1, BOT_AGENT_CODEX, 3, BOT_STATE_WORKING);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_IGNORED && !m.has_focus,
          "duplicate seq ignored");

    /* stale rev ignored */
    f = make_focus(LINK_A, 2, BOT_AGENT_CODEX, 2, BOT_STATE_WORKING);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_IGNORED && !m.has_focus,
          "older selection_rev focus ignored (seq consumed)");

    /* wrong agent at current rev ignored */
    f = make_focus(LINK_A, 3, BOT_AGENT_CURSOR, 3, BOT_STATE_WORKING);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_IGNORED && !m.has_focus,
          "focus for non-selected agent ignored");

    /* correct focus applies */
    f = make_focus(LINK_A, 4, BOT_AGENT_CODEX, 3, BOT_STATE_WORKING);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_APPLIED && m.has_focus &&
              m.focus.state == BOT_STATE_WORKING,
          "current rev+agent focus applies");

    /* stats applies, then a NEWER rev is buffered (not applied) */
    bot_msg_t s = make_stats(LINK_A, 5, BOT_AGENT_CODEX, 3);
    CHECK(bot_model_apply(&m, &s) == BOT_APPLY_APPLIED && m.has_stats,
          "stats applies");
    s = make_stats(LINK_A, 6, BOT_AGENT_CURSOR, 4);
    CHECK(bot_model_apply(&m, &s) == BOT_APPLY_BUFFERED && m.pending_stats &&
              m.stats.selection_rev == 3,
          "newer rev stats buffered, mailbox untouched");

    /* ack for unknown action ignored */
    bot_msg_t a = make_ack(LINK_A, 7, "0123456789abcdef0123456789abcdef", 0, 0,
                           BOT_AGENT_CURSOR, 4);
    CHECK(bot_model_apply(&m, &a) == BOT_APPLY_IGNORED,
          "ack for unknown action ignored");

    /* track action, rejected ack drops buffers and keeps selection */
    bot_model_track_action(&m, "0123456789abcdef0123456789abcdef");
    a = make_ack(LINK_A, 8, "0123456789abcdef0123456789abcdef", 1, 1,
                 BOT_AGENT_CODEX, 3);
    CHECK(bot_model_apply(&m, &a) == BOT_APPLY_APPLIED &&
              m.selected_agent == BOT_AGENT_CODEX && m.selection_rev == 3 &&
              !m.pending_stats,
          "rejected ack: selection kept, buffer dropped");

    /* buffer again, then accepted ack promotes */
    s = make_stats(LINK_A, 9, BOT_AGENT_CURSOR, 4);
    CHECK(bot_model_apply(&m, &s) == BOT_APPLY_BUFFERED, "re-buffered rev 4 stats");
    f = make_focus(LINK_A, 10, BOT_AGENT_CURSOR, 4, BOT_STATE_IDLE);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_BUFFERED, "re-buffered rev 4 focus");
    bot_model_track_action(&m, "11111111111111111111111111111111");
    a = make_ack(LINK_A, 11, "11111111111111111111111111111111", 0, 0,
                 BOT_AGENT_CURSOR, 4);
    CHECK(bot_model_apply(&m, &a) == BOT_APPLY_APPLIED &&
              m.selected_agent == BOT_AGENT_CURSOR && m.selection_rev == 4 &&
              m.has_stats && m.stats.selection_rev == 4 &&
              m.has_focus && m.focus.selection_rev == 4,
          "accepted ack promotes buffered focus+stats");

    /* welcome is NOT accepted while online */
    w = make_welcome(LINK_B, 1, BOT_AGENT_HERMES, 9);
    CHECK(bot_model_apply(&m, &w) == BOT_APPLY_IGNORED &&
              strncmp(m.link_id, LINK_A, 32) == 0,
          "online: new welcome ignored (needs re-handshake)");

    /* re-handshake: welcome clears pending action + old stats/focus */
    bot_model_track_action(&m, "22222222222222222222222222222222");
    bot_model_begin_handshake(&m);
    w = make_welcome(LINK_B, 1, BOT_AGENT_HERMES, 9);
    CHECK(bot_model_apply(&m, &w) == BOT_APPLY_APPLIED &&
              m.state == BOT_MS_ONLINE && !m.has_stats && !m.has_focus &&
              !m.action_pending && m.selected_agent == BOT_AGENT_HERMES &&
              m.selection_rev == 9,
          "new welcome clears pending action and old stats");

    /* old link's late frames after re-handshake are dead */
    f = make_focus(LINK_A, 99, BOT_AGENT_CODEX, 3, BOT_STATE_ERROR);
    CHECK(bot_model_apply(&m, &f) == BOT_APPLY_IGNORED && !m.has_focus,
          "old link command after re-handshake never acts");

    if (failures) {
        printf("test_device_model: %d FAILURE(S)\n", failures);
        return 1;
    }
    printf("test_device_model: all passed\n");
    return 0;
}
