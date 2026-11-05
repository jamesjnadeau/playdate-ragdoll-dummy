#ifndef PD_RENDER_H
#define PD_RENDER_H

#include "pd_api.h"
#include "render_list.h"

/* Registers the bitmap table RC_BITMAP commands index into (not copied; may be NULL). */
void pd_render_set_images(LCDBitmap** images, int count);
/* Clears the screen to white and draws every command in order. */
void pd_render_execute(PlaydateAPI* pd, const RenderList* rl);

#endif
