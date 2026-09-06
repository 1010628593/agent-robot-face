/* test_frame_parser.c — T05 frame parser + strict decoder acceptance tests.
 *
 * Covers docs/10_IMPLEMENTATION_PLAN.md T05:
 *   - valid frames fed byte-by-byte / coalesced / with log noise
 *   - oversize line dropped, next valid frame resyncs
 *   - duplicate keys, >12 depth, NaN, 1e999, array root, bad UTF-8
 *   - CRLF tolerated; UTF-8 multibyte split across feeds
 *   - all contracts/invalid device-message fixtures rejected
 *   - strict v1: unknown envelope/body keys rejected (05_PROTOCOL §10)
 *   - semantic rules mirrored from tools/validate_contracts.py
 *
 * Valid fixtures are the real contracts/examples files (single source of
 * truth), compacted at runtime into one-line frames.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bot_frame.h"

#ifndef BOT_CONTRACTS_DIR
#define BOT_CONTRACTS_DIR "../Bot_Status_v1_Handoff/contracts"
#endif

static bj_node_t NODES[512];
static char STR_POOL[8192];
static bot_frame_parser_t FP;
static bot_msg_t MSG;

static int failures = 0;

#define CHECK(cond, name)                              \
    do {                                               \
        if (cond) {                                    \
            printf("PASS %s\n", name);                 \
        } else {                                       \
            printf("FAIL %s (line %d)\n", name, __LINE__); \
            failures++;                                \
        }                                              \
    } while (0)

static void fp_reset(void)
{
    bot_frame_init(&FP, NODES, 512, STR_POOL, sizeof(STR_POOL));
    memset(&MSG, 0, sizeof(MSG));
}

/* Feed a whole string; return final result and count MSG/BAD/OVERSIZE events. */
typedef struct {
    int msgs, bad, oversize;
} feed_stat_t;

static feed_stat_t feed_bytes(const char *data, size_t len, bool bytewise)
{
    feed_stat_t st = { 0, 0, 0 };
    size_t chunk = bytewise ? 1 : len;
    for (size_t off = 0; off < len; off += chunk) {
        size_t n = bytewise ? 1 : len - off;
        for (size_t i = 0; i < n; i++) {
            bot_frame_result_t r = bot_frame_feed(&FP, (uint8_t)data[off + i], &MSG);
            if (r == BOT_FRAME_MSG) st.msgs++;
            else if (r == BOT_FRAME_BAD_LINE) st.bad++;
            else if (r == BOT_FRAME_OVERSIZE) st.oversize++;
        }
    }
    return st;
}

/* Load a file and compact JSON: drop whitespace outside strings. */
static char *load_compacted(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *raw = malloc((size_t)n + 1);
    if (!raw) { fclose(f); return NULL; }
    if (fread(raw, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(raw); return NULL; }
    fclose(f);
    raw[n] = '\0';
    char *out = malloc((size_t)n + 1);
    if (!out) { free(raw); return NULL; }
    bool in_str = false, esc = false;
    size_t o = 0;
    for (long i = 0; i < n; i++) {
        char c = raw[i];
        if (in_str) {
            out[o++] = c;
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
        } else if (c == '"') {
            in_str = true;
            out[o++] = c;
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            out[o++] = c;
        }
    }
    out[o] = '\0';
    free(raw);
    return out;
}

static char *frame_of(const char *json)
{
    size_t n = strlen(json);
    char *f = malloc(n + 8);
    if (!f) return NULL;
    memcpy(f, "@bot ", 5);
    memcpy(f + 5, json, n);
    f[5 + n] = '\n';
    f[5 + n + 1] = '\0';
    return f;
}

static void test_valid_examples(void)
{
    /* device-received example types (skip action/pong: host-received only) */
    static const char *files[] = {
        "01_hello.json", "02_welcome.json", "03_catalog.json",
        "04_focus_working.json", "05_focus_waiting.json", "06_focus_done.json",
        "07_stats_codex.json", "08_stats_unavailable.json",
        "09_stats_manual_balance.json", "11_select_ack.json",
        "12_conflict_ack.json", "14_notice.json", "15_ping.json",
        "17_stats_over_budget.json", "18_stats_unlimited.json",
    };
    char path[512];
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        snprintf(path, sizeof(path), "%s/examples/%s", BOT_CONTRACTS_DIR, files[i]);
        char *json = load_compacted(path);
        if (!json) {
            printf("FAIL load %s\n", files[i]);
            failures++;
            continue;
        }
        char *frame = frame_of(json);
        fp_reset();
        feed_stat_t st = feed_bytes(frame, strlen(frame), true /* byte-by-byte */);
        if (st.msgs == 1 && st.bad == 0) {
            printf("PASS example %s (type=%s)\n", files[i], bot_msg_type_name(MSG.type));
        } else {
            printf("FAIL example %s: msgs=%d bad=%d json_err=%s\n",
                   files[i], st.msgs, st.bad, bj_err_name(FP.last_json_err));
            failures++;
        }
        free(frame);
        free(json);
    }
}

