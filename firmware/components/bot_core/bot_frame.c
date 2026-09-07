/* bot_frame.c — frame parser + strict message decoder (T05).
 *
 * Decode pipeline per completed line: strict JSON parse (bot_json) ->
 * envelope validation -> per-type body validation (schema shape + semantic
 * rules mirrored from tools/validate_contracts.py, the contract authority).
 * Unknown keys are rejected everywhere (v1 strict, 05_PROTOCOL §10).
 * The parse tree lives in the caller-provided pool and is released as soon
 * as the fixed-size bot_msg_t is filled (§9).
 */
#include "bot_frame.h"

#include <math.h>
#include <string.h>

#define FRAME_PREFIX "@bot "
#define FRAME_PREFIX_LEN 5

/* ---- small helpers ------------------------------------------------------ */

static bool str_eq(const bj_node_t *n, const char *s)
{
    size_t len = strlen(s);
    return n->type == BJ_STR && n->str_len == len && memcmp(n->str, s, len) == 0;
}

static bool is_hex32(const char *s)
{
    for (int i = 0; i < 32; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return s[32] == '\0';
}

static bool is_ascii_printable(const char *s, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        if (s[i] < 0x20 || s[i] > 0x7E) return false;
    }
    return true;
}

/* Reject any object member not in the allowed key list (additionalProperties:false). */
static bool no_extra_keys(const bj_doc_t *doc, int32_t obj,
                          const char *const *keys, int key_count)
{
    const bj_node_t *o = &doc->nodes[obj];
    for (int32_t i = o->child; i != -1; i = doc->nodes[i].next) {
        const bj_node_t *c = &doc->nodes[i];
        bool known = false;
        for (int k = 0; k < key_count; k++) {
            if (c->key_len == strlen(keys[k]) &&
                memcmp(c->key, keys[k], c->key_len) == 0) {
                known = true;
                break;
            }
        }
        if (!known) return false;
    }
    return true;
}

/* Required string field with max length + ASCII printable constraint. */
static bool req_str(const bj_doc_t *doc, int32_t obj, const char *key,
                    char *out, uint32_t out_cap, uint16_t max_len, bool ascii_only)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0 || doc->nodes[n].type != BJ_STR) return false;
    const bj_node_t *s = &doc->nodes[n];
    if (s->str_len > max_len) return false;
    if (ascii_only && !is_ascii_printable(s->str, s->str_len)) return false;
    if ((uint32_t)s->str_len + 1 > out_cap) return false;
    memcpy(out, s->str, s->str_len);
    out[s->str_len] = '\0';
    return true;
}

/* Required integer field within [lo, hi] (rejects non-integral doubles). */
static bool req_int(const bj_doc_t *doc, int32_t obj, const char *key,
                    double lo, double hi, double *out)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0 || doc->nodes[n].type != BJ_NUM) return false;
    double v = doc->nodes[n].num;
    if (v != floor(v) || v < lo || v > hi) return false;
    *out = v;
    return true;
}

/* Optional number-or-null field. has_* set to false when JSON null. */
static bool opt_num_nullable(const bj_doc_t *doc, int32_t obj, const char *key,
                             bool *has, double *out)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0) return false; /* field itself is required by schema */
    if (doc->nodes[n].type == BJ_NULL) {
        *has = false;
        return true;
    }
    if (doc->nodes[n].type != BJ_NUM) return false;
    *has = true;
    *out = doc->nodes[n].num;
    return true;
}

/* ---- enum tables -------------------------------------------------------- */

typedef struct {
    const char *name;
    int value;
} enum_entry_t;

static bool enum_lookup(const enum_entry_t *tbl, int n, const bj_node_t *node,
                        int *out)
{
    if (node->type != BJ_STR) return false;
    for (int i = 0; i < n; i++) {
        if (str_eq(node, tbl[i].name)) {
            *out = tbl[i].value;
            return true;
        }
    }
    return false;
}

