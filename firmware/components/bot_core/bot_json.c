/* bot_json.c — strict JSON parser over a fixed node pool (T05).
 * See bot_json.h for the contract rules. Recursive descent, no heap.
 */
#include "bot_json.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bj_doc_t *doc;
    const char *buf;
    uint32_t len;
    uint32_t pos;
    uint32_t depth;
} bj_p;

void bj_init(bj_doc_t *doc, bj_node_t *nodes, uint32_t node_cap,
             char *str_pool, uint32_t str_cap)
{
    doc->nodes = nodes;
    doc->node_cap = node_cap;
    doc->node_used = 0;
    doc->str_pool = str_pool;
    doc->str_cap = str_cap;
    doc->str_used = 0;
    doc->err = BJ_OK;
    doc->err_pos = 0;
}

static bj_node_t *alloc_node(bj_p *p)
{
    if (p->doc->node_used >= p->doc->node_cap) {
        p->doc->err = BJ_ERR_POOL;
        return NULL;
    }
    bj_node_t *n = &p->doc->nodes[p->doc->node_used++];
    memset(n, 0, sizeof(*n));
    n->child = -1;
    n->next = -1;
    return n;
}

static char *alloc_str(bj_p *p, uint32_t len)
{
    if (p->doc->str_used + len > p->doc->str_cap) {
        p->doc->err = BJ_ERR_POOL;
        return NULL;
    }
    char *s = &p->doc->str_pool[p->doc->str_used];
    p->doc->str_used += len;
    return s;
}

static void skip_ws(bj_p *p)
{
    while (p->pos < p->len) {
        char c = p->buf[p->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        p->pos++;
    }
}

static char peek(bj_p *p)
{
    return p->pos < p->len ? p->buf[p->pos] : '\0';
}

static bool fail(bj_p *p, bj_err_t err)
{
    if (p->doc->err == BJ_OK) {
        p->doc->err = err;
        p->doc->err_pos = p->pos;
    }
    return false;
}

/* ---- UTF-8 validation -------------------------------------------------- */

/* Validate one UTF-8 sequence starting at buf[*pos]; advance past it.
 * Rejects overlongs, surrogates, >U+10FFFF, stray continuations. */
static bool utf8_step(bj_p *p)
{
    const uint8_t *b = (const uint8_t *)p->buf;
    uint32_t i = p->pos;
    uint8_t c = b[i];
    if (c < 0x80) {
        p->pos++;
        return true;
    }
    uint32_t need;
    uint32_t cp;
    if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1F; if (cp < 2) return fail(p, BJ_ERR_UTF8); }
    else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07; if (cp > 4) return fail(p, BJ_ERR_UTF8); }
    else return fail(p, BJ_ERR_UTF8);
    if (i + need >= p->len) return fail(p, BJ_ERR_UTF8);
    for (uint32_t k = 1; k <= need; k++) {
        uint8_t cc = b[i + k];
        if ((cc & 0xC0) != 0x80) return fail(p, BJ_ERR_UTF8);
        cp = (cp << 6) | (cc & 0x3F);
    }
    /* overlong / surrogate / out of range checks */
    if ((need == 1 && cp < 0x80) || (need == 2 && cp < 0x800) ||
        (need == 3 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF) ||
        cp > 0x10FFFF) {
        return fail(p, BJ_ERR_UTF8);
    }
    p->pos += need + 1;
    return true;
}

/* ---- string parsing (with escape decoding into the string pool) -------- */

