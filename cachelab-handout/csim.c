#include "cachelab.h"
#include "getopt.h"
#include "stdlib.h"
#include "unistd.h"
#include "assert.h"
#include "stdio.h"
#include "math.h"
#include "sys/types.h"
#include "fcntl.h"
#include "string.h"

#define LINE_SIZE 256
#define BUFFER_SIZE 256


/**
 * write miss strategy: load memory into cache
 * 
 * 用 csim-ref 试一下写失效，先写内存，再读这个内存，会发现读内存没有 cache miss，
 * 这说明 csim-res 用的是 write-allocate strategy，即写操作cache miss时，会把内存中的先数据写入缓存中
 */

int g_verbose = 0;

typedef struct CacheParameter {
    int s;
    int E;
    int b;
} CacheParameter;

static inline void InitCacheParameter(CacheParameter *cacheParameter)
{
    assert(cacheParameter != NULL);
    cacheParameter->s = -1;
    cacheParameter->b = -1;
    cacheParameter->E = -1;
}

static u_int64_t Get_B_Mask(u_int64_t addr, CacheParameter *cacheParameter)
{
    assert(cacheParameter != NULL);
    u_int64_t b_mask = 0;
    for (size_t i = 0; i < cacheParameter->b; ++i) {
        b_mask |= (1 << i);
    }
    return b_mask;
}

static u_int64_t Get_S_Mask(u_int64_t addr, CacheParameter *cacheParameter)
{
    u_int64_t s_mask = 0;
    for (size_t i = 0; i < cacheParameter->s; ++i) {
        s_mask |= (1 << (i + cacheParameter->b));
    }
    return s_mask;
}

typedef struct CacheLine {
    char valid;  // 0 or 1
    char dirty;  // 0 or 1
    u_int64_t tag;
    struct CacheLine *next;
    struct CacheLine *prev;
} CacheLine;

typedef struct CachePerSet {
    /* cacheLine and size specify an array */
    CacheLine *cacheLine;
    size_t size;
    
    /* head is the first node of linkedlist, it may be not equal to cacheLine */
    /* head is the most recently used */
    CacheLine *head;
} CachePerSet;


typedef struct Cache {
    CachePerSet *cachePerSet;
    size_t size;
} Cache;

Cache g_cache;

typedef struct Result {
    int hits;
    int missed;
    int evictions;
} Result;