static const enum_entry_t AGENTS[] = {
    { "codex", BOT_AGENT_CODEX }, { "workbuddy", BOT_AGENT_WORKBUDDY },
    { "cursor", BOT_AGENT_CURSOR }, { "hermes", BOT_AGENT_HERMES },
};
static const enum_entry_t MODES[] = {{"auto", BOT_SELECTION_AUTO}, {"pinned", BOT_SELECTION_PINNED}};
static const enum_entry_t STATES[] = {
    { "idle", BOT_STATE_IDLE }, { "working", BOT_STATE_WORKING },
    { "tool", BOT_STATE_TOOL }, { "waiting", BOT_STATE_WAITING },
    { "done", BOT_STATE_DONE }, { "error", BOT_STATE_ERROR },
    { "cancelled", BOT_STATE_CANCELLED }, { "unknown", BOT_STATE_UNKNOWN },
};
static const enum_entry_t HEALTHS[] = {
    { "ready", BOT_HEALTH_READY }, { "partial", BOT_HEALTH_PARTIAL },
    { "unavailable", BOT_HEALTH_UNAVAILABLE },
    { "needs_auth", BOT_HEALTH_NEEDS_AUTH }, { "disabled", BOT_HEALTH_DISABLED },
};
static const enum_entry_t CAP_STATE[] = {
    { "observed", BOT_CAP_OBSERVED }, { "reported", BOT_CAP_REPORTED },
    { "manual", BOT_CAP_MANUAL }, { "none", BOT_CAP_NONE },
};
static const enum_entry_t CAP_USAGE[] = {
    { "automatic", BOT_CAPU_AUTOMATIC }, { "import", BOT_CAPU_IMPORT },
    { "manual", BOT_CAPU_MANUAL }, { "none", BOT_CAPU_NONE },
};
static const enum_entry_t CAP_QUOTA[] = {
    { "official", BOT_CAPQ_OFFICIAL }, { "import", BOT_CAPQ_IMPORT },
    { "manual", BOT_CAPQ_MANUAL }, { "none", BOT_CAPQ_NONE },
};
static const enum_entry_t QUALITIES[] = {
    { "observed", BOT_QUALITY_OBSERVED }, { "inferred", BOT_QUALITY_INFERRED },
    { "reported", BOT_QUALITY_REPORTED }, { "manual", BOT_QUALITY_MANUAL },
    { "simulated", BOT_QUALITY_SIMULATED },
};
static const enum_entry_t REASONS[] = {
    { "none", BOT_REASON_NONE }, { "approval", BOT_REASON_APPROVAL },
    { "input", BOT_REASON_INPUT }, { "completed", BOT_REASON_COMPLETED },
    { "failed", BOT_REASON_FAILED }, { "cancelled", BOT_REASON_CANCELLED },
    { "unobserved", BOT_REASON_UNOBSERVED },
};
static const enum_entry_t MQUAL[] = {
    { "exact", BOT_MQUAL_EXACT }, { "estimated", BOT_MQUAL_ESTIMATED },
    { "manual", BOT_MQUAL_MANUAL }, { "unavailable", BOT_MQUAL_UNAVAILABLE },
    { "simulated", BOT_MQUAL_SIMULATED },
};
static const enum_entry_t COVS[] = {
    { "complete", BOT_COV_COMPLETE }, { "partial", BOT_COV_PARTIAL },
    { "since_bridge_start", BOT_COV_SINCE_BRIDGE_START },
    { "unknown", BOT_COV_UNKNOWN },
};
/* key/unit pairs stay aligned: contract UNIT map (validate_contracts.py) */
static const enum_entry_t MKEYS[] = {
    { "turns", BOT_MKEY_TURNS }, { "tool_calls", BOT_MKEY_TOOL_CALLS },
    { "requests", BOT_MKEY_REQUESTS }, { "input_tokens", BOT_MKEY_INPUT_TOKENS },
    { "output_tokens", BOT_MKEY_OUTPUT_TOKENS },
    { "total_tokens", BOT_MKEY_TOTAL_TOKENS },
    { "active_time_ms", BOT_MKEY_ACTIVE_TIME_MS },
    { "cost_usd_micros", BOT_MKEY_COST_USD_MICROS },
    { "context_tokens", BOT_MKEY_CONTEXT_TOKENS },
};
static const enum_entry_t MUNITS[] = {
    { "turn", BOT_UNIT_TURN }, { "call", BOT_UNIT_CALL },
    { "request", BOT_UNIT_REQUEST }, { "token", BOT_UNIT_TOKEN },
    { "ms", BOT_UNIT_MS }, { "usd_micros", BOT_UNIT_USD_MICROS },
};
static const enum_entry_t QKINDS[] = {
    { "rate_window", BOT_QKIND_RATE_WINDOW }, { "credits", BOT_QKIND_CREDITS },
    { "spend_budget", BOT_QKIND_SPEND_BUDGET },
    { "unlimited", BOT_QKIND_UNLIMITED }, { "unknown", BOT_QKIND_UNKNOWN },
};
static const enum_entry_t QUNITS[] = {
    { "percent", BOT_QUNIT_PERCENT }, { "credit", BOT_QUNIT_CREDIT },
    { "usd_micros", BOT_QUNIT_USD_MICROS }, { "token", BOT_QUNIT_TOKEN },
    { "request", BOT_QUNIT_REQUEST }, { "none", BOT_QUNIT_NONE },
};
static const enum_entry_t QSCOPES[] = {
    { "account", BOT_QSCOPE_ACCOUNT }, { "provider", BOT_QSCOPE_PROVIDER },
    { "organization", BOT_QSCOPE_ORGANIZATION },
    { "local_budget", BOT_QSCOPE_LOCAL_BUDGET },
};
static const enum_entry_t QAVAILS[] = {
    { "available", BOT_QAVAIL_AVAILABLE }, { "needs_auth", BOT_QAVAIL_NEEDS_AUTH },
    { "unsupported", BOT_QAVAIL_UNSUPPORTED }, { "error", BOT_QAVAIL_ERROR },
};

#define TBL_LEN(t) ((int)(sizeof(t) / sizeof((t)[0])))

static bool req_enum(const bj_doc_t *doc, int32_t obj, const char *key,
                     const enum_entry_t *tbl, int n, int *out)
{
    int32_t node = bj_obj_get(doc, obj, key);
    if (node < 0) return false;
    return enum_lookup(tbl, n, &doc->nodes[node], out);
}

/* key -> expected unit (contract UNIT map) */
static bot_metric_unit_t metric_unit_for(bot_metric_key_t key)
{
    switch (key) {
    case BOT_MKEY_TURNS: return BOT_UNIT_TURN;
    case BOT_MKEY_TOOL_CALLS: return BOT_UNIT_CALL;
    case BOT_MKEY_REQUESTS: return BOT_UNIT_REQUEST;
    case BOT_MKEY_INPUT_TOKENS:
    case BOT_MKEY_OUTPUT_TOKENS:
    case BOT_MKEY_TOTAL_TOKENS:
    case BOT_MKEY_CONTEXT_TOKENS: return BOT_UNIT_TOKEN;
    case BOT_MKEY_ACTIVE_TIME_MS: return BOT_UNIT_MS;
    case BOT_MKEY_COST_USD_MICROS: return BOT_UNIT_USD_MICROS;
    default: return BOT_UNIT_TOKEN;
    }
}

/* ---- body decoders ------------------------------------------------------ */

static bool dec_welcome(const bj_doc_t *doc, int32_t body, bot_welcome_t *w)
{
    static const char *const KEYS[] = {
        "mode", "bridge_epoch", "selected_agent", "selection_rev", "heartbeat_ms",
        "offline_after_ms", "max_frame_bytes", "demo"
    };
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    if (!req_str(doc, body, "bridge_epoch", w->bridge_epoch,
                 sizeof(w->bridge_epoch), 32, true)) return false;
    if (!is_hex32(w->bridge_epoch)) return false;
    int v;
    if (!req_enum(doc, body, "selected_agent", AGENTS, TBL_LEN(AGENTS), &v)) return false;
    w->selected_agent = (bot_agent_id_t)v;
    if (!req_enum(doc, body, "mode", MODES, TBL_LEN(MODES), &v)) return false;
    w->mode = (bot_selection_mode_t)v;
    double d;
    if (!req_int(doc, body, "selection_rev", 0, 2147483647, &d)) return false;
    w->selection_rev = (uint32_t)d;
    if (!req_int(doc, body, "heartbeat_ms", 2000, 2000, &d)) return false;
    w->heartbeat_ms = (uint32_t)d;
    if (!req_int(doc, body, "offline_after_ms", 6000, 6000, &d)) return false;
    w->offline_after_ms = (uint32_t)d;
    if (!req_int(doc, body, "max_frame_bytes", 8192, 8192, &d)) return false;
    w->max_frame_bytes = (uint32_t)d;
    return bj_get_bool(doc, body, "demo", &w->demo);
}

static bool dec_capabilities(const bj_doc_t *doc, int32_t caps,
                             bot_catalog_agent_t *a)
{
    static const char *const KEYS[] = {
        "state", "usage", "quota", "open_agent", "open_usage"
    };
    if (caps < 0 || doc->nodes[caps].type != BJ_OBJ) return false;
    if (!no_extra_keys(doc, caps, KEYS, TBL_LEN(KEYS))) return false;
    int v;
    if (!req_enum(doc, caps, "state", CAP_STATE, TBL_LEN(CAP_STATE), &v)) return false;
    a->cap_state = (bot_cap_state_t)v;
    if (!req_enum(doc, caps, "usage", CAP_USAGE, TBL_LEN(CAP_USAGE), &v)) return false;
    a->cap_usage = (bot_cap_usage_t)v;
    if (!req_enum(doc, caps, "quota", CAP_QUOTA, TBL_LEN(CAP_QUOTA), &v)) return false;
    a->cap_quota = (bot_cap_quota_t)v;
    if (!bj_get_bool(doc, caps, "open_agent", &a->cap_open_agent)) return false;
    if (!bj_get_bool(doc, caps, "open_usage", &a->cap_open_usage)) return false;
    return true;
}

