#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static void xor_swap(long long *x, long long *y)
{
    *x = *x ^ *y;
    *y = *y ^ *x;
    *x = *x ^ *y;
}

static long long k_to_u(char *buf, long long val, unsigned long len)
{
    // How many pages do I need to allocate?
    char *ker = vmalloc(16);
    if (!ker) {
        printk(KERN_ALERT "no memory allocated in kernel");
        return -ENOMEM;
    }

    snprintf(ker, len, "%lu", (unsigned long) val);

    if (copy_to_user(buf, ker, len) != 0)
        return -EFAULT;

    vfree(ker);
    return 0;
}

static ssize_t fib_basic(long long k, char *buf, size_t size)
{
    long long f[3];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[2] = f[0] + f[1];
        xor_swap(&f[0], &f[1]);
        f[1] = f[2];
    }

    if (k_to_u(buf, f[2], size) != 0)
        printk("copy from kernel to user failed");

    printk("%lld: %lld", k, f[2]);
    return f[2];
}

static ssize_t fib_fd(long long n)
{
    if (n == 0)
        return 0;

    if (n <= 2)
        return 1;

    // odd: F(n) = F(2k+1) = F(k+1)^2 + F(k)^2
    if (n & 0x01) {
        unsigned int k = (n - 1) / 2;
        return fib_fd(k + 1) * fib_fd(k + 1) + fib_fd(k) * fib_fd(k);
    } else {
        // even: F(n) = F(2k) = F(k) * [ 2 * F(k+1) – F(k) ]
        unsigned int k = n / 2;
        return fib_fd(k) * ((2 * fib_fd(k + 1)) - fib_fd(k));
    }
}

static ssize_t fib_fast_doubling(long long k, char *buf, size_t size)
{
    ssize_t result = fib_fd(k);

    if (k_to_u(buf, result, size) != 0)
        printk("copy from kernel to user failed");

    printk("%lld: %lld", k, (long long) result);
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

static long long fib_time_proxy(long long k)
{
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (ssize_t) fib_time_proxy(*offset);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    fib_basic(*offset, buf, size);
    ktime_t start_time = ktime_get();
    fib_fast_doubling(*offset, buf, size);
    // fib_basic(*offset, buf, size);
    ktime_t end_time = ktime_get();
    ktime_t process_time = ktime_sub(end_time, start_time);

    return (ssize_t) ktime_to_ns(process_time);
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
