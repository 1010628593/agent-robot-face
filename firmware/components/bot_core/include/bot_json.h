/* bot_json.h — strict JSON parser over a fixed node pool (T05).
 *
 * Mirrors bridge/src/bot_bridge/models.py strict_load rules (contract authority:
 * docs/05_PROTOCOL.md §1/§10 and tools/validate_contracts.py):
 *   - reject NaN / Infinity / non-JSON numbers (1e999 overflow rejected)
 *   - reject duplicate keys within the same object
 *   - reject invalid UTF-8 (full validation, including across escapes)
 *   - reject nesting deeper than 12
 *   - reject array/scalar root for protocol frames (root must be object)
 *   - reject trailing garbage after the top-level value
 *
 * No heap: caller provides a node pool and a string pool. Strings are decoded
 * (escapes resolved) into the string pool. Zero dynamic allocation.
 */
#ifndef BOT_JSON_H
#define BOT_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOT_JSON_MAX_DEPTH 12

typedef enum {
    BJ_OBJ = 0,
    BJ_ARR,
    BJ_STR,
    BJ_NUM,
    BJ_BOOL,
    BJ_NULL
} bj_type_t;

typedef struct {
    bj_type_t type;
    /* object member key (decoded; valid when this node is a child of OBJ) */
    const char *key;
    uint16_t key_len;
    /* STR: decoded value slice in the string pool */
    const char *str;
    uint16_t str_len;
    /* NUM: value; NUM values that overflow double are rejected at parse time */
    double num;
    bool boolean;
    int32_t child; /* pool index of first child, -1 if none */
    int32_t next;  /* pool index of next sibling, -1 if none */
} bj_node_t;

typedef enum {
    BJ_OK = 0,
    BJ_ERR_SYNTAX,
    BJ_ERR_DEPTH,
    BJ_ERR_POOL,
    BJ_ERR_DUP_KEY,
    BJ_ERR_UTF8,
    BJ_ERR_NUMBER,
    BJ_ERR_ROOT,     /* root is not an object */
    BJ_ERR_TRAILING  /* garbage after top-level value */
} bj_err_t;

typedef struct {
    bj_node_t *nodes;
    uint32_t node_cap;
    uint32_t node_used;
    char *str_pool;
    uint32_t str_cap;
    uint32_t str_used;
    bj_err_t err;
    uint32_t err_pos; /* byte offset of the failure, for diagnostics */
} bj_doc_t;

void bj_init(bj_doc_t *doc, bj_node_t *nodes, uint32_t node_cap,
             char *str_pool, uint32_t str_cap);

/* Parse buf[0..len) as one complete strict JSON value. Returns root index
 * (>=0) on success, -1 on failure (doc->err/err_pos describe why). */
int32_t bj_parse(bj_doc_t *doc, const char *buf, uint32_t len);

/* Object member lookup by exact key. Returns child index or -1. */
int32_t bj_obj_get(const bj_doc_t *doc, int32_t obj, const char *key);

/* Convenience accessors (return false when the node is absent/wrong type). */
bool bj_get_str(const bj_doc_t *doc, int32_t obj, const char *key,
                const char **s, uint16_t *len);
bool bj_get_num(const bj_doc_t *doc, int32_t obj, const char *key, double *out);
bool bj_get_bool(const bj_doc_t *doc, int32_t obj, const char *key, bool *out);
bool bj_is_null(const bj_doc_t *doc, int32_t obj, const char *key);

/* Copy a decoded string node into out (NUL-terminated). Returns false when
 * the value is missing, not a string, or does not fit (out_cap-1). */
bool bj_str_copy(const bj_doc_t *doc, int32_t node, char *out, uint32_t out_cap);

const char *bj_err_name(bj_err_t err);

#ifdef __cplusplus
}
#endif

#endif /* BOT_JSON_H */