static bool dec_catalog(const bj_doc_t *doc, int32_t body, bot_catalog_t *c)
{
    static const char *const KEYS[] = { "agents" };
    static const char *const AGENT_KEYS[] = {
        "id", "label", "state", "health", "active_sessions",
        "attention_count", "capabilities"
    };
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    int32_t arr = bj_obj_get(doc, body, "agents");
    if (arr < 0 || doc->nodes[arr].type != BJ_ARR) return false;
    uint8_t count = 0;
    bool seen[BOT_AGENT_COUNT] = { false, false, false, false };
    for (int32_t i = doc->nodes[arr].child; i != -1; i = doc->nodes[i].next) {
        if (count >= BOT_AGENT_COUNT) return false; /* maxItems=4 */
        if (doc->nodes[i].type != BJ_OBJ) return false;
        bot_catalog_agent_t *a = &c->agents[count];
        if (!no_extra_keys(doc, i, AGENT_KEYS, TBL_LEN(AGENT_KEYS))) return false;
        int v;
        if (!req_enum(doc, i, "id", AGENTS, TBL_LEN(AGENTS), &v)) return false;
        a->id = (bot_agent_id_t)v;
        if (seen[a->id]) return false; /* unique ids */
        seen[a->id] = true;
        if (!req_str(doc, i, "label", a->label, sizeof(a->label), 16, true)) return false;
        if (!req_enum(doc, i, "state", STATES, TBL_LEN(STATES), &v)) return false;
        a->state = (bot_state_t)v;
        if (!req_enum(doc, i, "health", HEALTHS, TBL_LEN(HEALTHS), &v)) return false;
        a->health = (bot_health_t)v;
        double d;
        if (!req_int(doc, i, "active_sessions", 0, 16, &d)) return false;
        a->active_sessions = (uint8_t)d;
        if (!req_int(doc, i, "attention_count", 0, 99, &d)) return false;
        a->attention_count = (uint8_t)d;
        if (!dec_capabilities(doc, bj_obj_get(doc, i, "capabilities"), a)) return false;
        count++;
    }
    /* semantic: catalog must contain the four unique agents */
    if (count != BOT_AGENT_COUNT) return false;
    for (int k = 0; k < BOT_AGENT_COUNT; k++) {
        if (!seen[k]) return false;
    }
    c->count = count;
    return true;
}

static bool dec_nullable_str(const bj_doc_t *doc, int32_t obj, const char *key,
                             bool *has, char *out, uint32_t out_cap, uint16_t max_len)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0) return false;
    if (doc->nodes[n].type == BJ_NULL) {
        *has = false;
        out[0] = '\0';
        return true;
    }
    if (!req_str(doc, obj, key, out, out_cap, max_len, true)) return false;
    *has = true;
    return true;
}

static bool dec_focus(const bj_doc_t *doc, int32_t body, bot_focus_t *f)
{
    static const char *const KEYS[] = {
        "agent_id", "selection_rev", "session_key", "run_id", "state",
        "reason", "quality", "source_age_ms", "stale", "tool", "detail",
        "run_elapsed_ms", "active_sessions", "progress"
    };
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    int v;
    if (!req_enum(doc, body, "agent_id", AGENTS, TBL_LEN(AGENTS), &v)) return false;
    f->agent_id = (bot_agent_id_t)v;
    double d;
    if (!req_int(doc, body, "selection_rev", 0, 2147483647, &d)) return false;
    f->selection_rev = (uint32_t)d;
    if (!dec_nullable_str(doc, body, "session_key", &f->has_session_key,
                          f->session_key, sizeof(f->session_key), 64)) return false;
    if (!dec_nullable_str(doc, body, "run_id", &f->has_run_id,
                          f->run_id, sizeof(f->run_id), BOT_RUN_ID_MAX)) return false;
    if (!req_enum(doc, body, "state", STATES, TBL_LEN(STATES), &v)) return false;
    f->state = (bot_state_t)v;
    if (!req_enum(doc, body, "reason", REASONS, TBL_LEN(REASONS), &v)) return false;
    f->reason = (bot_reason_t)v;
    if (!req_enum(doc, body, "quality", QUALITIES, TBL_LEN(QUALITIES), &v)) return false;
    f->quality = (bot_quality_t)v;
    if (!opt_num_nullable(doc, body, "source_age_ms", &f->has_source_age_ms, &d)) return false;
    if (f->has_source_age_ms) {
        if (d != floor(d) || d < 0 || d > 9007199254740991.0) return false;
        f->source_age_ms = (uint64_t)d;
    }
    if (!bj_get_bool(doc, body, "stale", &f->stale)) return false;
    if (!req_str(doc, body, "tool", f->tool, sizeof(f->tool), 24, true)) return false;
    if (!req_str(doc, body, "detail", f->detail, sizeof(f->detail), 48, true)) return false;
    if (!req_int(doc, body, "run_elapsed_ms", 0, 9007199254740991.0, &d)) return false;
    f->run_elapsed_ms = (uint64_t)d;
    if (!req_int(doc, body, "active_sessions", 0, 16, &d)) return false;
    f->active_sessions = (uint8_t)d;
    if (!opt_num_nullable(doc, body, "progress", &f->has_progress, &f->progress)) return false;
    if (f->has_progress && (f->progress < 0.0 || f->progress > 1.0)) return false;
    /* semantic: waiting needs explicit reason; terminal states need matching reason */
    if (f->state == BOT_STATE_WAITING &&
        f->reason != BOT_REASON_APPROVAL && f->reason != BOT_REASON_INPUT) return false;
    if (f->state == BOT_STATE_DONE && f->reason != BOT_REASON_COMPLETED) return false;
    if (f->state == BOT_STATE_ERROR && f->reason != BOT_REASON_FAILED) return false;
    if (f->state == BOT_STATE_CANCELLED && f->reason != BOT_REASON_CANCELLED) return false;
    return true;
}