static int hex4(bj_p *p, uint32_t *out)
{
    if (p->pos + 4 > p->len) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p->buf[p->pos++];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

static uint32_t utf8_encode(uint32_t cp, char out[4])
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* Parse a JSON string starting at '"' and decode it into the string pool.
 * On success *out points into the pool (*out_len bytes, not NUL-terminated). */
static bool parse_string(bj_p *p, const char **out, uint16_t *out_len)
{
    if (peek(p) != '"') return fail(p, BJ_ERR_SYNTAX);
    p->pos++;
    /* worst case decoded length == raw length; reserve progressively */
    uint32_t start = p->doc->str_used;
    for (;;) {
        if (p->pos >= p->len) return fail(p, BJ_ERR_SYNTAX);
        char c = p->buf[p->pos];
        if (c == '"') {
            p->pos++;
            *out = &p->doc->str_pool[start];
            *out_len = (uint16_t)(p->doc->str_used - start);
            return true;
        }
        if ((uint8_t)c < 0x20) return fail(p, BJ_ERR_SYNTAX); /* raw control */
        if (c == '\\') {
            p->pos++;
            if (p->pos >= p->len) return fail(p, BJ_ERR_SYNTAX);
            char e = p->buf[p->pos++];
            char chunk[4];
            uint32_t chunk_len = 0;
            switch (e) {
            case '"': chunk[0] = '"'; chunk_len = 1; break;
            case '\\': chunk[0] = '\\'; chunk_len = 1; break;
            case '/': chunk[0] = '/'; chunk_len = 1; break;
            case 'b': chunk[0] = '\b'; chunk_len = 1; break;
            case 'f': chunk[0] = '\f'; chunk_len = 1; break;
            case 'n': chunk[0] = '\n'; chunk_len = 1; break;
            case 'r': chunk[0] = '\r'; chunk_len = 1; break;
            case 't': chunk[0] = '\t'; chunk_len = 1; break;
            case 'u': {
                uint32_t cp;
                if (!hex4(p, &cp)) return fail(p, BJ_ERR_SYNTAX);
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    /* high surrogate: require \uDC00-\uDFFF */
                    if (p->pos + 2 > p->len || p->buf[p->pos] != '\\' ||
                        p->buf[p->pos + 1] != 'u') {
                        return fail(p, BJ_ERR_UTF8);
                    }
                    p->pos += 2;
                    uint32_t lo;
                    if (!hex4(p, &lo)) return fail(p, BJ_ERR_SYNTAX);
                    if (lo < 0xDC00 || lo > 0xDFFF) return fail(p, BJ_ERR_UTF8);
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return fail(p, BJ_ERR_UTF8); /* lone low surrogate */
                }
                chunk_len = utf8_encode(cp, chunk);
                break;
            }
            default:
                return fail(p, BJ_ERR_SYNTAX);
            }
            char *dst = alloc_str(p, chunk_len);
            if (!dst) return false;
            memcpy(dst, chunk, chunk_len);
        } else if ((uint8_t)c < 0x80) {
            char *dst = alloc_str(p, 1);
            if (!dst) return false;
            *dst = c;
            p->pos++;
        } else {
            /* multi-byte UTF-8: validate then copy raw bytes */
            uint32_t seq_start = p->pos;
            if (!utf8_step(p)) return false;
            uint32_t n = p->pos - seq_start;
            char *dst = alloc_str(p, n);
            if (!dst) return false;
            memcpy(dst, &p->buf[seq_start], n);
        }
    }
}

/* ---- number parsing (strict JSON grammar; reject non-finite) ----------- */

static bool parse_number(bj_p *p, double *out)
{
    uint32_t start = p->pos;
    if (peek(p) == '-') p->pos++;
    /* int part: 0 | [1-9][0-9]* */
    if (peek(p) == '0') {
        p->pos++;
    } else if (peek(p) >= '1' && peek(p) <= '9') {
        while (peek(p) >= '0' && peek(p) <= '9') p->pos++;
    } else {
        return fail(p, BJ_ERR_NUMBER);
    }
    if (peek(p) == '.') {
        p->pos++;
        if (!(peek(p) >= '0' && peek(p) <= '9')) return fail(p, BJ_ERR_NUMBER);
        while (peek(p) >= '0' && peek(p) <= '9') p->pos++;
    }
    if (peek(p) == 'e' || peek(p) == 'E') {
        p->pos++;
        if (peek(p) == '+' || peek(p) == '-') p->pos++;
        if (!(peek(p) >= '0' && peek(p) <= '9')) return fail(p, BJ_ERR_NUMBER);
        while (peek(p) >= '0' && peek(p) <= '9') p->pos++;
    }
    uint32_t n = p->pos - start;
    if (n >= 64) return fail(p, BJ_ERR_NUMBER); /* absurdly long literal */
    char tmp[64];
    memcpy(tmp, &p->buf[start], n);
    tmp[n] = '\0';
    double v = strtod(tmp, NULL);
    if (!isfinite(v)) return fail(p, BJ_ERR_NUMBER); /* 1e999 etc. */
    *out = v;
    return true;
}

