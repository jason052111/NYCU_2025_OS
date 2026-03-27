# Assignment I: Compiling a Custom Linux Kernel & Implementing New System Calls

**Author:** Student ID: 314551147

---

## 1. Compiling a Custom Linux Kernel

### 1.1 Screenshots

![Output of uname -a && cat /proc/version command](uname.png)
*Figure 1: Output of `uname -a` && `cat /proc/version` command*

![Output of make kernelrelease command](make_kernelrelease.png)
*Figure 2: Output of `make kernelrelease` command*

### 1.2 Steps Performed

#### Set Up Cross Compilation
```bash
export ARCH=riscv
export CROSS_COMPILE=riscv64-linux-gnu-
```

#### Configure the Kernel
```bash
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- defconfig
```

#### Modify Kernel Version Tag
Edit the `.config` file:
```bash
CONFIG_LOCALVERSION="-os-314551147"
```
Verify:
```bash
make kernelrelease
# Output: 6.1.0-os-314551147
```

#### Compile the Kernel
```bash
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -j$(nproc)
```
Generated image location:
```bash
arch/riscv/boot/Image
```

#### Run Kernel in QEMU
```bash
qemu-system-riscv64 -nographic -machine virt \
 -kernel linux/arch/riscv/boot/Image \
 -initrd initramfs.cpio.gz \
 -append "console=ttyS0 loglevel=3"
```

---

## 2. Q&A (Part 1)

### Main Differences Between RISC-V and x86
```text
* RISC-V is a RISC ISA, while x86 is a CISC ISA.
* RISC-V uses mostly fixed-length instructions, whereas x86 uses variable-length instructions.
* RISC-V is an open ISA that can be extended and customized, while x86 is proprietary and controlled by Intel/AMD.
* RISC-V provides 32 general-purpose registers with a clean, uniform design, while x86 historically has fewer general-purpose registers and more legacy complexity.
* Finally, x86 carries significant backward-compatibility baggage that increases hardware complexity, whereas RISC-V is designed to be simpler and more pipeline-friendly.
```

### Why Architecture Matters in Kernel Compilation
```text
Architecture differences matter because each CPU architecture has a different instruction set, ABI/calling convention, memory layout, and boot requirements, so the Linux kernel must be built for the correct target architecture. 

If you do not set the proper cross-compilation flags (e.g., ARCH=riscv and CROSS_COMPILE=riscv64-linux-gnu-), the build system will compile the kernel for the host architecture (typically x86_64) instead of RISC-V. As a result, the produced kernel image contains host-ISA instructions and cannot run on a RISC-V machine or QEMU RISC-V, leading to boot failure or an immediate crash due to an instruction-set mismatch.
```

### Why Docker Is Used
```text
Docker is used to provide a consistent and reproducible build environment for the assignment. 
1. Environment consistency: All students use the same toolchain and dependency versions, reducing "works on my machine" issues.
2. Isolation and safety: Kernel-building tools and configuration changes are contained inside the container and do not affect the host system.
3. Reproducibility and portability: The same container image can be reused on different machines to obtain the same results.
```

---

## 3. Implementing New System Calls

### 3.1 Screenshots

![Output of test_revstr command](test_revstr.png)
*Figure 3: Output of `test_revstr` command*

![Kernel dmesg output of sys_revstr](revstr_dmesg.png)
*Figure 4: Kernel dmesg output of `sys_revstr`*

![Output of test_tempbuf command](test_tempbuf.png)
*Figure 5: Output of `test_tempbuf` command*

![Kernel dmesg output of sys_tempbuf](tempbuf_dmesg.png)
*Figure 6: Kernel dmesg output of `sys_tempbuf`*

### 3.2 How System Calls Were Added

#### 1. Register System Call Numbers
**File:** `arch/riscv/include/uapi/asm/unistd.h`
```c
#define __NR_revstr   451
#define __NR_tempbuf  452
__SYSCALL(__NR_revstr, sys_revstr)
__SYSCALL(__NR_tempbuf, sys_tempbuf)
```

#### 2. Update System Call Count
**File:** `include/uapi/asm-generic/unistd.h`
```c
#define __NR_syscalls 453
```

#### 3. Modify Kernel Makefile
```makefile
obj-y += sys_tempbuf.o
obj-y += sys_revstr.o
```