static bool dec_metric(const bj_doc_t *doc, int32_t node, bot_metric_t *m)
{
    static const char *const KEYS[] = {
        "key", "label", "value", "unit", "quality", "coverage", "source",
        "as_of_ms", "stale_after_ms"
    };
    if (!no_extra_keys(doc, node, KEYS, TBL_LEN(KEYS))) return false;
    int v;
    if (!req_enum(doc, node, "key", MKEYS, TBL_LEN(MKEYS), &v)) return false;
    m->key = (bot_metric_key_t)v;
    if (!req_str(doc, node, "label", m->label, sizeof(m->label), 20, true)) return false;
    if (!opt_num_nullable(doc, node, "value", &m->has_value, &m->value)) return false;
    if (!req_enum(doc, node, "unit", MUNITS, TBL_LEN(MUNITS), &v)) return false;
    m->unit = (bot_metric_unit_t)v;
    if (!req_enum(doc, node, "quality", MQUAL, TBL_LEN(MQUAL), &v)) return false;
    m->quality = (bot_metric_quality_t)v;
    if (!req_enum(doc, node, "coverage", COVS, TBL_LEN(COVS), &v)) return false;
    m->coverage = (bot_coverage_t)v;
    if (!req_str(doc, node, "source", m->source, sizeof(m->source), 40, true)) return false;
    double d;
    if (!opt_num_nullable(doc, node, "as_of_ms", &m->has_as_of_ms, &d)) return false;
    if (m->has_as_of_ms) {
        if (d != floor(d) || d < 0 || d > 9007199254740991.0) return false;
        m->as_of_ms = (uint64_t)d;
    }
    if (!req_int(doc, node, "stale_after_ms", 1000, 86400000, &d)) return false;
    m->stale_after_ms = (uint32_t)d;
    /* semantic: key/unit consistency; unavailable -> value null; known -> value+as_of */
    if (metric_unit_for(m->key) != m->unit) return false;
    if (m->quality == BOT_MQUAL_UNAVAILABLE) {
        if (m->has_value) return false;
    } else {
        if (!m->has_value || !m->has_as_of_ms) return false;
    }
    return true;
}

static bool dec_quota(const bj_doc_t *doc, int32_t node, bot_quota_t *q,
                      bot_agent_id_t stats_agent)
{
    static const char *const KEYS[] = {
        "id", "account_key", "scope", "label", "kind", "unit", "used",
        "limit", "remaining", "used_pct", "resets_at_ms", "quality",
        "coverage", "source", "as_of_ms", "stale_after_ms", "availability",
        "reason", "shared_with"
    };
    if (!no_extra_keys(doc, node, KEYS, TBL_LEN(KEYS))) return false;
    if (!req_str(doc, node, "id", q->id, sizeof(q->id), 40, true)) return false;
    if (!req_str(doc, node, "account_key", q->account_key,
                 sizeof(q->account_key), 64, true)) return false;
    int v;
    if (!req_enum(doc, node, "scope", QSCOPES, TBL_LEN(QSCOPES), &v)) return false;
    q->scope = (bot_quota_scope_t)v;
    if (!req_str(doc, node, "label", q->label, sizeof(q->label), 24, true)) return false;
    if (!req_enum(doc, node, "kind", QKINDS, TBL_LEN(QKINDS), &v)) return false;
    q->kind = (bot_quota_kind_t)v;
    if (!req_enum(doc, node, "unit", QUNITS, TBL_LEN(QUNITS), &v)) return false;
    q->unit = (bot_quota_unit_t)v;
    if (!opt_num_nullable(doc, node, "used", &q->has_used, &q->used)) return false;
    if (!opt_num_nullable(doc, node, "limit", &q->has_limit, &q->limit)) return false;
    if (!opt_num_nullable(doc, node, "remaining", &q->has_remaining, &q->remaining)) return false;
    if (!opt_num_nullable(doc, node, "used_pct", &q->has_used_pct, &q->used_pct)) return false;
    double d;
    if (!opt_num_nullable(doc, node, "resets_at_ms", &q->has_resets_at_ms, &d)) return false;
    if (q->has_resets_at_ms) {
        if (d != floor(d) || d < 0 || d > 9007199254740991.0) return false;
        q->resets_at_ms = (uint64_t)d;
    }
    if (!req_enum(doc, node, "quality", MQUAL, TBL_LEN(MQUAL), &v)) return false;
    q->quality = (bot_metric_quality_t)v;
    if (!req_enum(doc, node, "coverage", COVS, TBL_LEN(COVS), &v)) return false;
    q->coverage = (bot_coverage_t)v;
    if (!req_str(doc, node, "source", q->source, sizeof(q->source), 40, true)) return false;
    if (!opt_num_nullable(doc, node, "as_of_ms", &q->has_as_of_ms, &d)) return false;
    if (q->has_as_of_ms) {
        if (d != floor(d) || d < 0 || d > 9007199254740991.0) return false;
        q->as_of_ms = (uint64_t)d;
    }
    if (!req_int(doc, node, "stale_after_ms", 1000, 604800000, &d)) return false;
    q->stale_after_ms = (uint32_t)d;
    if (!req_enum(doc, node, "availability", QAVAILS, TBL_LEN(QAVAILS), &v)) return false;
    q->availability = (bot_quota_avail_t)v;
    if (!req_str(doc, node, "reason", q->reason, sizeof(q->reason), 40, true)) return false;
    /* shared_with: 0..4 unique agent ids, never the stats agent itself */
    int32_t sw = bj_obj_get(doc, node, "shared_with");
    if (sw < 0 || doc->nodes[sw].type != BJ_ARR) return false;
    q->shared_count = 0;
    for (int32_t i = doc->nodes[sw].child; i != -1; i = doc->nodes[i].next) {
        if (q->shared_count >= BOT_AGENT_COUNT) return false;
        int av;
        if (!enum_lookup(AGENTS, TBL_LEN(AGENTS), &doc->nodes[i], &av)) return false;
        if ((bot_agent_id_t)av == stats_agent) return false;
        for (uint8_t k = 0; k < q->shared_count; k++) {
            if (q->shared_with[k] == (bot_agent_id_t)av) return false;
        }
        q->shared_with[q->shared_count++] = (bot_agent_id_t)av;
    }
    /* semantic: no fabricated numbers for unknown/unlimited/unavailable */
    if (q->kind == BOT_QKIND_UNKNOWN || q->kind == BOT_QKIND_UNLIMITED) {
        if (q->has_used || q->has_limit || q->has_remaining || q->has_used_pct) return false;
        if (q->unit != BOT_QUNIT_NONE) return false;
    }
    if (q->quality == BOT_MQUAL_UNAVAILABLE) {
        if (q->has_used || q->has_limit || q->has_remaining || q->has_used_pct) return false;
    }
    if (q->kind == BOT_QKIND_CREDITS && q->unit != BOT_QUNIT_CREDIT) return false;
    if (q->kind == BOT_QKIND_SPEND_BUDGET && q->unit != BOT_QUNIT_USD_MICROS) return false;
    if (q->has_used && q->has_limit && q->has_used_pct) {
        if (q->limit == 0.0) return false;
        double expected = q->used / q->limit * 100.0;
        if (fabs(expected - q->used_pct) > 0.1) return false;
    }
    return true;
}

