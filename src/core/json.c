#include "json.h"

/* Container parse states. */
enum {
    A_FIRST = 0, /* array: value or ']' */
    A_NEXT,      /* array: ',' or ']' */
    A_VALUE,     /* array: value (after ',') */
    O_FIRST,     /* object: key or '}' */
    O_COLON,     /* object: ':' */
    O_VALUE,     /* object: value */
    O_NEXT,      /* object: ',' or '}' */
    O_KEY        /* object: key (after ',') */
};

typedef struct {
    int tok;
    int state;
} Frame;

static bool is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_hex(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Returns end offset (past closing quote) or -1. */
static int scan_string(const char* s, int len, int p) {
    p++; /* opening quote */
    while (p < len) {
        unsigned char c = (unsigned char)s[p];
        if (c == '"') return p + 1;
        if (c < 0x20) return -1;
        if (c == '\\') {
            char e;
            if (p + 1 >= len) return -1;
            e = s[p + 1];
            if (e == 'u') {
                int k;
                if (p + 5 >= len) return -1;
                for (k = 2; k < 6; k++)
                    if (!is_hex(s[p + k])) return -1;
                p += 6;
                continue;
            }
            if (e != '"' && e != '\\' && e != '/' && e != 'b' && e != 'f' && e != 'n' &&
                e != 'r' && e != 't')
                return -1;
            p += 2;
            continue;
        }
        p++;
    }
    return -1;
}

/* Returns end offset of a JSON number starting at p, or -1. */
static int scan_number(const char* s, int len, int p) {
    if (p < len && s[p] == '-') p++;
    if (p >= len || !is_digit(s[p])) return -1;
    while (p < len && is_digit(s[p])) p++;
    if (p < len && s[p] == '.') {
        p++;
        if (p >= len || !is_digit(s[p])) return -1;
        while (p < len && is_digit(s[p])) p++;
    }
    if (p < len && (s[p] == 'e' || s[p] == 'E')) {
        p++;
        if (p < len && (s[p] == '+' || s[p] == '-')) p++;
        if (p >= len || !is_digit(s[p])) return -1;
        while (p < len && is_digit(s[p])) p++;
    }
    return p;
}

static bool match_lit(const char* s, int len, int p, const char* lit, int n) {
    int k;
    if (p + n > len) return false;
    for (k = 0; k < n; k++)
        if (s[p + k] != lit[k]) return false;
    return true;
}

int json_parse(const char* s, int len, JsonTok* toks, int max_toks) {
    Frame stack[JSON_MAX_DEPTH];
    int sp = 0;
    int ntok = 0;
    int p = 0;
    bool done = false; /* root value complete */

    if (!s || len <= 0 || !toks) return -1;

    for (;;) {
        char c;
        int st;
        bool want_value, want_key;

        while (p < len && is_ws(s[p])) p++;
        if (p >= len) break;
        if (done) return -1; /* trailing garbage */
        c = s[p];
        st = sp > 0 ? stack[sp - 1].state : -1;

        /* Closers and separators. */
        if (sp > 0) {
            if (c == ']' && (st == A_FIRST || st == A_NEXT)) {
                toks[stack[sp - 1].tok].end = p + 1;
                sp--;
                p++;
                if (sp == 0) done = true;
                else stack[sp - 1].state = (stack[sp - 1].state >= O_FIRST) ? O_NEXT : A_NEXT;
                continue;
            }
            if (c == '}' && (st == O_FIRST || st == O_NEXT)) {
                toks[stack[sp - 1].tok].end = p + 1;
                sp--;
                p++;
                if (sp == 0) done = true;
                else stack[sp - 1].state = (stack[sp - 1].state >= O_FIRST) ? O_NEXT : A_NEXT;
                continue;
            }
            if (c == ',' && st == A_NEXT) {
                stack[sp - 1].state = A_VALUE;
                p++;
                continue;
            }
            if (c == ',' && st == O_NEXT) {
                stack[sp - 1].state = O_KEY;
                p++;
                continue;
            }
            if (c == ':' && st == O_COLON) {
                stack[sp - 1].state = O_VALUE;
                p++;
                continue;
            }
        }

        want_key = (st == O_FIRST || st == O_KEY);
        want_value = (sp == 0) || st == A_FIRST || st == A_VALUE || st == O_VALUE;
        if (!want_key && !want_value) return -1;

        if (want_key) {
            int e, i;
            if (c != '"') return -1;
            e = scan_string(s, len, p);
            if (e < 0) return -1;
            if (ntok >= max_toks) return -2;
            i = ntok++;
            toks[i].type = JT_STR;
            toks[i].start = p + 1;
            toks[i].end = e - 1;
            toks[i].size = 0;
            toks[i].parent = stack[sp - 1].tok;
            p = e;
            stack[sp - 1].state = O_COLON;
            continue;
        }

        /* A value. */
        {
            int i, parent = sp > 0 ? stack[sp - 1].tok : -1;
            JsonType ty;
            int e = -1;

            if (c == '{' || c == '[') {
                if (sp >= JSON_MAX_DEPTH) return -1;
                if (ntok >= max_toks) return -2;
                i = ntok++;
                toks[i].type = (c == '{') ? JT_OBJ : JT_ARR;
                toks[i].start = p;
                toks[i].end = -1;
                toks[i].size = 0;
                toks[i].parent = parent;
                if (parent >= 0) toks[parent].size++;
                stack[sp].tok = i;
                stack[sp].state = (c == '{') ? O_FIRST : A_FIRST;
                sp++;
                p++;
                continue;
            }
            if (c == '"') {
                ty = JT_STR;
                e = scan_string(s, len, p);
            } else if (c == '-' || is_digit(c)) {
                ty = JT_NUM;
                e = scan_number(s, len, p);
            } else if (c == 't') {
                ty = JT_TRUE;
                e = match_lit(s, len, p, "true", 4) ? p + 4 : -1;
            } else if (c == 'f') {
                ty = JT_FALSE;
                e = match_lit(s, len, p, "false", 5) ? p + 5 : -1;
            } else if (c == 'n') {
                ty = JT_NULL;
                e = match_lit(s, len, p, "null", 4) ? p + 4 : -1;
            } else {
                return -1;
            }
            if (e < 0) return -1;
            if (ntok >= max_toks) return -2;
            i = ntok++;
            toks[i].type = ty;
            toks[i].start = (ty == JT_STR) ? p + 1 : p;
            toks[i].end = (ty == JT_STR) ? e - 1 : e;
            toks[i].size = 0;
            toks[i].parent = parent;
            if (parent >= 0) {
                /* Object size counts pairs: bump on the value, not the key. */
                toks[parent].size++;
                stack[sp - 1].state = (st >= O_FIRST) ? O_NEXT : A_NEXT;
            } else {
                done = true;
            }
            p = e;
        }
    }

    if (!done || sp != 0) return -1;
    return ntok;
}

/* Index of the token just past the subtree rooted at i. */
static int skip(const JsonTok* t, int ntok, int i) {
    int n, k;
    if (i < 0 || i >= ntok) return ntok;
    if (t[i].type == JT_OBJ) n = t[i].size * 2;
    else if (t[i].type == JT_ARR) n = t[i].size;
    else n = 0;
    i++;
    for (k = 0; k < n && i < ntok; k++) i = skip(t, ntok, i);
    return i;
}

int json_obj_get(const char* s, const JsonTok* t, int ntok, int obj, const char* key) {
    int i, k, n;
    if (!s || !t || !key || obj < 0 || obj >= ntok || t[obj].type != JT_OBJ) return -1;
    n = 0;
    while (key[n]) n++;
    i = obj + 1;
    for (k = 0; k < t[obj].size && i + 1 < ntok; k++) {
        if (t[i].type == JT_STR && t[i].end - t[i].start == n) {
            int j = 0;
            while (j < n && s[t[i].start + j] == key[j]) j++;
            if (j == n) return i + 1;
        }
        i = skip(t, ntok, i + 1);
    }
    return -1;
}

int json_arr_at(const JsonTok* t, int ntok, int arr, int k) {
    int i, j;
    if (!t || arr < 0 || arr >= ntok || t[arr].type != JT_ARR) return -1;
    if (k < 0 || k >= t[arr].size) return -1;
    i = arr + 1;
    for (j = 0; j < k && i < ntok; j++) i = skip(t, ntok, i);
    return i < ntok ? i : -1;
}

bool json_str(const char* s, const JsonTok* t, int i, char* out, int cap) {
    int p, w = 0;
    if (!s || !t || !out || cap <= 0 || i < 0 || t[i].type != JT_STR) return false;
    for (p = t[i].start; p < t[i].end && w < cap - 1; p++) {
        char c = s[p];
        if (c == '\\' && p + 1 < t[i].end) {
            char e = s[++p];
            switch (e) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'u': c = '?'; p += 4; break; /* not decoded */
                default: c = e; break;            /* " \ / */
            }
        }
        out[w++] = c;
    }
    out[w] = 0;
    return true;
}

