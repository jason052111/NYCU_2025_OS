// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mutex.h>

struct temp_entry {
	struct list_head list;
	size_t len;
	char *str;
};

// tempbuf_head is save in sys_tempbuf model, it is define in static, so it can always be access
static LIST_HEAD(tempbuf_head);  // = static struct list_head tempbuf_head = { &tempbuf_head, &tempbuf_head };
static DEFINE_MUTEX(tempbuf_lock);

enum tempbuf_mode {
	PRINT = 0,
	ADD   = 1,
	REMOVE= 2,
};

SYSCALL_DEFINE3(tempbuf, int, mode, void __user *, data, size_t, size)
{
	int ret = 0;

	switch ((enum tempbuf_mode)mode) {
	case ADD: {
		char *kstr;
		struct temp_entry *ent;

		if (!data || size == 0)
			return -EFAULT;

		kstr = kmalloc(size + 1, GFP_KERNEL);
		if (!kstr)
			return -ENOMEM;

		if (copy_from_user(kstr, data, size)) {
			kfree(kstr);
			return -EFAULT;
		}
		kstr[size] = '\0';

		ent = kmalloc(sizeof(*ent), GFP_KERNEL);
		if (!ent) {
			kfree(kstr);
			return -ENOMEM;
		}
		ent->str = kstr;
		ent->len = size;

		mutex_lock(&tempbuf_lock);
		list_add_tail(&ent->list, &tempbuf_head);
		mutex_unlock(&tempbuf_lock);

		pr_info("[tempbuf] Added: %s\n", kstr);
		return 0;
	}

	case REMOVE: {
		char *key;
		struct temp_entry *ent, *tmp;
		bool found = false;

		if (!data || size == 0)
			return -EFAULT;

		key = kmalloc(size + 1, GFP_KERNEL);
		if (!key)
			return -ENOMEM;

		if (copy_from_user(key, data, size)) {
			kfree(key);
			return -EFAULT;
		}
		key[size] = '\0';

		mutex_lock(&tempbuf_lock);
		list_for_each_entry_safe(ent, tmp, &tempbuf_head, list) {
			if (ent->len == size && strcmp(ent->str, key) == 0) {
				list_del(&ent->list);
				pr_info("[tempbuf] Removed: %s\n", ent->str);
				kfree(ent->str);
				kfree(ent);
				found = true;
				break; 
			}
		}
		mutex_unlock(&tempbuf_lock);

		kfree(key);
		return found ? 0 : -ENOENT;
	}

	case PRINT: {
		size_t cap, used = 0;
		char *kbuf;
		struct temp_entry *ent;
		bool first = true;

		if (!data || size == 0)
			return -EFAULT;

		cap = size;
		if (cap > 512)
			cap = 512;
		kbuf = kmalloc(cap + 1, GFP_KERNEL);
		if (!kbuf)
			return -ENOMEM;

		mutex_lock(&tempbuf_lock);
		list_for_each_entry(ent, &tempbuf_head, list) {
			if (!first) {
				if (used + 1 > cap)
					break;
				kbuf[used++] = ' ';
			}
			first = false;

			if (ent->len) {
				size_t to_copy = ent->len;
				if (to_copy > cap - used)
					to_copy = cap - used;
				memcpy(kbuf + used, ent->str, to_copy);
				used += to_copy;

				if (used == cap)
					break;
			}
		}
		mutex_unlock(&tempbuf_lock);

		kbuf[used] = '\0';

		if (copy_to_user(data, kbuf, used + 1)) {
			kfree(kbuf);
			return -EFAULT;
		}

		pr_info("[tempbuf] %s\n", kbuf);
		kfree(kbuf);
		return (int)used; 
	}

	default:
		return -EINVAL;
	}
}