static bool dec_stats(const bj_doc_t *doc, int32_t body, bot_stats_t *s)
{
    static const char *const KEYS[] = {
        "agent_id", "selection_rev", "sent_at_ms", "scope", "metrics", "quotas", "sparkline"
    };
    static const char *const SCOPE_KEYS[] = {
        "kind", "timezone", "start_ms", "end_ms"
    };
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    int v;
    if (!req_enum(doc, body, "agent_id", AGENTS, TBL_LEN(AGENTS), &v)) return false;
    s->agent_id = (bot_agent_id_t)v;
    double d;
    if (!req_int(doc, body, "selection_rev", 0, 2147483647, &d)) return false;
    s->selection_rev = (uint32_t)d;
    if (!req_int(doc, body, "sent_at_ms", 0, 9007199254740991.0, &d)) return false;
    s->sent_at_ms=(uint64_t)d;
    int32_t scope = bj_obj_get(doc, body, "scope");
    if (scope < 0 || doc->nodes[scope].type != BJ_OBJ) return false;
    if (!no_extra_keys(doc, scope, SCOPE_KEYS, TBL_LEN(SCOPE_KEYS))) return false;
    int32_t kind = bj_obj_get(doc, scope, "kind");
    if (kind < 0) return false;
    if (str_eq(&doc->nodes[kind], "today")) s->scope_kind = 0;
    else if (str_eq(&doc->nodes[kind], "session")) s->scope_kind = 1;
    else return false;
    if (!req_str(doc, scope, "timezone", s->scope_timezone,
                 sizeof(s->scope_timezone), 64, true)) return false;
    if (!req_int(doc, scope, "start_ms", 0, 9007199254740991.0, &d)) return false;
    s->scope_start_ms = (uint64_t)d;
    if (!req_int(doc, scope, "end_ms", 0, 9007199254740991.0, &d)) return false;
    s->scope_end_ms = (uint64_t)d;
    /* semantic: valid statistics interval (IANA tz validation is host-side) */
    if (s->scope_end_ms <= s->scope_start_ms) return false;

    int32_t metrics = bj_obj_get(doc, body, "metrics");
    if (metrics < 0 || doc->nodes[metrics].type != BJ_ARR) return false;
    s->metric_count = 0;
    for (int32_t i = doc->nodes[metrics].child; i != -1; i = doc->nodes[i].next) {
        if (s->metric_count >= 6) return false;
        if (doc->nodes[i].type != BJ_OBJ) return false;
        bot_metric_t *m = &s->metrics[s->metric_count];
        if (!dec_metric(doc, i, m)) return false;
        for (uint8_t k = 0; k < s->metric_count; k++) {
            if (s->metrics[k].key == m->key) return false; /* unique metric key */
        }
        s->metric_count++;
    }

    int32_t quotas = bj_obj_get(doc, body, "quotas");
    if (quotas < 0 || doc->nodes[quotas].type != BJ_ARR) return false;
    s->quota_count = 0;
    for (int32_t i = doc->nodes[quotas].child; i != -1; i = doc->nodes[i].next) {
        if (s->quota_count >= 2) return false;
        if (doc->nodes[i].type != BJ_OBJ) return false;
        bot_quota_t *q = &s->quotas[s->quota_count];
        if (!dec_quota(doc, i, q, s->agent_id)) return false;
        for (uint8_t k = 0; k < s->quota_count; k++) {
            if (strncmp(s->quotas[k].id, q->id, sizeof(q->id)) == 0) return false;
        }
        s->quota_count++;
    }

    int32_t spark = bj_obj_get(doc, body, "sparkline");
    if (spark < 0 || doc->nodes[spark].type != BJ_ARR) return false;
    s->sparkline_count = 0;
    for (int32_t i = doc->nodes[spark].child; i != -1; i = doc->nodes[i].next) {
        if (s->sparkline_count >= 24) return false;
        /* null bucket = uncovered segment, leave empty (never interpolate) */
        if (doc->nodes[i].type == BJ_NULL) {
            s->sparkline[s->sparkline_count++] = NAN;
        } else if (doc->nodes[i].type == BJ_NUM) {
            s->sparkline[s->sparkline_count++] = doc->nodes[i].num;
        } else {
            return false;
        }
    }
    return true;
}

static bool dec_ack(const bj_doc_t *doc, int32_t body, bot_ack_t *a)
{
    static const char *const KEYS[] = {
        "mode", "action_id", "status", "selected_agent", "selection_rev", "reason"
    };
    static const enum_entry_t STATUSES[] = {
        { "accepted", 0 }, { "rejected", 1 },
    };
    static const enum_entry_t AREASONS[] = {
        { "ok", 0 }, { "conflict", 1 }, { "offline", 2 }, { "unsupported", 3 },
        { "rate_limited", 4 }, { "invalid", 5 }, { "internal_error", 6 },
    };
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    if (!req_str(doc, body, "action_id", a->action_id,
                 sizeof(a->action_id), 32, true)) return false;
    if (!is_hex32(a->action_id)) return false;
    int v;
    if (!req_enum(doc, body, "status", STATUSES, TBL_LEN(STATUSES), &v)) return false;
    a->status = (uint8_t)v;
    if (!req_enum(doc, body, "selected_agent", AGENTS, TBL_LEN(AGENTS), &v)) return false;
    a->selected_agent = (bot_agent_id_t)v;
    if (!req_enum(doc, body, "mode", MODES, TBL_LEN(MODES), &v)) return false;
    a->mode = (bot_selection_mode_t)v;
    double d;
    if (!req_int(doc, body, "selection_rev", 0, 2147483647, &d)) return false;
    a->selection_rev = (uint32_t)d;
    if (!req_enum(doc, body, "reason", AREASONS, TBL_LEN(AREASONS), &v)) return false;
    a->reason = (uint8_t)v;
    /* semantic: accepted <-> reason ok */
    if ((a->status == 0) != (a->reason == 0)) return false;
    return true;
}

static bool dec_notice(const bj_doc_t *doc, int32_t body, bot_notice_t *n)
{
    static const char *const KEYS[] = {
        "notice_id", "agent_id", "run_id", "kind", "label", "expires_in_ms"
    };
    static const enum_entry_t KINDS[] = {
        { "waiting", 0 }, { "done", 1 }, { "error", 2 }, { "quota_low", 3 },
    };
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    if (!req_str(doc, body, "notice_id", n->notice_id,
                 sizeof(n->notice_id), 64, true)) return false;
    int v;
    if (!req_enum(doc, body, "agent_id", AGENTS, TBL_LEN(AGENTS), &v)) return false;
    n->agent_id = (bot_agent_id_t)v;
    if (!dec_nullable_str(doc, body, "run_id", &n->has_run_id,
                          n->run_id, sizeof(n->run_id), BOT_RUN_ID_MAX)) return false;
    if (!req_enum(doc, body, "kind", KINDS, TBL_LEN(KINDS), &v)) return false;
    n->kind = (uint8_t)v;
    if (!req_str(doc, body, "label", n->label, sizeof(n->label), 40, true)) return false;
    double d;
    if (!req_int(doc, body, "expires_in_ms", 500, 10000, &d)) return false;
    n->expires_in_ms = (uint32_t)d;
    return true;
}

