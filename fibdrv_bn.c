#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static ktime_t kt_proxy;

#define BN_SIZE 6

typedef struct bn {
    unsigned long long numbers[BN_SIZE];
} bn;

char *bn_to_string(bn src)
{
    size_t len = (64 * BN_SIZE) / 3 + 2;
    char *s = kmalloc(len, GFP_KERNEL);
    char *p = s;

    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    for (int i = BN_SIZE - 1; i >= 0; i--) {
        for (unsigned long long d = 1ull << 63; d; d >>= 1) {
            int carry = ((d & src.numbers[i]) > 0);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;
                carry = (s[j] > '9');
                if (carry)
                    s[j] -= 10;
            }
        }
    }
    while (p[0] == '0' && p[1] != '\0') {
        p++;
    }

    memmove(s, p, strlen(p) + 1);
    return s;
}

void bn_init(bn *src)
{
    for (int i = 0; i < BN_SIZE; i++) {
        src->numbers[i] = 0;
    }
}

void bn_add(bn *output, bn *x, bn *y)
{
    for (int i = 0; i < BN_SIZE; i++) {
        if (x->numbers[i] > ~output->numbers[i]) {
            if (i + 1 < BN_SIZE)
                output->numbers[i + 1] = 1;
        }
        output->numbers[i] += x->numbers[i];

        if (y->numbers[i] > ~output->numbers[i]) {
            if (i + 1 < BN_SIZE)
                output->numbers[i + 1] = 1;
        }
        output->numbers[i] += y->numbers[i];
    }
}

bn fib_sequence(long long k)
{
    bn lastlast, last, cur;
    bn_init(&lastlast);
    bn_init(&last);
    last.numbers[0] = 1;

    if (k == 0)
        return lastlast;

    if (k == 1)
        return last;

    for (int i = 2; i <= k; i++) {
        bn_init(&cur);
        bn_add(&cur, &last, &lastlast);

        lastlast = last;
        last = cur;
    }

    return cur;
    /*
        bn f[k + 2];
        bn_init(&f[0]);
        bn_init(&f[1]);
        f[1].numbers[0] = 1;
        for (int i = 2; i <= k; i++) {
            bn_init(&f[i]);
            //bn_add(&f[i], &f[i - 2], &f[i - 1]);
        }
        return f[k];
    */
}

bn fib_sequence_ff(long long k)
{
    // for (int i = 32 - __builtin_clz(k); i > 0; i--) {
    // if (k & (1 << (i - 1))) {
    //}
    //}
    return fib_sequence(k);
}

bn fib_time_proxy(long long k)
{
    kt_proxy = ktime_get();
    bn result = fib_sequence_ff(k);
    kt_proxy = ktime_sub(ktime_get(), kt_proxy);
    return result;
}

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

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    bn result = fib_time_proxy(*offset);

    char *fib = bn_to_string(result);

    copy_to_user(buf, fib, strlen(fib) + 1);

    kfree(fib);

    return ktime_to_ns(kt_proxy);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t kt;
    // bn fib;
    switch (size) {
    case 0:
        kt = ktime_get();
        fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 1:
        kt = ktime_get();
        fib_sequence_ff(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 2:
        kt = ktime_get();
        break;
    default:
        return 0;
    }
    return (ssize_t) ktime_to_ns(kt);
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
    fib_cdev->ops = &fib_fops;
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
