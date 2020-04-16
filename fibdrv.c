#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include "common.h"
#include "tiny-bignum-c/bn_kernel_space.h"


MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

#ifdef BIG_NUM
#define MAX_LENGTH 150
#elif defined ORIG_NUM
/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92
#endif

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);


#ifdef BIG_NUM
/* force add external function to kernel module */
inline void bignum_assign(struct bn *dst, struct bn *src)
{
    // require(dst, "dst is null");
    // require(src, "src is null");

    int i;
    for (i = 0; i < BN_ARRAY_SIZE; ++i) {
        dst->array[i] = src->array[i];
    }
}

inline void bignum_add(struct bn *a, struct bn *b, struct bn *c)
{
    // require(a, "a is null");
    // require(b, "b is null");
    // require(c, "c is null");

    DTYPE_TMP tmp;
    int carry = 0;
    int i;
    for (i = 0; i < BN_ARRAY_SIZE; ++i) {
        tmp = (DTYPE_TMP) a->array[i] + b->array[i] + carry;
        carry = (tmp > MAX_VAL);
        c->array[i] = (tmp & MAX_VAL);
    }
}

inline void bignum_from_int(struct bn *n, DTYPE_TMP i)
{
    // require(n, "n is null");

    bignum_init(n);

    /* Endianness issue if machine is not little-endian? */
#ifdef WORD_SIZE
#if (WORD_SIZE == 1)
    n->array[0] = (i & 0x000000ff);
    n->array[1] = (i & 0x0000ff00) >> 8;
    n->array[2] = (i & 0x00ff0000) >> 16;
    n->array[3] = (i & 0xff000000) >> 24;
#elif (WORD_SIZE == 2)
    n->array[0] = (i & 0x0000ffff);
    n->array[1] = (i & 0xffff0000) >> 16;
#elif (WORD_SIZE == 4)
    n->array[0] = i;
    DTYPE_TMP num_32 = 32;
    DTYPE_TMP tmp =
        i >> num_32; /* bit-shift with U64 operands to force 64-bit results */
    n->array[1] = tmp;
#endif
#endif
}

inline void bignum_init(struct bn *n)
{
    // require(n, "n is null");

    int i;
    for (i = 0; i < BN_ARRAY_SIZE; ++i) {
        n->array[i] = 0;
    }
}
/* -------------------------------------------- */
struct fabo_res {
    struct bn result;
    ktime_t ker_time_ns;
};

static void fib_sequence(long long k, char *buf, size_t size)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    ktime_t start, end;

    struct fabo_res res;
    memset(&res, 0,
           sizeof(struct fabo_res));  // TODO! even init the var, the result is
                                      // still faulty when k ~= 80

    struct bn tmp, f[k + 2];
    memset(&tmp, 0, sizeof(struct bn));
    memset(f, 0, sizeof(struct bn) * (k + 2));

    bignum_from_int(&f[0], 0);
    bignum_from_int(&f[1], 1);

    start = ktime_get_ns();
    for (int i = 2; i <= k; i++) {
        bignum_add(&f[i - 1], &f[i - 2], &tmp);
        bignum_assign(&f[i], &tmp);
    }
    end = ktime_get_ns();

    res.ker_time_ns = end - start;

    bignum_assign(&res.result, &f[k]);

    copy_to_user(buf, &res, size);
}

#elif defined ORIG_NUM

static long long fib_sequence(long long k, char *buf, size_t size)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    ktime_t start = ktime_get_ns(), end, tmp_diff;

    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    end = ktime_get_ns();

    tmp_diff = end - start;
    copy_to_user(buf, &tmp_diff, size);

    return f[k];
}

#endif

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number execution time  */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
#ifdef BIG_NUM

    fib_sequence(*offset, buf, size);
    return 0;

#elif defined ORIG_NUM

    return (ssize_t) fib_sequence(*offset, buf, size);

#endif
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    cdev_init(fib_cdev, &fib_fops);
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