/* USBv3 usage is independent of business selection. All fields required. */
static const enum_entry_t USUB[]={{"current",0},{"all",1},{"codex",2},{"cursor",3},{"hermes",4},{"workbuddy",5}};
static const enum_entry_t UPER[]={{"today",0},{"7d",1},{"30d",2}};
static bool unum(const bj_doc_t *d,int o,const char *k,bot_usage_number_t *n,bool pct) {
 if(!opt_num_nullable(d,o,k,&n->has,&n->value))return false;

 return !n->has || (n->value>=0 && n->value<=(pct?100:9007199254740991.0) && (pct || floor(n->value)==n->value));
}
static bool uview(const bj_doc_t *d,int o,bot_usage_view_t *v,const char *rev) {
 int x;double n;
 if(!req_enum(d,o,"subject",USUB,6,&x))return false;
v->subject=x;
 if(!req_enum(d,o,"period",UPER,3,&x))return false;
v->period=x;
 if(!req_int(d,o,"page",0,33,&n))return false;
v->page=n;
 if(!req_int(d,o,rev,0,2147483647,&n))return false;
v->usage_rev=n;return true;

}
static bool dec_usage_ack(const bj_doc_t *d,int o,bot_usage_ack_t *a,bool request) {
 static const char *const ak[]={"request_id","status","reason","usage_rev","subject","period","page"};
 static const char *const rk[]={"request_id","expected_usage_rev","subject","period","page"};
 if(!no_extra_keys(d,o,request?rk:ak,request?5:7) || !req_str(d,o,"request_id",a->request_id,33,32,true) || !is_hex32(a->request_id) || !uview(d,o,&a->view,request?"expected_usage_rev":"usage_rev"))return false;

 if(request)return true;

 static const enum_entry_t st[]={{"accepted",0},{"rejected",1}},re[]={{"ok",0},{"conflict",1}};int x;
 if(!req_enum(d,o,"status",st,2,&x))return false;
a->status=x;
 if(!req_enum(d,o,"reason",re,2,&x))return false;
a->reason=x;return a->status==a->reason;

}
static bool dec_usage(const bj_doc_t *d,int o,bot_usage_t *u) {
 static const char *const keys[]={"usage_rev","subject","period","page","data_rev","host_now_ms","as_of_ms","stale","status","summary","agents","quotas","quota_total","models","model_total","history"};
 if(!no_extra_keys(d,o,keys,16) || !uview(d,o,&u->view,"usage_rev"))return false;

 double n;int x;
 if(!req_int(d,o,"data_rev",0,2147483647,&n))return false;
u->data_rev=n;
 if(!req_int(d,o,"host_now_ms",0,9007199254740991.0,&n))return false;
 u->host_now_ms=(uint64_t)n;
 if(!unum(d,o,"as_of_ms",&u->as_of_ms,false) || !bj_get_bool(d,o,"stale",&u->stale))return false;

 static const enum_entry_t status[]={{"ready",0},{"starting",1},{"error",2},{"unavailable",3}};
 if(!req_enum(d,o,"status",status,4,&x))return false;
u->status=x;
 int b=bj_obj_get(d,o,"summary");if(b<0 || d->nodes[b].type!=BJ_OBJ)return false;

 static const char *const sk[]={"agent_id","total","input","output","cache_read","cache_write","cost_micros","cost_currency","coverage","cost_coverage","cost_source"};
 if(!no_extra_keys(d,b,sk,11) || !req_enum(d,b,"agent_id",USUB+1,5,&x))return false;
u->summary_agent=x;
 if(!unum(d,b,"total",&u->total,false)||!unum(d,b,"input",&u->input,false)||!unum(d,b,"output",&u->output,false)||!unum(d,b,"cache_read",&u->cache_read,false)||!unum(d,b,"cache_write",&u->cache_write,false)||!unum(d,b,"cost_micros",&u->cost_micros,false))return false;

 if(bj_is_null(d,b,"cost_currency"))u->cost_currency[0]=0;
 else {if(!req_str(d,b,"cost_currency",u->cost_currency,4,3,true)||strlen(u->cost_currency)!=3)return false;
for(int i=0;i<3;i++)if(u->cost_currency[i]<'A'||u->cost_currency[i]>'Z')return false;
}
 if(u->cost_micros.has != (u->cost_currency[0]!=0))return false;

 static const enum_entry_t cov[]={{"complete",0},{"partial",1},{"unknown",2}};
 if(!req_enum(d,b,"coverage",cov,3,&x))return false;
u->coverage=x;
 if(!req_enum(d,b,"cost_coverage",cov,3,&x))return false;
u->cost_coverage=x;
 if(!req_str(d,b,"cost_source",u->cost_source,41,40,true))return false;
if(!req_int(d,o,"quota_total",0,32,&n))return false;
u->quota_total=n;
 if(!req_int(d,o,"model_total",0,100,&n))return false;
u->model_total=n;
 const char *arrays[]={"agents","quotas","models","history"};int caps[]={4,3,3,30};
 for(int a=0;a<4;a++) {int ar=bj_obj_get(d,o,arrays[a]);if(ar<0||d->nodes[ar].type!=BJ_ARR)return false;
int count=0;
 for(int r=d->nodes[ar].child;r!=-1;r=d->nodes[r].next) {if(count>=caps[a]||d->nodes[r].type!=BJ_OBJ)return false;

 if(a==0) {static const char *const k[]={"id","total","used_pct","available"};static const int order[]={BOT_AGENT_CODEX,BOT_AGENT_CURSOR,BOT_AGENT_HERMES,BOT_AGENT_WORKBUDDY};bot_usage_agent_t *v=&u->agents[count];
 if(!no_extra_keys(d,r,k,4)||!req_enum(d,r,"id",AGENTS,4,&x)||x!=order[count])return false;
v->id=x;
 if(!unum(d,r,"total",&v->total,false)||!unum(d,r,"used_pct",&v->used_pct,true)||!bj_get_bool(d,r,"available",&v->available))return false;

 }else if(a==1) {static const char *const k[]={"id","agent_id","label","used_pct","reset_ms","stale","availability"};static const enum_entry_t av[]={{"available",0},{"unavailable",1},{"needs_auth",2},{"error",3}};bot_usage_quota_t *v=&u->quotas[count];
 if(!no_extra_keys(d,r,k,7)||!req_str(d,r,"id",v->id,33,32,true)||!is_hex32(v->id)||!req_str(d,r,"label",v->label,25,24,true)||!req_enum(d,r,"agent_id",AGENTS,4,&x))return false;
v->agent_id=x;
 if(!unum(d,r,"used_pct",&v->used_pct,true)||!unum(d,r,"reset_ms",&v->reset_ms,false)||!bj_get_bool(d,r,"stale",&v->stale)||!req_enum(d,r,"availability",av,4,&x))return false;
v->availability=x;
 }else if(a==2) {static const char *const k[]={"label","total"};bot_usage_model_t *v=&u->models[count];if(!no_extra_keys(d,r,k,2)||!req_str(d,r,"label",v->label,33,32,true)||!unum(d,r,"total",&v->total,false))return false;

 }else {static const char *const k[]={"day","total"};bot_usage_day_t *v=&u->history[count];if(!no_extra_keys(d,r,k,2)||!req_str(d,r,"day",v->day,11,10,true)||strlen(v->day)!=10||!unum(d,r,"total",&v->total,false))return false;
for(int i=0;i<10;i++)if(i==4||i==7){if(v->day[i]!='-')return false;
}else if(v->day[i]<'0'||v->day[i]>'9')return false;
}
 count++;}
 if(a==0&&count!=4)return false;
if(a==1) {u->quota_count=count;}
 if(a==2) {u->model_count=count;}
 if(a==3) {u->history_count=count;}
 }return u->quota_count<=u->quota_total && u->model_count<=u->model_total;
}