#### 4. Declare Prototypes
**File:** `include/linux/syscalls.h`
```c
asmlinkage long sys_revstr(char __user *str, size_t n);
asmlinkage long sys_tempbuf(int mode, void __user *data, size_t size);
```

#### 5. Implement System Calls
Files added to: `linux/kernel/`
* `sys_revstr.c`
* `sys_tempbuf.c`

### 3.3 System Call Behavior

#### Details of `sys_revstr`
The `sys_revstr` system call is responsible for reversing a user-provided string within the kernel. It takes two parameters: a user pointer to a character buffer (`char __user *str`) and its length (`size_t n`). Because the kernel cannot directly dereference user pointers, the function first allocates a temporary kernel buffer using `kmalloc()` and safely copies the string from user space via `copy_from_user()`. This ensures that kernel memory accesses remain valid and protected from faults.

After successfully copying the input string, the kernel performs an in-place reversal using a standard two-pointer swapping technique: one pointer starts at the beginning of the buffer and the other at the end, and the characters are swapped until the two meet. This produces the reversed string entirely inside kernel-managed memory.

Once the reversal is complete, the modified string is copied back to user space using `copy_to_user()`, again ensuring safe and validated interaction with user memory. For debugging and verification, the function also prints both the original and reversed strings to the kernel log using `printk()`, allowing developers to inspect results via `dmesg`.

Finally, the temporary kernel buffer is released using `kfree()` to avoid memory leaks. The function returns `0` on success and a negative error code if any of the copy operations fail.

#### Details of `sys_tempbuf`
The `sys_tempbuf` system call manages a dynamic kernel-side string buffer implemented as a global linked list using `struct list_head`. Depending on the input mode, the function can either add a new string to the list or remove one or more matching entries.

Before performing any operation, the function validates the user input. If the `data` pointer is `NULL`, the size is zero, or `copy_from_user()` fails, the system call returns `-EFAULT` to indicate an invalid or unreadable user argument.

* **ADD Mode:** In ADD mode, the system call appends a new entry to the global buffer list. A new node is allocated using `kmalloc(size + 1, GFP_KERNEL)`. The extra byte ensures space for a null terminator. The string is then copied from user space using `copy_from_user()`, and once the copy succeeds, `list_add_tail()` is used to append the node to the end of the list. This allows the buffer to grow dynamically with each invocation.
* **REMOVE Mode:** In REMOVE mode, the system call searches for entries in the list whose stored string matches the user-provided string. To safely traverse and modify the list, the function uses `list_for_each_entry_safe()`, which supports deletion during iteration. When a match is found, the node is removed using `list_del()` and its memory is freed with `kfree()`. This ensures that the internal list structure remains consistent and that removed entries do not leak kernel memory.

---

## 4. Q&A (Part 2)

### What does `syscalls.h` do?
```text
include/linux/syscalls.h declares the function prototypes for system calls provided by the Linux kernel. When adding a new system call, its prototype must be added to this header so other kernel code can reference it with the correct signature, ensuring successful compilation and proper linkage between the syscall number and its implementation.
```

### System Call vs GLIBC Call
```text
* A system call is a direct interface for a user program to request privileged services from the kernel, which switches execution from user space to kernel space.
* A glibc library call is a user-space wrapper provided by glibc that may add buffering, portability, and error handling on top of system calls. For example, the glibc function printf() ultimately outputs data by invoking the write() system call (SYS_write) on standard output.
```

### Static vs Dynamic Linking
```text
* Static linking copies all required library code into the executable at compile time, producing a self-contained binary. It usually results in a larger executable, but it does not depend on shared libraries at runtime.
* Dynamic linking keeps only references to shared libraries in the executable and loads the required .so libraries at runtime via the dynamic linker. This typically produces a smaller executable and allows library updates without recompiling, but it requires the correct shared libraries to be present on the target system.
```

### Why Use `-static` in This Assignment?
```text
We must compile the test programs with -static because the assignment runtime environment (a minimal RISC-V rootfs/initramfs under QEMU) typically does not include the dynamic loader or the required shared libraries. Static linking bundles the needed library code into the executable so it can run in this minimal system. If we omit -static, the program will be dynamically linked and will fail to start due to missing loader or shared libraries, producing errors such as "error while loading shared libraries" or "No such file or directory".
```
