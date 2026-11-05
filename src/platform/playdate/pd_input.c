#include "pd_input.h"

void pd_input_read(PlaydateAPI* pd, GameInput* in) {
    PDButtons cur, pushed, released;
    pd->system->getButtonState(&cur, &pushed, &released);
    in->held = (uint8_t)cur;
    in->pressed = (uint8_t)pushed;
    in->released = (uint8_t)released;
    in->crank_docked = pd->system->isCrankDocked() != 0;
    in->crank_delta = in->crank_docked ? 0.0f : pd->system->getCrankChange();
    in->crank_angle = pd->system->getCrankAngle();
}
