#ifndef PD_SOUND_H
#define PD_SOUND_H

#include "pd_api.h"
#include "game.h"

/* Loads the sound pack's samples from sound_pack_dir (for example "packs/core/sounds"): swing, hit-light,
   hit-heavy, burst, grunt-1..3, door-slam, step and hurt, without the .wav extension. A missing file is logged
   and that sound stays silent. */
void pd_sound_init(PlaydateAPI* pd, const char* sound_pack_dir);
/* Plays one sound on its own SamplePlayer, restarting it if it is still playing. The grunt rotates through grunt-1..3. */
void pd_sound_play(PlaydateAPI* pd, SoundId id);

#endif