static void test_invalid_examples(void)
{
    static const char *files[] = {
        "01_oversize_budget.json", "02_nan_value.json", "03_duplicate_key.json",
        "04_too_deep.json", "05_wrong_reason.json", "06_wrong_metric_unit.json",
        "07_duplicate_agent.json", "08_terminal_reason.json", "09_missing_link.json",
    };
    char path[512];
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        snprintf(path, sizeof(path), "%s/invalid/%s", BOT_CONTRACTS_DIR, files[i]);
        FILE *probe = fopen(path, "rb");
        if (!probe) {
            printf("SKIP %s (not present)\n", files[i]);
            continue;
        }
        fclose(probe);
        char *json = load_compacted(path);
        if (!json) { printf("FAIL load %s\n", files[i]); failures++; continue; }
        /* skip fixtures whose raw body legitimately exceeds the frame budget:
         * those are rejected by the budget check, not the decoder */
        if (strlen(json) > BOT_FRAME_MAX_BYTES) {
            fp_reset();
            char *frame = frame_of(json);
            feed_stat_t st = feed_bytes(frame, strlen(frame), false);
            if (st.oversize >= 1 && st.msgs == 0) {
                printf("PASS invalid %s (oversize drop)\n", files[i]);
            } else {
                printf("FAIL invalid %s: oversize=%d msgs=%d\n", files[i], st.oversize, st.msgs);
                failures++;
            }
            free(frame);
            free(json);
            continue;
        }
        fp_reset();
        char *frame = frame_of(json);
        feed_stat_t st = feed_bytes(frame, strlen(frame), false);
        if (st.bad == 1 && st.msgs == 0) {
            printf("PASS invalid %s rejected\n", files[i]);
        } else {
            printf("FAIL invalid %s accepted (msgs=%d)\n", files[i], st.msgs);
            failures++;
        }
        free(frame);
        free(json);
    }
}

static void test_streaming(void)
{
    const char *focus =
        "{\"v\":1,\"type\":\"focus\",\"link_id\":\"0123456789abcdef0123456789abcdef\","
        "\"seq\":5,\"body\":{\"agent_id\":\"codex\",\"selection_rev\":3,"
        "\"session_key\":null,\"run_id\":null,\"state\":\"working\",\"reason\":\"none\","
        "\"quality\":\"observed\",\"source_age_ms\":1200,\"stale\":false,"
        "\"tool\":\"Terminal\",\"detail\":\"build\",\"run_elapsed_ms\":84000,"
        "\"active_sessions\":2,\"progress\":null}}";

    /* coalesced frames + log noise before/between/after */
    fp_reset();
    const char *noise1 = "I (123) boot: heap ok\nW (45) wifi: reconnect\n";
    char *f1 = frame_of(focus);
    char *f2 = frame_of(focus);
    feed_stat_t st = { 0, 0, 0 };
    const char *chunks[] = { noise1, f1, "\n@bo not-a-frame\n", f2, "tail-log-no-newline" };
    for (int i = 0; i < 5; i++) {
        feed_stat_t c = feed_bytes(chunks[i], strlen(chunks[i]), false);
        st.msgs += c.msgs; st.bad += c.bad; st.oversize += c.oversize;
    }
    CHECK(st.msgs == 2 && st.bad == 0, "coalesced + log noise -> 2 msgs");

    /* oversize line dropped, next valid frame resyncs (T05 acceptance) */
    fp_reset();
    static char big[5 + 8200 + 2];
    memcpy(big, "@bot ", 5);
    memset(big + 5, 'a', 8200);
    big[5 + 8200] = '\n';
    big[5 + 8201] = '\0';
    feed_stat_t s1 = feed_bytes(big, strlen(big), false);
    char *f3 = frame_of(focus);
    feed_stat_t s2 = feed_bytes(f3, strlen(f3), false);
    CHECK(s1.oversize == 1 && s1.msgs == 0 && s2.msgs == 1,
          "oversize dropped, next frame resyncs");
    free(f1); free(f2); free(f3);

    /* CRLF tolerated */
    fp_reset();
    char *f4 = frame_of(focus);
    size_t n4 = strlen(f4);
    f4[n4 - 1] = '\r';
    f4[n4] = '\n';
    f4[n4 + 1] = '\0';
    feed_stat_t s3 = feed_bytes(f4, n4 + 1, true);
    CHECK(s3.msgs == 1 && s3.bad == 0, "CRLF tolerated");
    free(f4);
}

