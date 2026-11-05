#include <string.h>
#include "save.h"

static const char* const k_sel_keys[SAVE_SLOTS] = {
    "sel.dummy-small", "sel.dummy-medium", "sel.dummy-large",
    "sel.weapon-chicken", "sel.weapon-bat", "sel.weapon-pan", "sel.weapon-sledge",
    "sel.stuffing", "sel.sounds"
};

void save_defaults(SaveData* s) {
    memset(s, 0, sizeof(*s));
    s->version = SAVE_VERSION;
}

/* Appends s at *pos; returns false (leaving *pos) if it does not fit with room for a final NUL. */
static bool put(char* buf, int cap, int* pos, const char* s) {
    int n = (int)strlen(s);
    if (*pos + n >= cap) return false;
    memcpy(buf + *pos, s, (size_t)n);
    *pos += n;
    return true;
}

static void int_text(int v, char* out) {
    char tmp[12];
    int n = 0, i = 0;
    if (v < 0) v = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v > 0);
    while (n > 0) out[i++] = tmp[--n];
    out[i] = '\0';
}

int save_write(const SaveData* s, char* buf, int cap) {
    int pos = 0, i;
    char num[12];
    if (!buf || cap <= 0) return 0;
    int_text(s->version, num);
    if (!put(buf, cap, &pos, "version=") || !put(buf, cap, &pos, num) || !put(buf, cap, &pos, "\n")) return 0;
    int_text(s->best_bags, num);
    if (!put(buf, cap, &pos, "best_bags=") || !put(buf, cap, &pos, num) || !put(buf, cap, &pos, "\n")) return 0;
    int_text(s->best_distance_m, num);
    if (!put(buf, cap, &pos, "best_distance_m=") || !put(buf, cap, &pos, num) || !put(buf, cap, &pos, "\n")) return 0;
    for (i = 0; i < SAVE_SLOTS; i++) {
        if (!put(buf, cap, &pos, k_sel_keys[i]) || !put(buf, cap, &pos, "=") ||
            !put(buf, cap, &pos, s->sel[i]) || !put(buf, cap, &pos, "\n")) return 0;
    }
    buf[pos] = '\0';
    return pos;
}

/* Parses a non-negative decimal of at most 9 digits; false for anything else. */
static bool parse_int(const char* v, int n, int* out) {
    int i, r = 0;
    if (n > 0 && v[0] == '-') { *out = 0; return n > 1; }   /* negative: clamp */
    if (n <= 0 || n > 9) return false;
    for (i = 0; i < n; i++) {
        if (v[i] < '0' || v[i] > '9') return false;
        r = r * 10 + (v[i] - '0');
    }
    *out = r;
    return true;
}

static void apply(SaveData* o, const char* k, int kn, const char* v, int vn) {
    int i, x;
    if (kn == 7 && memcmp(k, "version", 7) == 0) { if (parse_int(v, vn, &x)) o->version = x; return; }
    if (kn == 9 && memcmp(k, "best_bags", 9) == 0) { if (parse_int(v, vn, &x)) o->best_bags = x; return; }
    if (kn == 15 && memcmp(k, "best_distance_m", 15) == 0) { if (parse_int(v, vn, &x)) o->best_distance_m = x; return; }
    for (i = 0; i < SAVE_SLOTS; i++) {
        if ((int)strlen(k_sel_keys[i]) == kn && memcmp(k, k_sel_keys[i], (size_t)kn) == 0) {
            if (vn > SAVE_SEL_LEN - 1) vn = SAVE_SEL_LEN - 1;
            memcpy(o->sel[i], v, (size_t)vn);
            o->sel[i][vn] = '\0';
            return;
        }
    }
}

bool save_read(const char* buf, int len, SaveData* out) {
    int pos = 0;
    save_defaults(out);
    if (!buf) return false;
    while (pos < len) {
        int end = pos, eq = -1, kn, vs, vn;
        while (end < len && buf[end] != '\n') { if (buf[end] == '=' && eq < 0) eq = end; end++; }
        if (eq > pos) {
            kn = eq - pos;
            vs = eq + 1;
            vn = end - vs;
            if (vn > 0 && buf[vs + vn - 1] == '\r') vn--;
            apply(out, buf + pos, kn, buf + vs, vn);
        }
        pos = end + 1;
    }
    return true;
}
