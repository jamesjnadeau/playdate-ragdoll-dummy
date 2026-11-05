#ifndef PD_INPUT_H
#define PD_INPUT_H

#include "pd_api.h"
#include "input.h"

/* Fills in from the buttons and crank. BTN_* bits match PDButtons. */
void pd_input_read(PlaydateAPI* pd, GameInput* in);

#endif
