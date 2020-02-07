#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>   // kmalloc
#include <linux/moduleparam.h>
#include <linux/uaccess.h>  // copy_to_user

#include <linux/mutex.h>
//#include <linux/semaphore.h>

//#define __exit __attribute__((__section__(".exit.text")))
#define SIZE 0x1000
#define TDLCD_MAJOR 230
#define TDLCD_MINOR 0
#define TDLCD_NAME "tdlcd"
#define TDLCD_NUM 10
#define CTRL_CLC 0x1

struct tdlcd_dev
{
    struct cdev cdev;
    unsigned char mem[SIZE];
    struct mutex mutex;
};

static int tdlcd_major = TDLCD_MAJOR;
static int tdlcd_minor = TDLCD_MINOR;

module_param(tdlcd_major, int, S_IRUGO);

struct tdlcd_dev *tdlcd_devp;



/* functions*/

static int tdlcd_open(struct inode *inode, struct file *filp)
{

    // linux drivers follow an unspoken rule ,the private_data of
    // file is set to pointer to device struct
    struct tdlcd_dev *dev = container_of(inode->i_cdev, struct tdlcd_dev, cdev);
    filp->private_data = dev;
    return 0;
};
static int  tdlcd_release(struct inode *inode, struct file *filp)
{
    return 0;
};

static long tdlcd_ioctl(struct file *filp,
                        unsigned int cmd, unsigned long arg)
{
    struct tdlcd_dev *dev = filp->private_data;

    switch (cmd)
    {
    case CTRL_CLC:
        /* code */

        mutex_lock(&dev->mutex);
        memset(dev->mem, 0, SIZE);
        mutex_unlock(&dev->mutex);
        printk(KERN_INFO "tdlcd has set mem to zero\n");
        break;

    default:
        return -EINVAL;
    }
    return 0;
};
static ssize_t tdlcd_read(struct file *filp, char __user *buf, size_t size,
                          loff_t *ppos)
{
    unsigned long pos = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct tdlcd_dev *dev = filp->private_data;

    if (pos > SIZE)
    {
        return 0;
    }
    if (count > SIZE - pos)
    {
        count = SIZE - pos;
    }

    mutex_lock(&dev->mutex);
    if (copy_to_user(buf, dev->mem + pos, count))
    {
        ret = -EFAULT;
        
    }
    else
    {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read %u bytes from %lu\n", count, pos);
    }

    mutex_unlock(&dev->mutex);
    return ret;
};
static ssize_t tdlcd_write(struct file *filp, const char __user *buf,
                           size_t size, loff_t *ppos)
{
    unsigned long pos = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct tdlcd_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);
    if (pos > SIZE)
    {
        return 0;
    }
    if (count > SIZE - pos)
    {
        count = SIZE - pos;
    }

    
    if (copy_from_user(dev->mem + pos, buf, count))
    {
        ret = -EFAULT;
    }
    else
    {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "write %u bytes from %lu\n", count, pos);
    }

    mutex_unlock(&dev->mutex);
    return ret;
};
static loff_t tdlcd_llseek(struct file *filp, loff_t offset, int orig){
    	loff_t ret = 0;
	switch (orig) {
	case 0:
		if (offset < 0) {
			ret = -EINVAL;
			break;
		}
		if ((unsigned int)offset > SIZE) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1:
		if ((filp->f_pos + offset) > SIZE) {
			ret = -EINVAL;
			break;
		}
		if ((filp->f_pos + offset) < 0) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
};

/* file_operations */
static const struct file_operations tdlcd_fops = {
    .owner = THIS_MODULE,
    .llseek = tdlcd_llseek,
    .read = tdlcd_read,
    .write = tdlcd_write,
    .unlocked_ioctl = tdlcd_ioctl,
    .open = tdlcd_open,
    .release = tdlcd_release,
};

static void tdlcd_setup_cdev(struct tdlcd_dev *dev, int index)
{
    int err;
    int dev_num = MKDEV(tdlcd_major, index);

    cdev_init(&dev->cdev, &tdlcd_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, dev_num, 1);

    if (err)
    {
        printk(KERN_NOTICE "Error %d adding tdlcd%d", err, index);
    }
};

/*drivers modules*/

static int __init tdlcd_init(void)
{
    //1.0 init cdev
    int ret;
    dev_t dev_num = MKDEV(tdlcd_major, tdlcd_minor);

    if (tdlcd_major)
    {
        ret = register_chrdev_region(dev_num, TDLCD_NUM, TDLCD_NAME);
    }
    else
    {
        ret = alloc_chrdev_region(&dev_num, 0, TDLCD_NUM, TDLCD_NAME);
        tdlcd_major = MAJOR(dev_num);
    }

    if (ret < 0)
    {
        return ret;
    }

    tdlcd_devp = kzalloc(sizeof(struct tdlcd_dev)*TDLCD_NUM, GFP_KERNEL);
    if (!tdlcd_devp)
    {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    int i;
    for (i = 0; i < TDLCD_NUM; i++)
    {
        tdlcd_setup_cdev(tdlcd_devp + i, i);
        mutex_init(&(tdlcd_devp + i)->mutex);
    }
    return 0;

fail_malloc:
    unregister_chrdev_region(dev_num, TDLCD_NUM);
    return ret;
};
static void __exit tdlcd_exit(void)
{
    int i;
    for (i = 0; i < TDLCD_NUM; i++)
    {
        cdev_del(&(tdlcd_devp + i)->cdev);
    }
    cdev_del(&tdlcd_devp->cdev);
    kfree(tdlcd_devp);
    unregister_chrdev_region(MKDEV(tdlcd_major, 0), TDLCD_NUM);
};



module_init(tdlcd_init);
module_exit(tdlcd_exit);

MODULE_AUTHOR("hjl,UESTC");
MODULE_LICENSE("Dual BSD/GPL");