bool json_num_f(const char* s, const JsonTok* t, int i, float* out) {
    int p, end, digits = 0, exp10 = 0, ex = 0;
    bool neg = false, eneg = false;
    float m = 0.0f, scale = 1.0f;
    int k;

    if (!s || !t || !out || i < 0 || t[i].type != JT_NUM) return false;
    p = t[i].start;
    end = t[i].end;
    if (p < end && s[p] == '-') {
        neg = true;
        p++;
    }
    for (; p < end && is_digit(s[p]); p++) {
        if (digits < 9) {
            m = m * 10.0f + (float)(s[p] - '0');
            if (m != 0.0f) digits++;
        } else {
            exp10++; /* dropped integer digit: keep magnitude */
        }
    }
    if (p < end && s[p] == '.') {
        p++;
        for (; p < end && is_digit(s[p]); p++) {
            if (digits < 9) {
                m = m * 10.0f + (float)(s[p] - '0');
                if (m != 0.0f) digits++;
                exp10--;
            }
        }
    }
    if (p < end && (s[p] == 'e' || s[p] == 'E')) {
        p++;
        if (p < end && (s[p] == '+' || s[p] == '-')) {
            eneg = (s[p] == '-');
            p++;
        }
        for (; p < end && is_digit(s[p]); p++) {
            if (ex < 1000) ex = ex * 10 + (s[p] - '0');
        }
        exp10 += eneg ? -ex : ex;
    }
    if (exp10 > 38) exp10 = 38;
    if (exp10 < -38) exp10 = -38;
    for (k = 0; k < (exp10 < 0 ? -exp10 : exp10); k++) scale *= 10.0f;
    m = (exp10 < 0) ? m / scale : m * scale;
    *out = neg ? -m : m;
    return true;
}

bool json_num_i(const char* s, const JsonTok* t, int i, int* out) {
    int p, end, v = 0;
    bool neg = false;
    if (!s || !t || !out || i < 0 || t[i].type != JT_NUM) return false;
    p = t[i].start;
    end = t[i].end;
    if (p < end && s[p] == '-') {
        neg = true;
        p++;
    }
    if (p >= end) return false;
    for (; p < end; p++) {
        if (!is_digit(s[p])) return false; /* fraction/exponent: not an int */
        if (v > 214748364) return false;
        v = v * 10 + (s[p] - '0');
    }
    *out = neg ? -v : v;
    return true;
}
