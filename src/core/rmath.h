#ifndef RMATH_H
#define RMATH_H

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;

float clampf(float v, float lo, float hi);
float lerpf(float a, float b, float t);
float approachf(float v, float target, float step);

/* Table-driven sine/cosine. Angles in degrees; any value is wrapped. */
float rsin_deg(float deg);
float rcos_deg(float deg);

Vec3 v3_add(Vec3 a, Vec3 b);
Vec3 v3_sub(Vec3 a, Vec3 b);
Vec3 v3_scale(Vec3 a, float s);
float v3_len(Vec3 a);
float v3_dot(Vec3 a, Vec3 b);

#endif
