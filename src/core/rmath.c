#include <math.h>
#include "rmath.h"

#define SIN_TABLE_N 1024

/* Full-circle sine table, built lazily on first use. sinf is only called here. */
static float s_sin_table[SIN_TABLE_N];
static int s_sin_ready = 0;

static void sin_table_init(void) {
    int i;
    for (i = 0; i < SIN_TABLE_N; i++) {
        s_sin_table[i] = sinf((float)i * (6.2831853f / (float)SIN_TABLE_N));
    }
    s_sin_ready = 1;
}

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

float approachf(float v, float target, float step) {
    if (v < target) {
        v += step;
        if (v > target) v = target;
    } else {
        v -= step;
        if (v < target) v = target;
    }
    return v;
}

float rsin_deg(float deg) {
    float d, pos, frac, a, b;
    int i;

    if (!s_sin_ready) sin_table_init();

    d = fmodf(deg, 360.0f);
    if (d < 0.0f) d += 360.0f;

    pos = d * ((float)SIN_TABLE_N / 360.0f);
    i = (int)pos;
    frac = pos - (float)i;
    i &= SIN_TABLE_N - 1;

    a = s_sin_table[i];
    b = s_sin_table[(i + 1) & (SIN_TABLE_N - 1)];
    return a + (b - a) * frac;
}

float rcos_deg(float deg) {
    return rsin_deg(deg + 90.0f);
}

Vec3 v3_add(Vec3 a, Vec3 b) {
    Vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

Vec3 v3_sub(Vec3 a, Vec3 b) {
    Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

Vec3 v3_scale(Vec3 a, float s) {
    Vec3 r = { a.x * s, a.y * s, a.z * s };
    return r;
}

float v3_len(Vec3 a) {
    return sqrtf(v3_dot(a, a));
}

float v3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
