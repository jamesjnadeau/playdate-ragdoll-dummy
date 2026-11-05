#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>

#define SAVE_VERSION 1
#define SAVE_SLOTS 9
#define SAVE_SEL_LEN 24   /* 23 characters + NUL */
#define SAVE_BUF_BYTES 1024   /* the largest possible file is about 450 bytes; the device stack is small */

/* sel[] slot order (Task 19 reuses it): dummy-small, dummy-medium, dummy-large, weapon-chicken, weapon-bat,
   weapon-pan, weapon-sledge, stuffing, sounds. An empty string means the core pack. */
typedef struct {
    int version;
    int best_bags;
    int best_distance_m;
    char sel[SAVE_SLOTS][SAVE_SEL_LEN];
} SaveData;

void save_defaults(SaveData* s);
/* Stores the bests and stamps the current SAVE_VERSION (a file is always written in the newest format). */
void save_set_bests(SaveData* s, int best_bags, int best_distance_m);

/* A failed write is retried a few times, spaced out, then given up on (until the next new best). */
#define SAVE_RETRY_MAX 3
#define SAVE_RETRY_WAIT_FRAMES 30
typedef struct { int fails, wait; } SaveRetry;
typedef enum { SAVE_RETRY_OK, SAVE_RETRY_AGAIN, SAVE_RETRY_GAVE_UP } SaveRetryResult;
void save_retry_init(SaveRetry* r);
/* Call once per frame while a save is pending: true when a write should be attempted now. */
bool save_retry_due(SaveRetry* r);
/* Reports a write attempt. GAVE_UP resets the state; the caller logs once and drops the pending save. */
SaveRetryResult save_retry_report(SaveRetry* r, bool ok);
/* Writes ASCII key=value lines. Returns the bytes written, or 0 if cap is too small. */
int save_write(const SaveData* s, char* buf, int cap);
/* Never fails hard: unknown keys are ignored and bad values keep the defaults. Returns false only for a NULL buf. */
bool save_read(const char* buf, int len, SaveData* out);

#endif