static bool dec_hello(const bj_doc_t *doc, int32_t body, bot_msg_t *m)
{
    /* Device->Host, decoded only for loopback tests. Shape check only. */
    static const char *const KEYS[] = {
        "device_id", "boot_id", "firmware", "display", "min_version",
        "max_version", "handshake_id"
    };
    (void)m;
    if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
    char tmp[65];
    if (!req_str(doc, body, "device_id", tmp, sizeof(tmp), 64, true)) return false;
    if (!req_str(doc, body, "boot_id", tmp, sizeof(tmp), 32, true)) return false;
    if (!is_hex32(tmp)) return false;
    if (!req_str(doc, body, "firmware", tmp, sizeof(tmp), 32, true)) return false;
    if (!req_str(doc, body, "handshake_id", tmp, sizeof(tmp), 32, true)) return false;
    if (!is_hex32(tmp)) return false;
    int32_t disp = bj_obj_get(doc, body, "display");
    if (disp < 0 || doc->nodes[disp].type != BJ_OBJ) return false;
    static const char *const display_keys[]={"width","height"};
    if(!no_extra_keys(doc,disp,display_keys,2))return false;
    double d;
    if (!req_int(doc, disp, "width", 466, 466, &d)) return false;
    if (!req_int(doc, disp, "height", 466, 466, &d)) return false;
    if (!req_int(doc, body, "min_version", 3, 3, &d)) return false;
    if (!req_int(doc, body, "max_version", 3, 3, &d)) return false;
    return true;
}

/* ---- envelope + dispatch ------------------------------------------------ */

static bool decode_message(bj_doc_t *doc, int32_t root, bot_msg_t *out)
{
    static const char *const ENVELOPE_KEYS[] = { "v", "type", "link_id", "seq", "body" };
    memset(out, 0, sizeof(*out));
    if (!no_extra_keys(doc, root, ENVELOPE_KEYS, TBL_LEN(ENVELOPE_KEYS))) return false;

    double d;
    if (!req_int(doc, root, "v", 3, 3, &d)) return false;

    int32_t type_node = bj_obj_get(doc, root, "type");
    if (type_node < 0 || doc->nodes[type_node].type != BJ_STR) return false;
    const bj_node_t *tn = &doc->nodes[type_node];
    struct {
        const char *name;
        bot_msg_type_t t;
    } types[] = {
        {"usage",BOT_MSG_USAGE},{"usage_ack",BOT_MSG_USAGE_ACK},{"usage_request",BOT_MSG_USAGE_REQUEST}, { "selection", BOT_MSG_SELECTION }, { "welcome", BOT_MSG_WELCOME }, { "catalog", BOT_MSG_CATALOG },
        { "focus", BOT_MSG_FOCUS }, { "stats", BOT_MSG_STATS },
        { "ack", BOT_MSG_ACK }, { "notice", BOT_MSG_NOTICE },
        { "ping", BOT_MSG_PING }, { "pong", BOT_MSG_PONG }, { "action", BOT_MSG_ACTION }, { "hello", BOT_MSG_HELLO },
    };
    bool found = false;
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (str_eq(tn, types[i].name)) {
            out->type = types[i].t;
            found = true;
            break;
        }
    }
    if (!found) return false;

    /* link_id: null allowed ONLY for hello (schema allOf[10] + §2) */
    int32_t link = bj_obj_get(doc, root, "link_id");
    if (link < 0) return false;
    if (doc->nodes[link].type == BJ_NULL) {
        if (out->type != BOT_MSG_HELLO) return false;
        out->has_link_id = false;
    } else {
        if (!bj_str_copy(doc, link, out->link_id, sizeof(out->link_id))) return false;
        if (!is_hex32(out->link_id)) return false;
        out->has_link_id = true;
    }

    if (!req_int(doc, root, "seq", 0, 2147483647, &d)) return false;
    out->seq = (uint32_t)d;
    /* hello is the only seq=0 message */
    if ((out->seq == 0) != (out->type == BOT_MSG_HELLO)) return false;

    int32_t body = bj_obj_get(doc, root, "body");
    if (body < 0 || doc->nodes[body].type != BJ_OBJ) return false;

    switch (out->type) {
    case BOT_MSG_SELECTION: {
        static const char *const keys[]={"mode","selected_agent","selection_rev"};
        int v;
        if(!no_extra_keys(doc,body,keys,3)) return false;
        if(!req_enum(doc,body,"mode",MODES,TBL_LEN(MODES),&v)) return false;
        out->body.selection.mode=(bot_selection_mode_t)v;
        if(!req_enum(doc,body,"selected_agent",AGENTS,TBL_LEN(AGENTS),&v)) return false;
        out->body.selection.selected_agent=(bot_agent_id_t)v;
        if(!req_int(doc,body,"selection_rev",0,2147483647,&d)) return false;
        out->body.selection.selection_rev=(uint32_t)d;
        return true;
    }
    case BOT_MSG_USAGE: return dec_usage(doc,body,&out->body.usage);
    case BOT_MSG_USAGE_ACK: return dec_usage_ack(doc,body,&out->body.usage_ack,false);
    case BOT_MSG_USAGE_REQUEST: return dec_usage_ack(doc,body,&out->body.usage_ack,true);
    case BOT_MSG_WELCOME: return dec_welcome(doc, body, &out->body.welcome);
    case BOT_MSG_CATALOG: return dec_catalog(doc, body, &out->body.catalog);
    case BOT_MSG_FOCUS: return dec_focus(doc, body, &out->body.focus);
    case BOT_MSG_STATS: return dec_stats(doc, body, &out->body.stats);
    case BOT_MSG_ACTION: {
        static const char *const keys[]={"action_id","kind","mode","agent_id","expected_selection_rev"};
        int v;char kind[16];bot_action_t *a=&out->body.action;
        if(!no_extra_keys(doc,body,keys,5))return false;
        if(!req_str(doc,body,"action_id",a->action_id,33,32,true) || !is_hex32(a->action_id))return false;
        if(!req_str(doc,body,"kind",kind,sizeof(kind),15,true) || strcmp(kind,"select"))return false;
        if(!req_enum(doc,body,"mode",MODES,TBL_LEN(MODES),&v))return false;
        a->mode=(bot_selection_mode_t)v;
        if(!req_enum(doc,body,"agent_id",AGENTS,TBL_LEN(AGENTS),&v))return false;
        a->agent_id=(bot_agent_id_t)v;
        if(!req_int(doc,body,"expected_selection_rev",0,2147483647,&d))return false;
        a->expected_selection_rev=(uint32_t)d;
        return true;
    }
    case BOT_MSG_ACK: return dec_ack(doc, body, &out->body.ack);
    case BOT_MSG_NOTICE: return dec_notice(doc, body, &out->body.notice);
    case BOT_MSG_PONG:
    case BOT_MSG_PING: {
        static const char *const KEYS[] = { "monotonic_ms" };
        if (!no_extra_keys(doc, body, KEYS, TBL_LEN(KEYS))) return false;
        if (!req_int(doc, body, "monotonic_ms", 0, 9007199254740991.0, &d)) return false;
        out->body.ping_monotonic_ms = (uint64_t)d;
        return true;
    }
    case BOT_MSG_HELLO: return dec_hello(doc, body, out);
    default: return false;
    }
}

