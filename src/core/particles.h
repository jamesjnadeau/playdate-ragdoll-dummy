#ifndef PARTICLES_H
#define PARTICLES_H

#include <stdbool.h>
#include <stdint.h>
#include "rmath.h"
#include "rng.h"
#include "camera.h"
#include "render_list.h"
#include "tuning.h"

typedef enum { PK_PUFF, PK_SCRAP, PK_TOOTH } ParticleKind;
typedef struct {
    bool active; ParticleKind kind; uint8_t variant; uint8_t bounced;
    Vec3 p, v; float angle, spin; int life; bool settled;
} Particle;
typedef struct { Particle p[PARTICLE_MAX]; int next; } ParticlePool;

/* BURST_PUFFS puffs and BURST_SCRAPS scraps sprayed in a hemisphere around wall_normal (pointing away
   from the surface), plus BURST_FLOOR_PUFFS puffs thrown at the floor below to form the pile.
   A full pool is overwritten in ring order, oldest first. */
void particles_burst(ParticlePool* pp, Rng* rng, Vec3 at, Vec3 wall_normal);
/* TOOTH_COUNT teeth, up and out. */
void particles_teeth(ParticlePool* pp, Rng* rng, Vec3 at);
/* Gravity, floor settle (puffs stick, scraps and teeth bounce once), walls, life--. */
void particles_step(ParticlePool* pp);
/* Appended after the world, far to near among themselves. */
void draw_particles(RenderList* rl, const Camera* cam, const ParticlePool* pp);

#endif
