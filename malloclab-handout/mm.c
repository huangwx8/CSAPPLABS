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

/*
* 本实验的基础思路仿照书上完成，使用隐含式链表，链表的每个结点代表一个使用中或者未
* 使用的块。块间的链式连接形成了线性结构，每次我们想申请空间就使用first-fit或者next-fit
* 方法。
* 
* hacking:
* 简单的按需分配法在"binary-bal.rep"和"binary2-bal.rep"上表现不佳。这是因为这两个测试例刻意
* 制造了内部碎片，从而堆利用率低。为了解决这个问题，可以
* 把大块和小块分开存储，从而避免大小块放在一起引起的内部碎片。具体到代码就是在malloc的
* 的时候，根据情况留出一定空隙，这个空隙允许小块插入而大块无法插入，从而小块排列在一起，减小了
* 大量离散分布的小块可能造成的大量碎片。
*
* 简单的重分配策略在"realloc-bal.rep"和"realloc2-bal.rep"上表现不佳，这两个case会频繁
* 调用realloc来增加某指针的占有空间，如果每次只分配刚刚好的空间，则每次都需要遍历链表，
* 分配更大的空间。为了不受限制，可以在每次realloc申请新空间时分配略多空间。
* 除此之外的一个技巧是：如果要realloc增加空间的指针指向堆的最后一个块，则直接增长堆
* 而无需开辟新空间再做复制。这两个测试例主要考察这个trick。
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "me",
    /* First member's full name */
    "noel huang",
    /* First member's email address */
    "huangwx8@mail2.sysu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* size of a word */
#define WSIZE 4

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

size_t mem_occupied;
void *cur_ptr;
bool logflag = 0;

/*
* set - set word with given address
*/
inline void set(void *p, size_t value)
{
    *(size_t *)(p) = value;
}

/*
* get - get word with given address
*/
inline size_t get(void *p)
{
    return *(size_t *)(p);
}

/*
* set - set node information with given address
*/
inline void set_node_info(void *p, size_t size, bool use)
{
    if (use)
        size += 1;
    set(p, size);
}

/*
* get - get node information with given address
*/
inline void get_node_info(void *p, size_t *psize, bool *puse)
{
    size_t val = get(p);
    *psize = val & (~0x7);
    *puse = val & 1;
}

/* linked list check */
void traversal()
{
    void *p = mem_heap_lo() + WSIZE;
    size_t blocksize;
    bool use;
    /* search */
    while (1)
    {
        get_node_info(p, &blocksize, &use);
        printf("(%p,[%d,%d])->", p, blocksize, !use);
        if (blocksize == 0)
            break;
        p += blocksize;
    }
    printf("\n");
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void *p = mem_heap_lo();
    /* allocate 4 words as prologue and epilogue blocks */
    if (mem_sbrk(WSIZE * 4) == (void *)-1)
        return -1;

    /* init linked list */
    set(p, 0);                          /* padding space */
    set_node_info(p + WSIZE, 8, 1);     /* prologue->head->size = 8, prologue->head->use = 1 */
    set_node_info(p + WSIZE * 2, 8, 1); /* prologue->foot->size = 8, prologue->foot->use = 1 */
    set_node_info(p + WSIZE * 3, 0, 1); /* epilogue->head->size = 0, epilogue->head->use = 1 */

    mem_occupied = 0;
    cur_ptr = p + WSIZE;

    return 0;
}

