#ifndef CACHE_H
#define CACHE_H

#include <stdlib.h>
/**
 * not implement LRU, not limit MAX_OBJECT_SIZE
 * just an cache-array of length 10, and evict first element in array.
 */

#define ENTRY_SIZE 10

typedef struct {
    char *url;  // url represent a string, including '\0'
    char *object;
    size_t object_len;
} Entry;


void CacheInit();

void CacheObject(char *url, size_t url_len, char *object, size_t object_len);

char *FindObejct(char *url, size_t *object_len);

#endif