/* ---- recursive value parsing ------------------------------------------- */

static bool parse_value(bj_p *p, bj_node_t *node);

static bool parse_array(bj_p *p, bj_node_t *node)
{
    node->type = BJ_ARR;
    p->pos++; /* [ */
    skip_ws(p);
    int32_t *tail = &node->child;
    if (peek(p) == ']') { p->pos++; return true; }
    for (;;) {
        bj_node_t *child = alloc_node(p);
        if (!child) return false;
        *tail = (int32_t)(child - p->doc->nodes);
        if (!parse_value(p, child)) return false;
        tail = &child->next;
        skip_ws(p);
        char c = peek(p);
        if (c == ',') { p->pos++; skip_ws(p); continue; }
        if (c == ']') { p->pos++; return true; }
        return fail(p, BJ_ERR_SYNTAX);
    }
}

static bool parse_object(bj_p *p, bj_node_t *node)
{
    node->type = BJ_OBJ;
    p->pos++; /* { */
    skip_ws(p);
    int32_t *tail = &node->child;
    if (peek(p) == '}') { p->pos++; return true; }
    for (;;) {
        skip_ws(p);
        if (peek(p) != '"') return fail(p, BJ_ERR_SYNTAX);
        bj_node_t *child = alloc_node(p);
        if (!child) return false;
        *tail = (int32_t)(child - p->doc->nodes);
        if (!parse_string(p, &child->key, &child->key_len)) return false;
        /* duplicate key check within THIS object only */
        for (int32_t sib = node->child; sib != -1 && sib != (int32_t)(child - p->doc->nodes);
             sib = p->doc->nodes[sib].next) {
            const bj_node_t *s = &p->doc->nodes[sib];
            if (s->key_len == child->key_len &&
                memcmp(s->key, child->key, s->key_len) == 0) {
                return fail(p, BJ_ERR_DUP_KEY);
            }
        }
        skip_ws(p);
        if (peek(p) != ':') return fail(p, BJ_ERR_SYNTAX);
        p->pos++;
        skip_ws(p);
        if (!parse_value(p, child)) return false;
        tail = &child->next;
        skip_ws(p);
        char c = peek(p);
        if (c == ',') { p->pos++; continue; }
        if (c == '}') { p->pos++; return true; }
        return fail(p, BJ_ERR_SYNTAX);
    }
}

static bool parse_value(bj_p *p, bj_node_t *node)
{
    if (p->depth >= BOT_JSON_MAX_DEPTH) return fail(p, BJ_ERR_DEPTH);
    skip_ws(p);
    char c = peek(p);
    switch (c) {
    case '{':
        p->depth++;
        if (!parse_object(p, node)) return false;
        p->depth--;
        return true;
    case '[':
        p->depth++;
        if (!parse_array(p, node)) return false;
        p->depth--;
        return true;
    case '"':
        node->type = BJ_STR;
        return parse_string(p, &node->str, &node->str_len);
    case 't':
        if (p->pos + 4 <= p->len && memcmp(&p->buf[p->pos], "true", 4) == 0) {
            p->pos += 4;
            node->type = BJ_BOOL;
            node->boolean = true;
            return true;
        }
        return fail(p, BJ_ERR_SYNTAX);
    case 'f':
        if (p->pos + 5 <= p->len && memcmp(&p->buf[p->pos], "false", 5) == 0) {
            p->pos += 5;
            node->type = BJ_BOOL;
            node->boolean = false;
            return true;
        }
        return fail(p, BJ_ERR_SYNTAX);
    case 'n':
        if (p->pos + 4 <= p->len && memcmp(&p->buf[p->pos], "null", 4) == 0) {
            p->pos += 4;
            node->type = BJ_NULL;
            return true;
        }
        return fail(p, BJ_ERR_SYNTAX);
    default:
        if (c == '-' || (c >= '0' && c <= '9')) {
            node->type = BJ_NUM;
            return parse_number(p, &node->num);
        }
        return fail(p, BJ_ERR_SYNTAX);
    }
}