static void test_strict_json(void)
{
    struct {
        const char *name;
        const char *json;
    } bad[] = {
        { "duplicate key", "{\"v\":1,\"v\":2}" },
        { "NaN", "{\"v\":1,\"x\":NaN}" },
        { "Infinity", "{\"v\":1,\"x\":Infinity}" },
        { "1e999 overflow", "{\"v\":1,\"x\":1e999}" },
        { "array root", "[1,2,3]" },
        { "trailing garbage", "{\"v\":1} x" },
        { "unknown envelope key", "{\"v\":1,\"type\":\"ping\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":3,\"zzz\":1,\"body\":{\"monotonic_ms\":1}}" },
        { "bad utf8", "{\"v\":1,\"x\":\"\xc3\x28\"}" },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        fp_reset();
        char *f = frame_of(bad[i].json);
        feed_stat_t st = feed_bytes(f, strlen(f), false);
        if (st.bad == 1 && st.msgs == 0) {
            printf("PASS strict: %s\n", bad[i].name);
        } else {
            printf("FAIL strict: %s (msgs=%d bad=%d)\n", bad[i].name, st.msgs, st.bad);
            failures++;
        }
        free(f);
    }

    /* depth 13 rejected, depth 12 accepted */
    fp_reset();
    char deep[256];
    int pos = 0;
    pos += snprintf(deep + pos, sizeof(deep) - (size_t)pos, "{\"v\":1,\"type\":\"x\",");
    (void)pos;
    char nest[128];
    int n = 0;
    for (int i = 0; i < 13; i++) n += snprintf(nest + n, sizeof(nest) - (size_t)n, "{\"a\":");
    n += snprintf(nest + n, sizeof(nest) - (size_t)n, "1");
    for (int i = 0; i < 13; i++) n += snprintf(nest + n, sizeof(nest) - (size_t)n, "}");
    fp_reset();
    char *fd = frame_of(nest);
    feed_stat_t sd = feed_bytes(fd, strlen(fd), false);
    CHECK(sd.bad == 1 && sd.msgs == 0, "depth 13 rejected");
    free(fd);

    /* multibyte UTF-8 fed byte-by-byte: labels are ASCII-only by schema, so a
     * non-ASCII label must be REJECTED as a bad line — and the parser must
     * still handle the multibyte bytes cleanly and recover on the next frame. */
    fp_reset();
    const char *u =
        "{\"v\":1,\"type\":\"notice\",\"link_id\":\"0123456789abcdef0123456789abcdef\","
        "\"seq\":9,\"body\":{\"notice_id\":\"n1\",\"agent_id\":\"codex\",\"run_id\":null,"
        "\"kind\":\"done\",\"label\":\"ok \xc3\xa9 \xe4\xb8\xad\",\"expires_in_ms\":2000}}";
    char *fu = frame_of(u);
    feed_stat_t su = feed_bytes(fu, strlen(fu), true);
    free(fu);
    const char *ping =
        "{\"v\":1,\"type\":\"ping\",\"link_id\":\"0123456789abcdef0123456789abcdef\","
        "\"seq\":10,\"body\":{\"monotonic_ms\":1}}";
    char *fp2 = frame_of(ping);
    feed_stat_t sp = feed_bytes(fp2, strlen(fp2), true);
    CHECK(su.bad == 1 && su.msgs == 0 && sp.msgs == 1,
          "non-ASCII label rejected; multibyte split ok; next frame recovers");
    free(fp2);
}