/* ---- byte-fed frame state machine --------------------------------------- */

void bot_frame_init(bot_frame_parser_t *fp, bj_node_t *nodes, uint32_t node_cap,
                    char *str_pool, uint32_t str_cap)
{
    fp->state = BOT_FR_SEEK;
    fp->prefix_pos = 0;
    fp->line_len = 0;
    fp->overflow = false;
    fp->bad_lines = 0;
    fp->oversize_lines = 0;
    fp->decoded = 0;
    fp->nodes = nodes;
    fp->node_cap = node_cap;
    fp->str_pool = str_pool;
    fp->str_cap = str_cap;
    fp->last_json_err = BJ_OK;
}

static bot_frame_result_t finish_line(bot_frame_parser_t *fp, bot_msg_t *out)
{
    /* strip a single trailing CR (CRLF tolerance; CR not counted in budget) */
    uint16_t len = fp->line_len;
    if (len > 0 && fp->line[len - 1] == '\r') len--;
    fp->line[len] = '\0';
    if (len == 0) {
        return BOT_FRAME_NONE; /* "@bot \n" empty line: ignore quietly */
    }
    fp->last_json_err=BJ_OK;
    bj_init(&fp->doc, fp->nodes, fp->node_cap, fp->str_pool, fp->str_cap);
    int32_t root = bj_parse(&fp->doc, fp->line, len);
    if (root < 0) {
        fp->last_json_err = fp->doc.err;
        fp->bad_lines++;
        return BOT_FRAME_BAD_LINE;
    }
    if (!decode_message(&fp->doc, root, out)) {
        fp->bad_lines++;
        return BOT_FRAME_BAD_LINE;
    }
    fp->decoded++;
    return BOT_FRAME_MSG; /* tree released: next line re-initialises the pool */
}

bot_frame_result_t bot_frame_feed(bot_frame_parser_t *fp, uint8_t byte,
                                  bot_msg_t *out)
{
    switch (fp->state) {
    case BOT_FR_SEEK:
        if (byte == '@') {
            fp->state = BOT_FR_PREFIX;
            fp->prefix_pos = 1;
        }
        return BOT_FRAME_NONE;

    case BOT_FR_PREFIX:
        if (byte == (uint8_t)FRAME_PREFIX[fp->prefix_pos]) {
            fp->prefix_pos++;
            if (fp->prefix_pos >= FRAME_PREFIX_LEN) {
                fp->state = BOT_FR_LINE;
                fp->line_len = 0;
                fp->overflow = false;
            }
            return BOT_FRAME_NONE;
        }
        /* mismatch: this byte might itself start a new prefix */
        fp->state = BOT_FR_SEEK;
        if (byte == '@') {
            fp->state = BOT_FR_PREFIX;
            fp->prefix_pos = 1;
        }
        return BOT_FRAME_NONE;

    case BOT_FR_LINE:
        if (byte == '\n') {
            fp->state = BOT_FR_SEEK;
            return finish_line(fp, out);
        }
        if (fp->line_len >= BOT_FRAME_MAX_BYTES) {
            /* oversize line: drop everything until the next newline (§1) */
            fp->state = BOT_FR_SKIP;
            fp->oversize_lines++;
            fp->line_len = 0;
            return BOT_FRAME_OVERSIZE;
        }
        fp->line[fp->line_len++] = (char)byte;
        return BOT_FRAME_NONE;

    case BOT_FR_SKIP:
        if (byte == '\n') {
            fp->state = BOT_FR_SEEK;
        }
        return BOT_FRAME_NONE;

    default:
        fp->state = BOT_FR_SEEK;
        return BOT_FRAME_NONE;
    }
}

const char *bot_msg_type_name(bot_msg_type_t t)
{
    switch (t) {
    case BOT_MSG_WELCOME: return "welcome";
    case BOT_MSG_CATALOG: return "catalog";
    case BOT_MSG_FOCUS: return "focus";
    case BOT_MSG_STATS: return "stats";
    case BOT_MSG_USAGE: return "usage";case BOT_MSG_USAGE_ACK:return "usage_ack";case BOT_MSG_USAGE_REQUEST:return "usage_request";
    case BOT_MSG_ACTION: return "action";
    case BOT_MSG_ACK: return "ack";
    case BOT_MSG_NOTICE: return "notice";
    case BOT_MSG_PING: return "ping";
    case BOT_MSG_PONG: return "pong";
    case BOT_MSG_HELLO: return "hello";
    case BOT_MSG_SELECTION: return "selection";
    case BOT_MSG_NONE:
    default: return "none";
    }
}
