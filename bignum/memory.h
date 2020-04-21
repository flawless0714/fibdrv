#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <linux/slab.h>

static void *(*orig_malloc)(size_t, gfp_t) = kmalloc;
static void *(*orig_realloc)(const void *, size_t, gfp_t) = krealloc;
static void (*orig_free)(const void *) = kfree;

/* TODO: implement custom memory allocator which fits arbitrary precision
 * operations
 */
static inline void *xmalloc(size_t size)
{
    void *p;
    if (!(p = (*orig_malloc)(size, GFP_KERNEL))) {
        printk("fibdrv: malloc failed\n");
        // abort();
    }
    return p;
}

static inline void *xrealloc(void *ptr, size_t size)
{
    void *p;
    if (!(p = (*orig_realloc)(ptr, size, GFP_KERNEL)) && size != 0) {
        printk("fibdrv: malloc failed\n");
        // abort();
    }
    return p;
}

static inline void xfree(void *ptr)
{
    (*orig_free)(ptr);
}

#define MALLOC(n) xmalloc(n)
#define REALLOC(p, n) xrealloc(p, n)
#define FREE(p) xfree(p)

#endif /* !_MEMORY_H_ */
