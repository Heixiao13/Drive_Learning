/**
 * Linux设备驱动开发详解 6.3.6  代码清单6.17 使用文件私有数据的globalmem设备驱动
 */ 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
//#include <asm/system.h>
// #include <asm/uacess.h>
#include <linux/slab.h>
 
 
#define GLOBALMEM_SIZE 0x1000 /* 全局内存最大4K */
 
#define MEM_CLEAR 0x1 /* 清零全局内存 */
 
#define GLOBALMEM_MAJOR 255 /* 预设的globalmem主设备号 */
 
static int globalmem_major = GLOBALMEM_MAJOR;
 
/*globalmem设备结构体*/
struct globalmem_dev {
    struct cdev cdev; /*cdev结构体*/
    unsigned char mem[GLOBALMEM_SIZE];/*全局内存*/
};
 
struct globalmem_dev *globalmem_devp; /*设备结构体指针*/
 
/*文件打开函数*/
int globalmem_open(struct inode *inode, struct file *filep){
    /*将设备结构体指针赋值给文件私有指针*/
    filep->private_data = globalmem_devp;
    return 0;
}
 
/*文件释放函数*/
int globalmem_release(struct inode *inode, struct file *filep){
    return 0;
}
 
 
/*读取函数*/
static ssize_t globalmem_read(struct file *filep, char __user *buf, size_t size, loff_t *ppos){
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filep->private_data; /*获得设备结构体指针*/
 
    /*分析和获取有效的写长度*/
    if(p >= GLOBALMEM_SIZE){
        return count ? -ENXIO : 0;
    }
    if(count > GLOBALMEM_SIZE - p){
        count = GLOBALMEM_SIZE - p;
    }
 
    /*内核空间->用户空间*/
    if(copy_to_user(buf, (void*)(dev->mem + p), count)){
        ret = -EFAULT;
    }
    else{
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read %d bytes(s) from %ld\n", count, p);
    }
    return ret;
}
 
static ssize_t globalmem_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos){
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filep->private_data;/*获得设备结构体指针*/
 
    /*分析和获得有效的写长度*/
    if(p >= GLOBALMEM_SIZE){
        return count ? -ENXIO:0;
    }
    if(count > GLOBALMEM_SIZE - p){
        count = GLOBALMEM_SIZE - p;
    }
 
    /*用户空间->内核空间*/
    if(copy_from_user(dev->mem + p, buf, count)){
        ret = -EFAULT;
    }
    else{
        *ppos += count;
        ret = count;
 
        printk(KERN_INFO "written %d bytes(s) from %ld\n", count, p);
    }
    return ret;
}
 
/*seek文件定位函数*/
static loff_t globalmem_llseek(struct file *filep, loff_t offset, int orig){
    loff_t ret = 0;
    switch(orig){
        case 0: /*相对文件开始位置偏移*/
            if(offset < 0){
                ret = -EINVAL;
                break;
            }
            if((unsigned int)offset > GLOBALMEM_SIZE){
                ret = -EINVAL;
                break;
            }
            filep->f_pos = (unsigned int)offset;
            ret = filep->f_pos;
            break;
        case 1: /*相对文件当前位置偏移*/
            if((filep->f_pos + offset) > GLOBALMEM_SIZE){
                ret = -EINVAL;
                break;
            }
            if((filep->f_pos+offset) < 0){
                ret = -EINVAL;
                break;
            }
            filep->f_pos += offset;
            ret = filep->f_pos;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}
 
/*文件操作结构体*/
static const struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .llseek = globalmem_llseek,
    .read = globalmem_read,
    .write = globalmem_write,
    /* .ioctl = globalmem_ioctl, */
    .open = globalmem_open,
    .release = globalmem_release
};
 
/*初始化并注册cdev*/
static void globalmem_setup_cdev(struct globalmem_dev *dev, int index){
    int err, devno = MKDEV(globalmem_major, index);
 
    cdev_init(&dev->cdev, &globalmem_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &globalmem_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if(err){
        printk(KERN_NOTICE "Error %d adding LED%d", err, index);
    }
}
 
/*设备驱动模块加载函数*/
int globalmem_init(void){
    int result;
    dev_t devno = MKDEV(globalmem_major, 0);
 
    /*申请设备号*/
    if(globalmem_major){
        result = register_chrdev_region(devno, 1, "globalmem");
    }
    else{ /*动态申请设备号*/
        result = alloc_chrdev_region(&devno, 0, 1, "globalmem");
        globalmem_major = MAJOR(devno);
    }
    if(result < 0){
        return result;
    }
 
    /*动态申请设备结构体的内存*/
    globalmem_devp = kmalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
    if(!globalmem_devp){ /*申请失败*/
        result = -ENOMEM;
        goto fail_malloc;
    }
 
    memset(globalmem_devp, 0, sizeof(struct globalmem_dev));
 
    globalmem_setup_cdev(globalmem_devp, 0);
    return 0;
 
    fail_malloc:
    unregister_chrdev_region(devno, 1);
    return result;
}
 
/*模块卸载函数*/
void globalmem_exit(void){
    cdev_del(&globalmem_devp->cdev);/*注销cdev*/
    kfree(globalmem_devp);/*释放设备结构体内存*/
    unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);/*释放设备号*/
}
 
MODULE_AUTHOR("Heixiao");
MODULE_LICENSE("GPL");
module_param(globalmem_major, int, S_IRUGO);
module_init(globalmem_init);
module_exit(globalmem_exit);