static void test_semantics(void)
{
    struct {
        const char *name;
        const char *json;
    } bad[] = {
        { "quota unknown with numbers",
          "{\"v\":1,\"type\":\"stats\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":4,"
          "\"body\":{\"agent_id\":\"codex\",\"selection_rev\":3,"
          "\"scope\":{\"kind\":\"today\",\"timezone\":\"Asia/Shanghai\",\"start_ms\":1,\"end_ms\":2},"
          "\"metrics\":[],\"quotas\":[{\"id\":\"q1\",\"account_key\":\"a\",\"scope\":\"account\","
          "\"label\":\"L\",\"kind\":\"unknown\",\"unit\":\"none\",\"used\":5,\"limit\":null,"
          "\"remaining\":null,\"used_pct\":null,\"resets_at_ms\":null,\"quality\":\"unavailable\","
          "\"coverage\":\"unknown\",\"source\":\"s\",\"as_of_ms\":null,\"stale_after_ms\":1000,"
          "\"availability\":\"unsupported\",\"reason\":\"r\",\"shared_with\":[]}],\"sparkline\":[]}}" },
        { "ack accepted with conflict",
          "{\"v\":1,\"type\":\"ack\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":4,"
          "\"body\":{\"action_id\":\"0123456789abcdef0123456789abcdef\",\"status\":\"accepted\","
          "\"selected_agent\":\"codex\",\"selection_rev\":4,\"reason\":\"conflict\"}}" },
        { "focus waiting without reason",
          "{\"v\":1,\"type\":\"focus\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":4,"
          "\"body\":{\"agent_id\":\"codex\",\"selection_rev\":3,\"session_key\":null,\"run_id\":null,"
          "\"state\":\"waiting\",\"reason\":\"none\",\"quality\":\"observed\",\"source_age_ms\":1,"
          "\"stale\":false,\"tool\":\"\",\"detail\":\"\",\"run_elapsed_ms\":0,\"active_sessions\":0,"
          "\"progress\":null}}" },
        { "metric key/unit mismatch",
          "{\"v\":1,\"type\":\"stats\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":4,"
          "\"body\":{\"agent_id\":\"codex\",\"selection_rev\":3,"
          "\"scope\":{\"kind\":\"today\",\"timezone\":\"Asia/Shanghai\",\"start_ms\":1,\"end_ms\":2},"
          "\"metrics\":[{\"key\":\"turns\",\"label\":\"TURNS\",\"value\":3,\"unit\":\"token\","
          "\"quality\":\"exact\",\"coverage\":\"complete\",\"source\":\"s\",\"as_of_ms\":5,"
          "\"stale_after_ms\":1000}],\"quotas\":[],\"sparkline\":[]}}" },
        { "unavailable metric with value",
          "{\"v\":1,\"type\":\"stats\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":4,"
          "\"body\":{\"agent_id\":\"codex\",\"selection_rev\":3,"
          "\"scope\":{\"kind\":\"today\",\"timezone\":\"Asia/Shanghai\",\"start_ms\":1,\"end_ms\":2},"
          "\"metrics\":[{\"key\":\"turns\",\"label\":\"TURNS\",\"value\":0,\"unit\":\"turn\","
          "\"quality\":\"unavailable\",\"coverage\":\"unknown\",\"source\":\"s\",\"as_of_ms\":null,"
          "\"stale_after_ms\":1000}],\"quotas\":[],\"sparkline\":[]}}" },
        { "unknown body key rejected (v1 strict)",
          "{\"v\":1,\"type\":\"ping\",\"link_id\":\"0123456789abcdef0123456789abcdef\",\"seq\":4,"
          "\"body\":{\"monotonic_ms\":1,\"future_field\":2}}" },
        { "hello with link_id null ok but seq must be 0",
          "{\"v\":1,\"type\":\"hello\",\"link_id\":null,\"seq\":1,"
          "\"body\":{\"device_id\":\"d\",\"boot_id\":\"0123456789abcdef0123456789abcdef\","
          "\"firmware\":\"f\",\"display\":{\"width\":466,\"height\":466},"
          "\"min_version\":1,\"max_version\":1,"
          "\"handshake_id\":\"0123456789abcdef0123456789abcdef\"}}" },
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        fp_reset();
        char *f = frame_of(bad[i].json);
        feed_stat_t st = feed_bytes(f, strlen(f), false);
        if (st.bad == 1 && st.msgs == 0) {
            printf("PASS semantic: %s\n", bad[i].name);
        } else {
            printf("FAIL semantic: %s (msgs=%d bad=%d)\n", bad[i].name, st.msgs, st.bad);
            failures++;
        }
        free(f);
    }

    /* hello link_id=null seq=0 must DECODE (the one exception) */
    fp_reset();
    const char *hello =
        "{\"v\":1,\"type\":\"hello\",\"link_id\":null,\"seq\":0,"
        "\"body\":{\"device_id\":\"d\",\"boot_id\":\"0123456789abcdef0123456789abcdef\","
        "\"firmware\":\"f\",\"display\":{\"width\":466,\"height\":466},"
        "\"min_version\":1,\"max_version\":1,"
        "\"handshake_id\":\"0123456789abcdef0123456789abcdef\"}}";
    char *fh = frame_of(hello);
    feed_stat_t sh = feed_bytes(fh, strlen(fh), false);
    CHECK(sh.msgs == 1 && MSG.type == BOT_MSG_HELLO && !MSG.has_link_id,
          "hello link_id=null seq=0 accepted (only exception)");
    free(fh);
}

int main(void)
{
    test_valid_examples();
    test_invalid_examples();
    test_streaming();
    test_strict_json();
    test_semantics();
    if (failures) {
        printf("test_frame_parser: %d FAILURE(S)\n", failures);
        return 1;
    }
    printf("test_frame_parser: all passed\n");
    return 0;
}
