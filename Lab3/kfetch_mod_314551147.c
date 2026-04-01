#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/utsname.h> 
#include <linux/mm.h>      
#include <linux/cpumask.h> 
#include <linux/sched/signal.h> 
#include <linux/ktime.h>   
#include <linux/of.h>

#define DEVICE_NAME "kfetch"
#define KFETCH_CLASS_NAME "kfetch_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("314551147");
MODULE_DESCRIPTION("System Information Fetching Kernel Module");

#define KFETCH_NUM_INFO 6
#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)
#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)

#define LOGO_LINES 7
static const char *kfetch_logo[] = {
"                    ",
"        .-.         ",
"       (.. |        ",
"       <>  |        ",
"      / --- \\       ",
"     ( |   | )      ",
"   |\\\\_)__(_//|     ",
"  <__)------(__>    "
};

static int major;
static struct cdev kfetch_cdev;
static struct class *kfetch_class;
static DEFINE_MUTEX(kfetch_mutex);

static int current_mask = KFETCH_FULL_INFO; 

struct kfetch_data {
    int mask;
    char kbuf[2048];
};

static int get_procs_count(void) {
    struct task_struct *task;
    int count = 0;
    rcu_read_lock();
    for_each_process(task) {
        count++;
    }
    rcu_read_unlock();
    return count;
}

static void fetch_info(struct kfetch_data *data) {
    struct sysinfo si;
    struct timespec64 uptime;
    char *buf = data->kbuf;
    char *pos = buf;
    
    char info_lines[8][128]; 
    int info_cnt = 0;
    int i;

    snprintf(info_lines[info_cnt++], 128, "%s", init_uts_ns.name.nodename);

    {
        int host_len = strlen(init_uts_ns.name.nodename);
        int j;
        for(j=0; j<host_len; j++) info_lines[info_cnt][j] = '-';
        info_lines[info_cnt][j] = '\0';
        info_cnt++;
    }

    if (data->mask & KFETCH_RELEASE) {
        snprintf(info_lines[info_cnt++], 128, "Kernel: %s", init_uts_ns.name.release);
    }

    if (data->mask & KFETCH_CPU_MODEL) {
        struct device_node *np;
        const char *isa_str = NULL;

        np = of_find_node_by_type(NULL, "cpu");
        if (np) {
            if (of_property_read_string(np, "riscv,isa", &isa_str) != 0) {
                isa_str = NULL;
            }
            
            of_node_put(np);
        }

        snprintf(info_lines[info_cnt++], 128, "CPU: %s", 
                 isa_str ? isa_str : "RISC-V Processor");    }

    if (data->mask & KFETCH_NUM_CPUS) {
        snprintf(info_lines[info_cnt++], 128, "CPUs: %d / %d", num_online_cpus(), num_possible_cpus());
    }

    if (data->mask & KFETCH_MEM) {
        si_meminfo(&si);
        unsigned long total_mb = (si.totalram * si.mem_unit) / 1024 / 1024;
        unsigned long free_mb = (si.freeram * si.mem_unit) / 1024 / 1024;
        snprintf(info_lines[info_cnt++], 128, "Mem: %lu MB / %lu MB", free_mb, total_mb);
    }

    if (data->mask & KFETCH_NUM_PROCS) {
        snprintf(info_lines[info_cnt++], 128, "Procs: %d", get_procs_count());
    }

    if (data->mask & KFETCH_UPTIME) {
        ktime_get_boottime_ts64(&uptime);
        snprintf(info_lines[info_cnt++], 128, "Uptime: %llu mins", uptime.tv_sec / 60);
    }

    int max_lines = (LOGO_LINES > info_cnt) ? LOGO_LINES : info_cnt;

    for (i = 0; i < max_lines; i++) {
        if (i < LOGO_LINES) {
            pos += sprintf(pos, "%s", kfetch_logo[i]);
        } else {
            pos += sprintf(pos, "%-20s", " "); 
        }

        if (i < info_cnt) {
            pos += sprintf(pos, "%s", info_lines[i]);
        }
        pos += sprintf(pos, "\n");
    }
    *pos = '\0';
}

static int kfetch_open(struct inode *inode, struct file *file) {
    struct kfetch_data *data;

    data = kmalloc(sizeof(struct kfetch_data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    if (mutex_lock_interruptible(&kfetch_mutex)) {
        kfree(data);
        return -ERESTARTSYS;
    }
    data->mask = current_mask;
    mutex_unlock(&kfetch_mutex);

    memset(data->kbuf, 0, sizeof(data->kbuf));
    file->private_data = data;
    return 0;
}

static int kfetch_release(struct inode *inode, struct file *file) {
    struct kfetch_data *data = file->private_data;
    if (data) kfree(data);
    return 0;
}

static ssize_t kfetch_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
    struct kfetch_data *data = filp->private_data;
    size_t data_len;

    if (*offset > 0) return 0;

    if (mutex_lock_interruptible(&kfetch_mutex)) return -ERESTARTSYS;
    memset(data->kbuf, 0, sizeof(data->kbuf));
    fetch_info(data);
    mutex_unlock(&kfetch_mutex);

    data_len = strlen(data->kbuf);
    if (length < data_len) data_len = length; 

    if (copy_to_user(buffer, data->kbuf, data_len)) {
        return -EFAULT;
    }

    *offset += data_len;
    return data_len;
}

static ssize_t kfetch_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) {
    struct kfetch_data *data = filp->private_data;
    int mask_info;

    if (length != sizeof(int)) return -EINVAL;

    if (copy_from_user(&mask_info, buffer, length)) {
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&kfetch_mutex)) return -ERESTARTSYS;

    data->mask = mask_info;
    current_mask = mask_info;
    
    mutex_unlock(&kfetch_mutex);

    return length;
}

static const struct file_operations kfetch_ops = {
    .owner = THIS_MODULE,
    .open = kfetch_open,
    .release = kfetch_release,
    .read = kfetch_read,
    .write = kfetch_write,
};

static int __init kfetch_init(void) {
    dev_t dev;
    int ret;

    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;
    major = MAJOR(dev);

    cdev_init(&kfetch_cdev, &kfetch_ops);
    kfetch_cdev.owner = THIS_MODULE;
    ret = cdev_add(&kfetch_cdev, dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    kfetch_class = class_create(KFETCH_CLASS_NAME);
    if (IS_ERR(kfetch_class)) {
        cdev_del(&kfetch_cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(kfetch_class);
    }

    if (device_create(kfetch_class, NULL, dev, NULL, DEVICE_NAME) == NULL) {
        class_destroy(kfetch_class);
        cdev_del(&kfetch_cdev);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    current_mask = KFETCH_FULL_INFO;

    printk(KERN_INFO "kfetch: loaded\n");
    return 0;
}

static void __exit kfetch_exit(void) {
    dev_t dev = MKDEV(major, 0);
    device_destroy(kfetch_class, dev);
    class_destroy(kfetch_class);
    cdev_del(&kfetch_cdev);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO "kfetch: unloaded\n");
}

module_init(kfetch_init);
module_exit(kfetch_exit);

