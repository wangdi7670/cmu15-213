#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

#define WSIZE 4
#define DSIZE 8
#define CHUNK_SIZE (1 << 12)  // 4KB
#define MAX_UINT_32 (~0)

static inline unsigned int MaxSize(unsigned int size1, unsigned int size2)
{
    return size1 > size2 ? size1 : size2;
}

/**
 * @param alloc: 1 or 0
 */
static inline unsigned int Pack(unsigned int size, unsigned int alloc)
{
    return size | alloc;
}

static inline void Put(void *ptr, unsigned int val)
{
    *(unsigned int *)(ptr) = val;
}

static inline void *GetHeaderPtr(void *bp)
{
    return (void *)((char *)bp - WSIZE);
}

static inline size_t GetSize(void *bp)
{
    void *header = GetHeaderPtr(bp);
    return *(unsigned *)header & (~7);
}

static inline void *GetFooterPtr(void *bp) 
{
    return (void *)((char *)bp + GetSize(bp) - DSIZE);
}

static inline int GetAlloc(void *bp)
{
    void *header = GetHeaderPtr(bp);
    return *(unsigned *)header & 1;
}

static inline void *NextBlockPtr(void *bp)
{
    size_t size = GetSize(bp);
    return (void *)((char *)bp + size);
}

static inline void *PrevBlockPtr(void *bp)
{
    unsigned int prevSize = *(unsigned int *)((char *)bp - DSIZE) & (~7);
    return (void *)((char *)bp - prevSize);
}