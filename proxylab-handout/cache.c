#include <stdlib.h>
#include "cache.h"
#include "csapp.h"

Entry g_cache[ENTRY_SIZE];

void CacheInit()
{
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
        g_cache[i].url = NULL;
        g_cache[i].object = NULL;
        g_cache[i].object_len = 0;
    }
}

void CacheObject(char *url, size_t url_len, char *object, size_t object_len)
{
    int index = -1;
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
        if (g_cache[i].url == NULL && g_cache[i].object == NULL) {
            index = i;
            break;
        }
    }

    /* for convience, just evict first, not implement LRU */
    if (index == -1) {
        index = 0;
        Free(g_cache[index].url);
        Free(g_cache[index].object);
    }
    
    g_cache[index].url = (char *)Malloc(url_len + 1);            
    memcpy(g_cache[index].url, url, url_len);
    g_cache[index].url[url_len] = '\0';

    g_cache[index].object = (char *)Malloc(object_len);
    memcpy(g_cache[index].object, object, object_len);

    g_cache[index].object_len = object_len;
}

char *FindObejct(char *url, size_t *object_len)
{
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
        if (g_cache[i].url == NULL) {
            continue;
        }
        
        if (strcmp(url, g_cache[i].url) == 0) {

            *object_len = g_cache[i].object_len;
            return g_cache[i].object;
        }
    }

    return NULL;
}