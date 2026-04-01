# Course Summary: Operating Systems

---

## 1. Course Overview
Throughout this course, we explored the deep internals of the Linux Operating System, bridging the gap between user-space applications and kernel-space management. The hands-on assignments provided a comprehensive journey from configuring and cross-compiling a custom RISC-V Linux kernel, to manipulating thread scheduling policies, and finally developing and registering a custom character device driver as a Loadable Kernel Module (LKM).

---

## 2. Key Learnings by Module

### 2.1 Kernel Compilation & System Calls (Assignment I)
* **Cross-Compilation & Emulation:** Mastered the process of configuring (`defconfig`), modifying, and cross-compiling the Linux kernel for the RISC-V architecture. Successfully deployed and tested the custom kernel using QEMU with a minimal `initramfs`.
* **System Call Implementation:** Learned the complete lifecycle of adding new system calls (`sys_revstr`, `sys_tempbuf`) to the Linux kernel, including updating syscall tables (`unistd.h`, `syscalls.h`) and modifying kernel Makefiles.
* **Safe User-Kernel Data Transfer:** Gained hands-on experience with `copy_from_user()` and `copy_to_user()` to securely pass data across the user-kernel boundary without causing kernel panics.
* **Kernel Data Structures:** Utilized kernel-specific memory allocation (`kmalloc`/`kfree`) and manipulated the kernel's built-in doubly linked list (`struct list_head`) for dynamic state management.

### 2.2 Thread Scheduling & CPU Affinity (Assignment II)
* **POSIX Threads & CPU Affinity:** Used `pthreads` to spawn multiple worker threads and utilized `sched_setaffinity` to bind them to a specific CPU core (Core 0), forcing direct contention to observe scheduling behaviors.
* **Scheduling Policies & Priorities:** Configured thread attributes (`pthread_attr_t`) to test and observe the differences between the Completely Fair Scheduler (`SCHED_OTHER`/`SCHED_NORMAL`) and real-time scheduling (`SCHED_FIFO`).
* **Precise CPU-Time Measurement:** Implemented an accurate CPU-bound busy-wait mechanism using `clock_gettime()` with `CLOCK_THREAD_CPUTIME_ID`, successfully calculating pure execution time while excluding preempted time.
* **Real-Time Bandwidth Control:** Understood the impact of Linux's real-time throttling (`/proc/sys/kernel/sched_rt_runtime_us`) and how to bypass it to observe uninterrupted FIFO thread execution.

### 2.3 Kernel Modules & Device Drivers (Assignment III)
* **Linux Kernel Modules (LKM):** Developed a dynamic kernel module (`kfetch`) that can be loaded and unloaded at runtime without recompiling the entire kernel.
* **Character Device Registration:** Implemented standard device driver initialization utilizing `alloc_chrdev_region`, `cdev_init`, `cdev_add`, `class_create`, and `device_create` for automatic node generation in `/dev/`.
* **File Operations (`file_operations`):** Mapped standard user-space system calls (`open`, `read`, `write`, `close`) to custom kernel functions (`kfetch_open`, `kfetch_read`, etc.). 
* **Concurrency & State Management:** Solved race conditions using `mutex_lock_interruptible` and achieved proper per-process state isolation by utilizing the `file->private_data` pointer.
* **Standard Tool Compatibility:** Managed file offsets (`loff_t *ppos`) correctly within the `read` function, ensuring the driver responds accurately to standard Linux CLI tools like `cat`.

---

## 3. Core Competencies Acquired

| Skill Category | Specific Techniques Learned |
| :--- | :--- |
| **Kernel Programming** | `copy_from_user`/`copy_to_user`, `kmalloc`/`kfree`, `struct list_head` manipulation, syscall table modification. |
| **Process & Thread Management** | `pthreads`, `pthread_barrier_t`, `sched_setaffinity`, `SCHED_FIFO`, `CLOCK_THREAD_CPUTIME_ID`. |
| **Device Driver Development** | Character devices (`cdev`), dynamic major numbers, `struct file_operations`, `file->private_data`, managing `loff_t *ppos`. |
| **Concurrency Control** | Identifying critical sections in kernel space, utilizing `mutex_lock_interruptible` to prevent race conditions. |
| **Build Systems & Emulation** | Cross-compiling for RISC-V, QEMU headless booting, static vs. dynamic linking (`-static`), Makefile configuration. |

---

## 4. Conclusion
This sequence of assignments provided a rigorous, practical understanding of operating system design. By moving progressively from static kernel compilation to dynamic thread scheduling, and finally to writing loadable device drivers, we developed the crucial ability to safely and efficiently interact with hardware and system resources from both sides of the user-kernel divide.
