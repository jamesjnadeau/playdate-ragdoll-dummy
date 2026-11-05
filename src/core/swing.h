#ifndef SWING_H
#define SWING_H

#include "input.h"
#include "weapons.h"

typedef enum { SIDE_LEFT = -1, SIDE_CENTER = 0, SIDE_RIGHT = 1 } SwingSide;
typedef enum { VERT_UP = -1, VERT_DOWN = 1 } SwingVert;
typedef enum { SWS_READY, SWS_MEASURE, SWS_DELAY, SWS_SWEEP, SWS_RECOVER } SwingPhase;

typedef struct {
    SwingPhase phase;
    int t; /* frames elapsed in the current phase */
    SwingSide side;
    SwingVert vert;
    float peak;
    float factor;
    float sweep01;
} Swing;

typedef enum { SWE_NONE, SWE_STARTED, SWE_IMPACT } SwingEvent;

SwingEvent swing_update(Swing* s, const GameInput* in, const WeaponDef* w);

#endif
