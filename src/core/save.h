#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>

#define SAVE_VERSION 1
#define SAVE_SLOTS 9
#define SAVE_SEL_LEN 24   /* 23 characters + NUL */

/* sel[] slot order (Task 19 reuses it): dummy-small, dummy-medium, dummy-large, weapon-chicken, weapon-bat,
   weapon-pan, weapon-sledge, stuffing, sounds. An empty string means the core pack. */
typedef struct {
    int version;
    int best_bags;
    int best_distance_m;
    char sel[SAVE_SLOTS][SAVE_SEL_LEN];
} SaveData;

void save_defaults(SaveData* s);
/* Writes ASCII key=value lines. Returns the bytes written, or 0 if cap is too small. */
int save_write(const SaveData* s, char* buf, int cap);
/* Never fails hard: unknown keys are ignored and bad values keep the defaults. Returns false only for a NULL buf. */
bool save_read(const char* buf, int len, SaveData* out);

#endif