int32_t bj_parse(bj_doc_t *doc, const char *buf, uint32_t len)
{
    bj_p p = { doc, buf, len, 0, 0 };
    /* whole-input UTF-8 validation up front (strict UTF-8 requirement) */
    bj_p u = { doc, buf, len, 0, 0 };
    while (u.pos < u.len) {
        if (!utf8_step(&u)) {
            doc->err = BJ_ERR_UTF8;
            doc->err_pos = u.pos;
            return -1;
        }
    }
    skip_ws(&p);
    bj_node_t *root = alloc_node(&p);
    if (!root) return -1;
    if (!parse_value(&p, root)) return -1;
    if (root->type != BJ_OBJ) {
        doc->err = BJ_ERR_ROOT;
        doc->err_pos = 0;
        return -1;
    }
    skip_ws(&p);
    if (p.pos != p.len) {
        doc->err = BJ_ERR_TRAILING;
        doc->err_pos = p.pos;
        return -1;
    }
    return 0; /* root is always pool slot 0 */
}

int32_t bj_obj_get(const bj_doc_t *doc, int32_t obj, const char *key)
{
    if (obj < 0 || (uint32_t)obj >= doc->node_used) return -1;
    const bj_node_t *o = &doc->nodes[obj];
    if (o->type != BJ_OBJ) return -1;
    size_t klen = strlen(key);
    for (int32_t i = o->child; i != -1; i = doc->nodes[i].next) {
        const bj_node_t *c = &doc->nodes[i];
        if (c->key_len == klen && memcmp(c->key, key, klen) == 0) return i;
    }
    return -1;
}

bool bj_get_str(const bj_doc_t *doc, int32_t obj, const char *key,
                const char **s, uint16_t *len)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0 || doc->nodes[n].type != BJ_STR) return false;
    *s = doc->nodes[n].str;
    *len = doc->nodes[n].str_len;
    return true;
}

bool bj_get_num(const bj_doc_t *doc, int32_t obj, const char *key, double *out)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0 || doc->nodes[n].type != BJ_NUM) return false;
    *out = doc->nodes[n].num;
    return true;
}

bool bj_get_bool(const bj_doc_t *doc, int32_t obj, const char *key, bool *out)
{
    int32_t n = bj_obj_get(doc, obj, key);
    if (n < 0 || doc->nodes[n].type != BJ_BOOL) return false;
    *out = doc->nodes[n].boolean;
    return true;
}

bool bj_is_null(const bj_doc_t *doc, int32_t obj, const char *key)
{
    int32_t n = bj_obj_get(doc, obj, key);
    return n >= 0 && doc->nodes[n].type == BJ_NULL;
}

bool bj_str_copy(const bj_doc_t *doc, int32_t node, char *out, uint32_t out_cap)
{
    if (node < 0 || (uint32_t)node >= doc->node_used) return false;
    const bj_node_t *n = &doc->nodes[node];
    if (n->type != BJ_STR) return false;
    if ((uint32_t)n->str_len + 1 > out_cap) return false;
    memcpy(out, n->str, n->str_len);
    out[n->str_len] = '\0';
    return true;
}

const char *bj_err_name(bj_err_t err)
{
    switch (err) {
    case BJ_OK: return "ok";
    case BJ_ERR_SYNTAX: return "syntax";
    case BJ_ERR_DEPTH: return "depth";
    case BJ_ERR_POOL: return "pool";
    case BJ_ERR_DUP_KEY: return "dup_key";
    case BJ_ERR_UTF8: return "utf8";
    case BJ_ERR_NUMBER: return "number";
    case BJ_ERR_ROOT: return "root";
    case BJ_ERR_TRAILING: return "trailing";
    default: return "?";
    }
}
