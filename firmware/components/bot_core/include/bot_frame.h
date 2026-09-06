/* bot_frame.h — USB text frame parser + strict message decoder (T05).
 *
 * Frame format (docs/05_PROTOCOL.md §1): "@bot " + one compact UTF-8 JSON
 * line + "\n". JSON body <= 8192 bytes; CRLF tolerated (CR not counted).
 * Receiver accumulates byte-by-byte; supports split/coalesced frames, log
 * noise between frames, oversize-line resync. Strict JSON per bot_json.h;
 * unknown fields rejected (v1 strict, §10).
 *
 * Decoded output is a fixed-size bot_msg_t — the parse tree is released
 * immediately after extraction (§9). Invalid messages never reach the model.
 */
#ifndef BOT_FRAME_H
#define BOT_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "bot_json.h"
#include "bot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- decoded message bodies (fixed size, contract field limits) -------- */

typedef enum {
    BOT_MSG_NONE = 0,
    BOT_MSG_WELCOME,
    BOT_MSG_CATALOG,
    BOT_MSG_FOCUS,
    BOT_MSG_STATS,
    BOT_MSG_ACTION, /* D->H, decoded for loopback tests */
    BOT_MSG_ACK,
    BOT_MSG_NOTICE,
    BOT_MSG_PING,
    BOT_MSG_PONG,
    BOT_MSG_HELLO
} bot_msg_type_t;

typedef enum {
    BOT_HEALTH_READY = 0,
    BOT_HEALTH_PARTIAL,
    BOT_HEALTH_UNAVAILABLE,
    BOT_HEALTH_NEEDS_AUTH,
    BOT_HEALTH_DISABLED
} bot_health_t;

typedef enum {
    BOT_CAP_OBSERVED = 0, BOT_CAP_REPORTED, BOT_CAP_MANUAL, BOT_CAP_NONE
} bot_cap_state_t;
typedef enum {
    BOT_CAPU_AUTOMATIC = 0, BOT_CAPU_IMPORT, BOT_CAPU_MANUAL, BOT_CAPU_NONE
} bot_cap_usage_t;
typedef enum {
    BOT_CAPQ_OFFICIAL = 0, BOT_CAPQ_IMPORT, BOT_CAPQ_MANUAL, BOT_CAPQ_NONE
} bot_cap_quota_t;

typedef enum {
    BOT_REASON_NONE = 0, BOT_REASON_APPROVAL, BOT_REASON_INPUT,
    BOT_REASON_COMPLETED, BOT_REASON_FAILED, BOT_REASON_CANCELLED,
    BOT_REASON_UNOBSERVED
} bot_reason_t;

typedef enum {
    BOT_MQUAL_EXACT = 0, BOT_MQUAL_ESTIMATED, BOT_MQUAL_MANUAL,
    BOT_MQUAL_UNAVAILABLE, BOT_MQUAL_SIMULATED
} bot_metric_quality_t;

typedef enum {
    BOT_COV_COMPLETE = 0, BOT_COV_PARTIAL,
    BOT_COV_SINCE_BRIDGE_START, BOT_COV_UNKNOWN
} bot_coverage_t;

typedef enum {
    BOT_MKEY_TURNS = 0, BOT_MKEY_TOOL_CALLS, BOT_MKEY_REQUESTS,
    BOT_MKEY_INPUT_TOKENS, BOT_MKEY_OUTPUT_TOKENS, BOT_MKEY_TOTAL_TOKENS,
    BOT_MKEY_ACTIVE_TIME_MS, BOT_MKEY_COST_USD_MICROS, BOT_MKEY_CONTEXT_TOKENS
} bot_metric_key_t;

typedef enum {
    BOT_UNIT_TURN = 0, BOT_UNIT_CALL, BOT_UNIT_REQUEST, BOT_UNIT_TOKEN,
    BOT_UNIT_MS, BOT_UNIT_USD_MICROS
} bot_metric_unit_t;

typedef enum {
    BOT_QKIND_RATE_WINDOW = 0, BOT_QKIND_CREDITS, BOT_QKIND_SPEND_BUDGET,
    BOT_QKIND_UNLIMITED, BOT_QKIND_UNKNOWN
} bot_quota_kind_t;

typedef enum {
    BOT_QUNIT_PERCENT = 0, BOT_QUNIT_CREDIT, BOT_QUNIT_USD_MICROS,
    BOT_QUNIT_TOKEN, BOT_QUNIT_REQUEST, BOT_QUNIT_NONE
} bot_quota_unit_t;

typedef enum {
    BOT_QSCOPE_ACCOUNT = 0, BOT_QSCOPE_PROVIDER,
    BOT_QSCOPE_ORGANIZATION, BOT_QSCOPE_LOCAL_BUDGET
} bot_quota_scope_t;

typedef enum {
    BOT_QAVAIL_AVAILABLE = 0, BOT_QAVAIL_NEEDS_AUTH,
    BOT_QAVAIL_UNSUPPORTED, BOT_QAVAIL_ERROR
} bot_quota_avail_t;

typedef struct {
    bot_agent_id_t id;
    char label[17];
    bot_state_t state;
    bot_health_t health;
    uint8_t active_sessions;
    uint8_t attention_count;
    bot_cap_state_t cap_state;
    bot_cap_usage_t cap_usage;
    bot_cap_quota_t cap_quota;
    bool cap_open_agent; /* host allowlist: open_agent action permitted */
    bool cap_open_usage; /* host allowlist: open_usage action permitted */
} bot_catalog_agent_t;

typedef struct {
    char bridge_epoch[33];
    bot_agent_id_t selected_agent;
    uint32_t selection_rev;
    uint32_t heartbeat_ms;     /* const 2000 in v1 */
    uint32_t offline_after_ms; /* const 6000 in v1 */
    uint32_t max_frame_bytes;  /* const 8192 in v1 */
    bool demo;
} bot_welcome_t;

