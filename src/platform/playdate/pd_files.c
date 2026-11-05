#include "pd_files.h"

char* pd_read_file(PlaydateAPI* pd, const char* path, int* len_out) {
    FileStat st;
    SDFile* f;
    char* buf;
    int got, total = 0;

    if (len_out) *len_out = 0;
    if (pd->file->stat(path, &st) != 0 || st.size < 0) return NULL;
    f = pd->file->open(path, kFileRead | kFileReadData);
    if (!f) return NULL;
    buf = pd->system->realloc(NULL, (size_t)st.size + 1);
    if (!buf) {
        pd->file->close(f);
        return NULL;
    }
    while (total < st.size) {
        got = pd->file->read(f, buf + total, (unsigned int)(st.size - total));
        if (got <= 0) break;
        total += got;
    }
    pd->file->close(f);
    buf[total] = '\0';
    if (len_out) *len_out = total;
    return buf;
}

bool pd_write_file(PlaydateAPI* pd, const char* path, const void* data, int len) {
    SDFile* f = pd->file->open(path, kFileWrite);
    int wrote;
    if (!f) return false;
    wrote = pd->file->write(f, data, (unsigned int)len);
    pd->file->close(f);
    return wrote == len;
}
