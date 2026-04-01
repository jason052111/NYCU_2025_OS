# Linux Kernel Programming & System Implementation
> **Course Project Portfolio** > This project covers three major domains of Linux kernel development: Kernel compilation and architectural understanding, system call implementation with scheduling analysis, and kernel module (driver) development.

---

## 🎓 Key Learning Outcomes

Through these hands-on assignments, I have mastered the following critical low-level technologies:

* **Kernel Infrastructure**: Gained proficiency in setting up consistent **RISC-V cross-compilation** environments using Docker and testing kernel images via the **QEMU** emulator.
* **System Call Mechanisms**: Deepened my understanding of the isolation between **User Space** and **Kernel Space**, including registering new syscalls and implementing security checks like `copy_from_user` and `copy_to_user`.
* **Linux Scheduling Logic**: Analyzed behavioral differences between the **CFS (Completely Fair Scheduler)** and **Real-time (SCHED_FIFO)** schedulers, specifically focusing on CPU affinity and priority preemption.
* **Kernel Module Development**: Mastered the architecture of **Character Device Drivers**, covering major/minor number management, `file_operations` implementation, concurrency control via **Mutexes**, and kernel memory management.

---

## 🛠 Project Scope & Implementation Details

### I. Custom Linux Kernel & System Calls
Constructed a custom kernel for the RISC-V architecture and extended its functionality with new system calls.

* **Technologies**: RISC-V Toolchain, QEMU, `unistd.h` syscall table modification.
* **Implemented Syscalls**:
    1.  `sys_revstr`: Reverses a string at the kernel level, utilizing kernel dynamic memory allocation (`kmalloc/kfree`).
    2.  `sys_tempbuf`: Implements a kernel-level dynamic linked list (`struct list_head`) to provide a cross-process data caching mechanism.
* **Key Concept**: Understood why `-static` linking is mandatory for minimal environments lacking dynamic loaders.

### II. Multi-threaded Scheduling Demonstration
Developed a tool to observe how the Linux kernel prioritizes tasks under different scheduling policies.

* **Technologies**: `pthread` concurrency, `sched_setaffinity` for CPU pinning, `CLOCK_THREAD_CPUTIME_ID` for precise execution tracking.
* **Key Observations**:
    * **Priority Preemption**: Verified that high-priority `SCHED_FIFO` threads completely preempt lower-priority tasks.
    * **Bandwidth Control**: Analyzed how `kernel.sched_rt_runtime_us` prevents real-time tasks from causing system-wide hangs.
    * **Busy-Waiting**: Implemented a logic that calculates "N seconds of actual CPU time," excluding time spent while the thread was preempted.

### III. kfetch: System Information Kernel Module
Developed `kfetch`, a character device driver that exposes system telemetry through a graphical ASCII interface.

* **Technologies**: `cdev` registration, `device_create` for automatic node generation, `private_data` for session isolation.
* **Highlights**:
    * **Session Management**: Used `file->private_data` to ensure that info-masks are isolated per process, preventing data pollution.
    * **Tool Compatibility**: Correctly handled `loff_t *ppos` (offset) to ensure full compatibility with standard Unix tools like `cat` and `head`.
    * **Concurrency**: Implemented mutex locks to protect global variables from race conditions.

---

## 🚀 Quick Start

### Prerequisites
* Docker (with cross-riscv64 toolchain)
* QEMU (riscv64-system)

### Build the Kernel
```bash
export ARCH=riscv
export CROSS_COMPILE=riscv64-linux-gnu-
make defconfig
make -j$(nproc)
