#ifndef PD_FILES_H
#define PD_FILES_H

#include <stdbool.h>
#include "pd_api.h"

/* Reads a whole data-dir file. Returns a pd->system->realloc buffer
 * (NUL-terminated, *len_out excludes the NUL) or NULL; free with realloc(p, 0). */
char* pd_read_file(PlaydateAPI* pd, const char* path, int* len_out);
bool pd_write_file(PlaydateAPI* pd, const char* path, const void* data, int len);

#endif
