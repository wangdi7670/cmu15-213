/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *g_heapList;
static int g_debug = 1;

static void *PrintLog(char *info)
{
    if (g_debug) {
        fprintf(stderr, info);
    }
}


/* 
 * mm_init - initialize the malloc package.
 * @return 1 if failure, 0 if success
 */
int mm_init(void)
{
    void *p = mem_sbrk(4 * WSIZE);
    if (p == (void *)-1) {
        return 1;
    }

    Put(p + WSIZE, Pack(8, 1));
    Put(p + 2 * WSIZE, Pack(8, 1));
    Put(p + 3 * WSIZE, Pack(0, 1));

    g_heapList = p + 2 * WSIZE;
    return 0;
}

static void *FindFit(int size)
{
    void *bp = NextBlockPtr(g_heapList);

    while (1) {
        int alloc = GetAlloc(bp);
        size_t curSize = GetSize(bp);
        if (alloc == 1 && curSize == 0) {
            return NULL;
        }

        if (alloc == 0 && curSize > size) {
            return bp;
        }

        bp = NextBlockPtr(bp);
    } 
}

static void Place(void *bp, int newsize)
{
    size_t oldSize = GetSize(bp);

    if (oldSize > newsize) {
        int leftSize = oldSize - newsize;
        Put(GetFooterPtr(bp), Pack(leftSize, 0));  // Set truncated-block footer

        Put(GetHeaderPtr(bp), Pack(newsize, 1));  // Set allocated-block header
        Put(GetFooterPtr(bp), Pack(newsize, 1));  // Set allocated-block footer

        Put(GetHeaderPtr(NextBlockPtr(bp)), Pack(leftSize, 0));  // Set truncatec-block header
    } else {
        assert(oldSize == newsize);
        Put(GetHeaderPtr(bp), Pack(newsize, 1));
        Put(GetFooterPtr(bp), Pack(newsize, 1));
    }
}

static void *Coalesce(void *bp)
{
    void *prevBp = PrevBlockPtr(bp);
    int prevAlloc = GetAlloc(prevAlloc);
    void *nextBp = NextBlockPtr(bp);
    int nextAlloc = GetAlloc(nextBp);

    if (prevAlloc == 1 && nextAlloc == 1) {
        return bp;
    }

    unsigned int curSize = GetSize(bp);

    if (prevAlloc == 0 && nextAlloc == 1) {
        unsigned int prevSize = GetSize(prevBp);
        assert((unsigned int)MAX_UINT_32 - curSize > prevSize);

        Put(GetHeaderPtr(prevBp), Pack(curSize + prevSize, 0));
        Put(GetFooterPtr(prevBp), Pack(curSize + prevSize, 0));
        return prevBp;
    }

    if (prevAlloc == 1 && nextAlloc == 0) {
        unsigned int nextSize = GetSize(nextBp);
        assert((unsigned int)MAX_UINT_32 - curSize > nextSize);

        Put(GetHeaderPtr(bp), Pack(curSize + nextSize, 0));
        Put(GetFooterPtr(bp), Pack(curSize + nextSize, 0));
        return bp;
    }

    assert(prevAlloc == 0);
    assert(nextAlloc == 0);

    unsigned int prevSize = GetSize(prevBp);
    unsigned int nextSize = GetSize(nextBp);
    assert((unsigned int)(MAX_UINT_32) - curSize > prevSize);
    assert((unsigned)(MAX_UINT_32) - curSize - prevSize > nextSize);

    unsigned int newSize = curSize + prevSize + nextSize;
    Put(GetHeaderPtr(prevBp), Pack(newSize, 0));
    Put(GetFooterPtr(prevBp), Pack(newSize, 0));
    return prevBp;
}

static void *ExtendHeap(int size)
{
    assert(size % 8 == 0);
    void *p = mem_sbrk(size);
    if (p == (void *)-1) {
        return NULL;
    }

    Put(GetHeaderPtr(p), Pack(size, 0));
    Put(GetFooterPtr(p), Pack(size, 0));
    Put(GetHeaderPtr(NextBlockPtr(p)), Pack(0, 1));  // new epilogue-block

    return Coalesce(p);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size <= 0) {
        PrintLog("size is invalid\n");
        return NULL;
    }
    int newsize = ALIGN(size + SIZE_T_SIZE);

    void *bp = FindFit(newsize);
    if (bp != NULL) {
        Place(bp, newsize);
        return bp;
    }

    bp = ExtendHeap(MaxSize(newsize, CHUNK_SIZE));
    if (bp == NULL) {
        printf("ERROR ExtendHeap failed\n");
        return NULL;
    }
    Place(bp, newsize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    int success = 0;
    void *bp = NextBlockPtr(g_heapList);

    while (1) {
        int alloc = GetAlloc(bp);
        size_t size = GetSize(bp);
        if (size == 0 && alloc == 1) {  // enconter epilogue-block
            break;
        }

        if (bp == ptr) {
            if (alloc == 1) {
                // free current block
                Put(GetHeaderPtr(bp), Pack(GetSize(bp), 0));  
                Put(GetFooterPtr(bp), Pack(GetSize(bp), 0));
                Coalesce(bp);

                success = 1;    
            } else {
                fprintf(stderr, "ptr(%p) is already freed", ptr);
            }

            break;
        }

        bp = NextBlockPtr(bp);
    }

    if (success == 0) {
        fprintf(stderr, "ptr(%p) is invalid", ptr);
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize){
        copySize = size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














