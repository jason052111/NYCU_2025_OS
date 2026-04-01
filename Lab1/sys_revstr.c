#include <linux/kernel.h>    // printk
#include <linux/syscalls.h>  // SYSCALL_DEFINE2
#include <linux/slab.h>      // kmalloc, kfree
#include <linux/uaccess.h>   // copy_from_user, copy_to_user

SYSCALL_DEFINE2(revstr, char __user *, usr_str, size_t, n)
{
    char *kbuf;
    size_t i, j;

    if (!usr_str || n == 0)
        return 0;

    // kmalloc is link malloc, is use for kernel space 
    // GFP_KERNEL means normal allocation
    kbuf = kmalloc(n + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    // copy_from_user() : safely copy data from user-space to kernel buffer
    // copy_from_user() success will return 0
    if (copy_from_user(kbuf, usr_str, n)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[n] = '\0';

    printk(KERN_INFO "The origin string: %s\n", kbuf);

    i = 0;
    j = (n > 0) ? n - 1 : 0;
    while (i < j) {
        char tmp = kbuf[i];
        kbuf[i] = kbuf[j];
        kbuf[j] = tmp;
        i++;
        j--;
    }

    printk(KERN_INFO "The reversed string: %s\n", kbuf);

    // copy_to_user() : safely copy data from kernel buffer to user-space 
    // copy_to_user success will return 0
    if (copy_to_user(usr_str, kbuf, n)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kfree(kbuf);
    return 0;
}