/*
* highbit - return the smallest 2^k that makes 2^k >= x
*/
size_t highbit(size_t x)
{
    x--;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x + 1;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /* size it needs */
    size = ALIGN(size + SIZE_T_SIZE);

    void *p;
    if (0)
    { /* first-fit */
        p = mem_heap_lo() + WSIZE;
    }
    else
    { /* next-fit */
        p = cur_ptr;
    }

    size_t blocksize;
    bool use;

    /* search a free block large enough */
    while (1)
    {
        get_node_info(p, &blocksize, &use);
        if (blocksize == 0)
            break;
        if (!use && blocksize >= size)
            break;
        p += blocksize;
    }

    /* no free block */
    if (blocksize == 0)
    {
        size_t prev_blocksize;
        bool prev_use;
        get_node_info(p - WSIZE, &prev_blocksize, &prev_use);

        /* following code is mainly work for trace 7, trace 8 */
        size_t dupsize = prev_blocksize * 4;
        if (prev_use && prev_blocksize > 8 && prev_blocksize < 100 && dupsize < size)
        {
            /* leave a empty block for other mm_malloc(prev_blocksize) requests */
            if (mem_sbrk(dupsize) == (void *)-1) /* mem_sbrk failed */
                return NULL;
            set_node_info(p, dupsize, 0);                     /* set self_head */
            set_node_info(p + dupsize - WSIZE, dupsize, 0); /* set self_foot */
            p += dupsize;
        }
        /* above code is mainly work for trace 7, trace 8 */

        /* request new memory space */
        if (mem_sbrk(size) == (void *)-1) /* mem_sbrk failed */
            return NULL;
        set_node_info(p, size, 1);                /* set self_head */
        set_node_info(p + size - WSIZE, size, 1); /* set self_foot */
        set_node_info(p + size, 0, 1);            /* shift right epilogue */
        cur_ptr = mem_heap_lo() + WSIZE;          /* reset cur_ptr */
    }
    /* find a free block */
    else
    {
        set_node_info(p, size, 1);                /* set self_head */
        set_node_info(p + size - WSIZE, size, 1); /* set self_foot */
        if (blocksize > size)
        {                                                              /* divide into two blocks if needed */
            set_node_info(p + size, blocksize - size, 0);              /* set residual_head */
            set_node_info(p + blocksize - WSIZE, blocksize - size, 0); /* set residual_foot */
        }
        cur_ptr = p + size;
    }

    /* update occupied */
    mem_occupied += size;
    if (logflag)
    {
        printf("need %d memory, allocate pointer %p\n", size, p);
        printf("current pointer %p\n", cur_ptr);
        traversal();
    }
    return p + WSIZE;
}

void *coalesce(void *p)
{
    size_t size, prev_size, next_size;
    bool use, prev_use, next_use;

    get_node_info(p, &size, &use);
    get_node_info(p - WSIZE, &prev_size, &prev_use);
    get_node_info(p + size, &next_size, &next_use);

    if (!prev_use && !next_use) /* prev_block is free, so is next_block */
    {
        set_node_info(p - prev_size, size + prev_size + next_size, 0);
        set_node_info(p + size + next_size - WSIZE, size + prev_size + next_size, 0);
        p = p - prev_size;
    }
    else if (!prev_use && next_use)
    { /* prev_block is free, next_block is using */
        set_node_info(p - prev_size, size + prev_size, 0);
        set_node_info(p + size - WSIZE, size + prev_size, 0);
        p = p - prev_size;
    }
    else if (prev_use && !next_use)
    { /* prev_block is not free, next_block is free */
        set_node_info(p, size + next_size, 0);
        set_node_info(p + size + next_size - WSIZE, size + next_size, 0);
    }
    cur_ptr = p;
    return p + WSIZE;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{

    void *p = ptr - WSIZE;

    size_t size;
    bool use;
    get_node_info(p, &size, &use);

    set_node_info(p, size, 0);                /* self_head->use = 0 */
    set_node_info(p + size - WSIZE, size, 0); /* self_foot->use = 0 */

    coalesce(p); /* coalesce neighborhood */

    mem_occupied -= size; /* update occupied */
    if (logflag)
    {
        printf("free pointer %p\n", p);
        printf("current pointer %p\n", cur_ptr);
        traversal();
    }
}

/*
 * mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t raw_size = size;
    /* size it needs */
    size = ALIGN(size + SIZE_T_SIZE);

    /* point at the head */
    ptr -= WSIZE;

    size_t blocksize;
    bool use;
    get_node_info(ptr, &blocksize, &use);

    if (blocksize >= size)
        return ptr + WSIZE;
    else
    { /* not enough, call malloc to allocate appropriate space */
        /* following code is mainly working for trace 9, trace 10 */
        void *pnext = ptr + blocksize;
        size_t next_blocksize;
        bool next_use;
        get_node_info(pnext, &next_blocksize, &next_use);
        if (next_blocksize == 0)
        {
            /* if ptr pionts at the last block of heap, simply extend heap, dont need to remove data */
            size_t size_ext = size + 0x400;
            if (mem_sbrk(size_ext - blocksize) == (void *)-1) /* mem_sbrk failed */
                return NULL;
            set_node_info(ptr, size_ext, 1);                    /* set self_head */
            set_node_info(ptr + size_ext - WSIZE, size_ext, 1); /* set self_foot */
            set_node_info(ptr + size_ext, 0, 1);                /* shift right epilogue */
            return ptr + WSIZE;
        }
        /* above code is mainly working for trace 7, trace 8 */

        else
        { /* if ptr is not the last, try to malloc and free */
            void *oldptr = ptr + WSIZE;
            void *newptr = mm_malloc(raw_size + 0x400); /* call malloc */
            if (newptr == NULL)
                return NULL;
            memcpy(newptr, oldptr, raw_size); /* copy old data */
            mm_free(oldptr);
            return newptr;
        }
    }
}
