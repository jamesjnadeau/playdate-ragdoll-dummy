#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

enum { BTN_LEFT = 1, BTN_RIGHT = 2, BTN_UP = 4, BTN_DOWN = 8, BTN_B = 16, BTN_A = 32 };

typedef struct {
    uint8_t held, pressed, released;
    float crank_delta;
    float crank_angle; /* degrees, 0 = crank arm up */
    bool crank_docked;
} GameInput;

#endif
