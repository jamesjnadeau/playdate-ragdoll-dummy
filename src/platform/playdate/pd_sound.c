#include <string.h>
#include "pd_sound.h"

#define GRUNT_VARIANTS 3

static const char* const k_file[SND_COUNT] = {
    "swing", "hit-light", "hit-heavy", "burst", "grunt-1", "door-slam", "step"
};

static SamplePlayer* g_player[SND_COUNT];
static AudioSample* g_sample[SND_COUNT];
static AudioSample* g_grunt[GRUNT_VARIANTS];
static int g_grunt_next = 0;

static AudioSample* load_sample(PlaydateAPI* pd, const char* dir, const char* name) {
    char path[96];
    AudioSample* s;
    size_t dn = strlen(dir), nn = strlen(name);
    if (dn + 1 + nn + 1 > sizeof(path)) return NULL;
    memcpy(path, dir, dn);
    path[dn] = '/';
    memcpy(path + dn + 1, name, nn + 1);
    s = pd->sound->sample->load(path);
    if (!s) pd->system->logToConsole("sound missing: %s", path);
    return s;
}

void pd_sound_init(PlaydateAPI* pd, const char* sound_pack_dir) {
    int i;
    for (i = 0; i < SND_COUNT; i++) {
        g_sample[i] = load_sample(pd, sound_pack_dir, k_file[i]);
        g_player[i] = pd->sound->sampleplayer->newPlayer();
        if (g_player[i] && g_sample[i]) pd->sound->sampleplayer->setSample(g_player[i], g_sample[i]);
    }
    g_grunt[0] = g_sample[SND_GRUNT];   /* grunt-1 was loaded with the table above */
    for (i = 1; i < GRUNT_VARIANTS; i++) {
        char name[] = "grunt-1";
        name[6] = (char)('1' + i);
        g_grunt[i] = load_sample(pd, sound_pack_dir, name);
    }
}

void pd_sound_play(PlaydateAPI* pd, SoundId id) {
    SamplePlayer* p;
    if ((int)id < 0 || (int)id >= SND_COUNT) return;
    p = g_player[id];
    if (!p) return;
    if (id == SND_GRUNT) {
        int k, found = 0;
        for (k = 0; k < GRUNT_VARIANTS; k++) {   /* the next variant that loaded */
            AudioSample* s = g_grunt[(g_grunt_next + k) % GRUNT_VARIANTS];
            if (s) {
                pd->sound->sampleplayer->setSample(p, s);
                g_grunt_next = (g_grunt_next + k + 1) % GRUNT_VARIANTS;
                found = 1;
                break;
            }
        }
        if (!found) return;   /* no variant loaded: silent */
    } else if (!g_sample[id]) {
        return;   /* missing file: silent */
    }
    pd->sound->sampleplayer->play(p, 1, 1.0f);
}