static CacheLine *GetTail(const CachePerSet *cachePerSet)
{
    assert(cachePerSet != NULL);
    CacheLine *temp = cachePerSet->head;
    while (temp != NULL) {
        if (temp->next == NULL) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

// Do LRU
static void PutMostFront(CacheLine *temp, CachePerSet *cachePerSet)
{
    assert(temp != NULL);
    assert(cachePerSet != NULL);
    if (temp != cachePerSet->head) {
        assert(temp->prev != NULL);
        temp->prev->next = temp->next;
        if (temp->next != NULL) {
            temp->next->prev = temp->prev;
        }

        // LRU
        assert(cachePerSet->head != NULL);
        cachePerSet->head->prev = temp;
        temp->next = cachePerSet->head;
        temp->prev = NULL;
        cachePerSet->head = temp;
    }
}

static void AccessCacheLine(CachePerSet *cachePerSet, u_int64_t tag, Result *result)
{
    assert(cachePerSet != NULL);
    assert(result != NULL);


    int isHits = 0;
    CacheLine *temp = cachePerSet->head;
    CacheLine *candidate = NULL;
    while (temp != NULL) {
        if (g_verbose == 1) {
            printf("access cacheLine(%p), valid = %d, tag = %ld\n", temp, temp->valid, temp->tag);
            printf("dest tag = %ld\n", tag);
        }
        if (candidate == NULL && temp->valid == 0) {
            candidate = temp;
        }
        if (temp->valid == 1 && temp->tag == tag) {
            result->hits += 1;
            if (g_verbose) {
                printf("1 hit\n");
            }
            isHits = 1;
            break;
        }
        temp = temp->next;
    }

    if (isHits == 1) {
        PutMostFront(temp, cachePerSet);
    } else {
        result->missed += 1;
        if (g_verbose) {
            printf("1 miss\n");
        }
        if (candidate == NULL) {
            result->evictions += 1;
            if (g_verbose) {
                printf("1 eviction\n");
            }
            CacheLine *tail = GetTail(cachePerSet);
            assert(tail != NULL);
            tail->tag = tag;
            PutMostFront(tail, cachePerSet);
            if (g_verbose == 1) {
                printf("tail CacheLine(%p): tag = %ld, valid = %d\n", tail, tag, tail->valid);
            }
        } else {
            candidate->valid = 1;
            candidate->tag = tag;
            PutMostFront(candidate, cachePerSet);
            assert(candidate == cachePerSet->head);
            if (g_verbose == 1) {
                printf("candidate CacheLine(%p): tag = %ld, valid = %d\n", candidate, tag, candidate->valid);
            }
        }
    }
}

static void InitResult(Result *result)
{
    assert(result != NULL);
    result->hits = 0;
    result->missed = 0;
    result->evictions = 0;   
}

static void InitCachePerfSet(CachePerSet *cachePerSet, CacheParameter *parameter)
{
    assert(parameter != NULL);
    assert(cachePerSet != NULL);

    cachePerSet->cacheLine = malloc(sizeof(CacheLine) * parameter->E);
    assert(cachePerSet->cacheLine != NULL);
    cachePerSet->size = parameter->E;
    cachePerSet->head = cachePerSet->cacheLine;

    for (size_t i = 0; i < parameter->E; ++i) {
        CacheLine *curCacheLine = cachePerSet->cacheLine + i;
        if (i == 0) {
            curCacheLine->prev == NULL;
        } else {
            curCacheLine->prev = curCacheLine - 1;
        }

        if (i == parameter->E - 1) {
            curCacheLine->next = NULL;
        } else {
            curCacheLine->next = curCacheLine + 1;
        }
    }
}

static void InitCache(Cache *cache, CacheParameter *parameter)
{
    assert(parameter != NULL);
    u_int64_t sets = pow((double)2, (double)parameter->s);

    CachePerSet *cacheSet = malloc(sets * sizeof(CachePerSet));
    assert(cacheSet != NULL);
    cache->cachePerSet = cacheSet;
    cache->size = sets;

    for (size_t i = 0; i < sets; ++i) {
        CachePerSet *curCacheSet = cacheSet + i;
        InitCachePerfSet(curCacheSet, parameter);
    }
}

static void FreeCachePerSet(CachePerSet *cachePerSet) 
{
    assert(cachePerSet != NULL);
    free(cachePerSet->cacheLine);
}

static void FreeCache(Cache *cache)
{
    assert(cache != NULL);
    for (size_t i = cache->size - 1; i >= 0; --i) {
        FreeCachePerSet(cache->cachePerSet + i);
        if (i == 0) {
            break;
        }
    }
    free(cache->cachePerSet);
}

static inline char GetOperation(char *line, size_t size)
{
    assert(line != NULL);
    assert(line[0] == ' ');
    return line[1];
}

//  S 18,1
/**
 * @param buffer[output]
 */
static void GetAddress(char *line, size_t size, char *buffer, size_t bufferSize)
{
    assert(line != NULL);
    size_t i = 0;
    for (; i < size; ++i) {
        if (line[i] == ',') {
            break;
        }
    }

    assert(i != size);
    size_t addrFirstCharIdx = 3;
    size_t addressLength = i - addrFirstCharIdx + 1;
    assert(bufferSize > addressLength);

    memcpy(buffer, line + addrFirstCharIdx, addressLength);
    buffer[addressLength] = '\0';
}

static int GetSetsIdx(char *address, size_t size, CacheParameter *parameter)
{
    assert(parameter != NULL);
    u_int64_t addr = atoll(address);
    u_int64_t s_mask = Get_S_Mask(addr, parameter);
    return (s_mask & addr) >> parameter->b;
}

static u_int64_t GetTag(char *address, size_t size, CacheParameter *parameter)
{
    u_int64_t addr = atoll(address);
    return addr >> (parameter->s + parameter->b);
}

static void ProcessLOperation(char *line, size_t size, Cache *cache, Result *result, CacheParameter *parameter)
{
    assert(line != NULL);
    assert(cache != NULL);
    assert(result != NULL);
    char address[BUFFER_SIZE];
    GetAddress(line, LINE_SIZE, address, BUFFER_SIZE);

    int setsIdx = GetSetsIdx(address, BUFFER_SIZE, parameter);
    CachePerSet *cachePerSet = &cache->cachePerSet[setsIdx];

    u_int64_t tag = GetTag(address, BUFFER_SIZE, parameter);

    AccessCacheLine(cachePerSet, tag, result);
}

static void ProcessSOperation(char *line, size_t size, Cache *cache, Result *result, CacheParameter *parameter)
{
    assert(line != NULL);
    assert(cache != NULL);
    assert(result != NULL);
    char address[BUFFER_SIZE];
    GetAddress(line, LINE_SIZE, address, BUFFER_SIZE);

}


int main(int argc, char *argv[])
{
    char c;
    CacheParameter cacheParameter;
    InitCacheParameter(&cacheParameter);
    FILE *file;
    while ((c = getopt(argc, argv, "s:E:b:t:v")) != -1) {
        switch (c) {
            case 's':
                cacheParameter.s = atoi(optarg);
                break;
            case 'E':
                cacheParameter.E = atoi(optarg);
                break;
            case 'b':
                cacheParameter.b = atoi(optarg);
                break;
            case 'v':
                g_verbose = 1;
                break;
            case 't':
                file = fopen(optarg, "r");
                if (file == NULL) {
                    perror("fopen failed");
                    exit(1);
                }
                break;
        }
    }

    if (cacheParameter.s == -1 || cacheParameter.b == -1 || cacheParameter.E == -1) {
        printf("input option is wrong\n");
        exit(1);
    }

    InitCache(&g_cache, &cacheParameter);

    Result result;
    InitResult(&result);
    char line[LINE_SIZE];
    while (fgets(line, sizeof(line), file)) {
        if (g_verbose == 1) {
            printf("== line.length = %ld, %s", strlen(line), line);
        }
        if (line[0] != ' ') {
            continue;
        }

        char operation = GetOperation(line, LINE_SIZE);
        switch (operation) {
            case 'L':
                ProcessLOperation(line, LINE_SIZE, &g_cache, &result, &cacheParameter);
                break;
            case 'S':
                break;
            case 'M':
                break;
            default:
                printf("wrong operation: %c\n", operation);
                exit(1);
        }
        printf("\n");
    }

    FreeCache(&g_cache);

    printSummary(result.hits, result.missed, result.evictions);
    return 0;
}