typedef struct {
    uint8_t count; /* 1..4 */
    bot_catalog_agent_t agents[BOT_AGENT_COUNT];
} bot_catalog_t;

typedef struct {
    bot_agent_id_t agent_id;
    uint32_t selection_rev;
    bool has_session_key;
    char session_key[65];
    bool has_run_id;
    char run_id[BOT_RUN_ID_MAX + 1];
    bot_state_t state;
    bot_reason_t reason;
    bot_quality_t quality;
    bool has_source_age_ms;
    uint64_t source_age_ms;
    bool stale;
    char tool[BOT_TOOL_NAME_MAX + 1];
    char detail[49];
    uint64_t run_elapsed_ms;
    uint8_t active_sessions;
    bool has_progress;
    double progress; /* 0..1 when has_progress */
} bot_focus_t;

typedef struct {
    bot_metric_key_t key;
    char label[21];
    bool has_value; /* false == unavailable must be value=null, never 0 */
    double value;
    bot_metric_unit_t unit;
    bot_metric_quality_t quality;
    bot_coverage_t coverage;
    char source[41];
    bool has_as_of_ms;
    uint64_t as_of_ms;
    uint32_t stale_after_ms;
} bot_metric_t;

typedef struct {
    char id[41];
    char account_key[65];
    bot_quota_scope_t scope;
    char label[25];
    bot_quota_kind_t kind;
    bot_quota_unit_t unit;
    bool has_used, has_limit, has_remaining, has_used_pct;
    double used, limit, remaining, used_pct;
    bool has_resets_at_ms;
    uint64_t resets_at_ms;
    bot_metric_quality_t quality;
    bot_coverage_t coverage;
    char source[41];
    bool has_as_of_ms;
    uint64_t as_of_ms;
    uint32_t stale_after_ms;
    bot_quota_avail_t availability;
    char reason[41];
    uint8_t shared_count;
    bot_agent_id_t shared_with[BOT_AGENT_COUNT];
} bot_quota_t;

typedef struct {
    bot_agent_id_t agent_id;
    uint32_t selection_rev;
    uint8_t scope_kind; /* 0=today 1=session */
    char scope_timezone[65];
    uint64_t scope_start_ms;
    uint64_t scope_end_ms;
    uint8_t metric_count;
    bot_metric_t metrics[6];
    uint8_t quota_count;
    bot_quota_t quotas[2];
    uint8_t sparkline_count;
    double sparkline[24];
} bot_stats_t;

typedef struct {
    char action_id[33];
    uint8_t status; /* 0=accepted 1=rejected */
    bot_agent_id_t selected_agent;
    uint32_t selection_rev;
    uint8_t reason; /* 0 ok,1 conflict,2 offline,3 unsupported,4 rate_limited,5 invalid,6 internal_error */
} bot_ack_t;

typedef struct {
    char notice_id[65];
    bot_agent_id_t agent_id;
    bool has_run_id;
    char run_id[BOT_RUN_ID_MAX + 1];
    uint8_t kind; /* 0 waiting 1 done 2 error 3 quota_low */
    char label[41];
    uint32_t expires_in_ms;
} bot_notice_t;

typedef struct {
    bot_msg_type_t type;
    bool has_link_id; /* hello is the only message allowed link_id=null */
    char link_id[33];
    uint32_t seq;
    union {
        bot_welcome_t welcome;
        bot_catalog_t catalog;
        bot_focus_t focus;
        bot_stats_t stats;
        bot_ack_t ack;
        bot_notice_t notice;
        uint64_t ping_monotonic_ms;
    } body;
} bot_msg_t;

/* ---- frame parser ------------------------------------------------------- */

typedef enum {
    BOT_FR_SEEK = 0, /* scanning for '@' of "@bot " */
    BOT_FR_PREFIX,   /* matching "bot " after '@' */
    BOT_FR_LINE,     /* accumulating JSON line */
    BOT_FR_SKIP      /* oversize line: discard until newline, then resync */
} bot_frame_state_t;

typedef enum {
    BOT_FRAME_NONE = 0,  /* byte consumed, no message yet */
    BOT_FRAME_MSG,       /* *out now holds one fully validated message */
    BOT_FRAME_BAD_LINE,  /* line looked like a frame but failed strict checks */
    BOT_FRAME_OVERSIZE   /* line exceeded the budget; resyncing at next \n */
} bot_frame_result_t;

typedef struct {
    bot_frame_state_t state;
    uint8_t prefix_pos;
    uint16_t line_len;
    bool overflow;
    uint32_t bad_lines;
    uint32_t oversize_lines;
    uint32_t decoded;
    char line[BOT_FRAME_MAX_BYTES + 2]; /* + CR + NUL */
    /* parse workspace (reused per line; tree released after decode) */
    bj_doc_t doc;
    bj_node_t *nodes;
    uint32_t node_cap;
    char *str_pool;
    uint32_t str_cap;
    bj_err_t last_json_err;
} bot_frame_parser_t;

void bot_frame_init(bot_frame_parser_t *fp, bj_node_t *nodes, uint32_t node_cap,
                    char *str_pool, uint32_t str_cap);

/* Feed one byte. On BOT_FRAME_MSG, *out contains a fully validated message;
 * the internal parse tree is already released and may be reused. */
bot_frame_result_t bot_frame_feed(bot_frame_parser_t *fp, uint8_t byte,
                                  bot_msg_t *out);

const char *bot_msg_type_name(bot_msg_type_t t);

#ifdef __cplusplus
}
#endif

#endif /* BOT_FRAME_H */
