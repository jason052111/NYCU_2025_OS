# Assignment III: System Information Fetching Kernel Module

Lab instructions : https://hackmd.io/@seco1024/BJYIDp7lZx

---

## 1. Program Description

The objective of this project is to implement a Linux Kernel Module named `kfetch`. This module registers a character device in the system at the path `/dev/kfetch`.

Its primary function is similar to the popular user-space tool `neofetch`, but it operates within the **Kernel Space**. Users interact with this device via a user-space application (`kfetch.c`), allowing them to configure the type of information to be displayed (such as CPU model, memory size, kernel version, etc.) and read the formatted graphical system information (including an ASCII Art Logo) directly from the kernel.

### System Architecture
The entire system is divided into two parts:

* **Kernel Space (`kfetch_mod.ko`):** Responsible for collecting system information, managing device files, handling concurrency, and maintaining private data for each open file session.
* **User Space (`kfetch`):** Responsible for parsing command-line arguments, setting the information mask, and reading the final output result.

---

## 2. Implementation Details

The core implementation of this module lies in the definition of the `file_operations` structure and the specific logic of each operation function. The key processes are described below:

### 2.1 Module Initialization & Exit
To achieve automatic device node creation and proper registration, standard character device registration procedures are adopted in `kfetch_init` and `kfetch_exit`.

* **Initialization (`kfetch_init`):**
  1. **Acquire Device Number:** Uses `alloc_chrdev_region` to dynamically request a major device number, avoiding conflicts with existing system devices.
  2. **Register Character Device (`cdev`):** Uses `cdev_init` and `cdev_add` to bind the `file_operations` to the device number, informing the kernel how to operate this device.
  3. **Automatic Device Node Creation:** Uses `class_create` and `device_create` to notify the `udev` system to automatically generate the `kfetch` file under the `/dev/` directory, eliminating the need for manual `mknod` execution.
  4. **Initialize Global Variables:** Sets the default information mask (`current_mask`) to display all information (`KFETCH_FULL_INFO`).

* **Exit (`kfetch_exit`):**
  Strictly adheres to the **LIFO (Last In First Out)** principle for resource release. It sequentially executes `device_destroy`, `class_destroy`, `cdev_del`, and `unregister_chrdev_region` to prevent memory leaks.

### 2.2 File Operations
The following four key behaviors are defined via the `struct file_operations`:

#### A. Open Device (`kfetch_open`)
When a user executes `open()`:
* A `struct kfetch_data` is allocated using `kmalloc`.
* This structure is bound to the current file session using the `file->private_data` pointer. This ensures that when multiple users access the device simultaneously, their settings (Mask) and data buffers are isolated and do not interfere with each other.

#### B. Set Information Mask (`kfetch_write`)
When a user executes `write()` to pass configuration parameters (Mask):
* **Parameter Validation:** Checks if the write length matches `sizeof(int)` to prevent invalid data.
* **Data Transfer:** Uses `copy_from_user` to safely copy the Mask value from User Space to Kernel Space.
* **Concurrency Control:** Uses `mutex_lock_interruptible` to lock the critical section, preventing Race Conditions caused by multiple processes modifying global variables simultaneously.
* **State Update:** Updates both `file->private_data->mask` (for the current user) and `current_mask` (global default).

#### C. Read System Information (`kfetch_read`)
This is the core function responsible for generating and returning data.
* **Data Generation:** Based on the stored Mask, kernel APIs (such as `utsname()`, `si_meminfo()`) are called to retrieve system information. `snprintf` is used to assemble the ASCII Logo and text information into the kernel buffer.
* **Offset Handling:** This is a critical design to support standard tools like `cat`:
  * Checks if `*ppos` (Offset) has exceeded the data length. If so, it returns 0 (EOF) to terminate reading.
  * If `*ppos` is 0, it indicates the first read, and data generation proceeds.
* **Return Data:** Uses `copy_to_user` to copy buffer data to the user and manually updates `*ppos` to mark the number of bytes read, ensuring no duplicate reads or infinite loops occur.

#### D. Release Resources (`kfetch_release`)
When a user executes `close()`:
* Retrieves `file->private_data`.
* Uses `kfree` to release the memory allocated during `open`, ensuring system memory stability during long-term operation.

---

## 3. Key Technical Highlights

* **Application of Private Data:** Utilizing `file->private_data` implements per-process state management, resolving data pollution issues associated with global variables.
* **Safety Protection:**
  * Usage of `copy_from_user` / `copy_to_user` ensures kernel memory safety.
  * Setting `.owner = THIS_MODULE` in `cdev` prevents the module from being forcibly unloaded while in use, avoiding Kernel Panic.
* **Standardized Driver Behavior:** Correct handling of `loff_t *ppos` (Offset) allows this driver to support not only the proprietary client program but also be fully compatible with standard Linux tools (e.g., `cat`, `head`, `cp`).
