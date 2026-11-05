#ifndef JSON_H
#define JSON_H

#include <stdbool.h>

#define JSON_MAX_DEPTH 32

typedef enum { JT_OBJ, JT_ARR, JT_STR, JT_NUM, JT_TRUE, JT_FALSE, JT_NULL } JsonType;

/* start/end are byte offsets, [start,end). Strings exclude the quotes (raw,
 * escapes not decoded). size = children (object: key/value pairs).
 * parent = containing object/array token, or -1 for the root. In an object,
 * keys and values are consecutive tokens (key, then value subtree). */
typedef struct {
    JsonType type;
    int start, end;
    int size;
    int parent;
} JsonTok;

/* Token count, -1 invalid, -2 out of tokens. Does not allocate. */
int json_parse(const char* s, int len, JsonTok* toks, int max_toks);

/* Index of the value for key in object token obj, or -1. */
int json_obj_get(const char* s, const JsonTok* t, int ntok, int obj, const char* key);

/* Copies the string (handles \" \\ \n etc.), truncating safely, NUL-terminated. */
bool json_str(const char* s, const JsonTok* t, int i, char* out, int cap);

bool json_num_f(const char* s, const JsonTok* t, int i, float* out);
bool json_num_i(const char* s, const JsonTok* t, int i, int* out);

/* Index of the k-th element of array token arr, or -1. */
int json_arr_at(const JsonTok* t, int ntok, int arr, int k);

#endif
