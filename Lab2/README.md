# Assignment II: Scheduling Policy Demonstration Program

---

## 1. Implementation

### 1.1 Configuration and Argument Parsing
We first define a configuration structure `Config` to store all user-specified settings: the number of threads (`num_threads`), the busy-wait duration per iteration (`time_wait`), and two arrays `policies[]` and `priorities[]`, which record the scheduling policy and real-time priority of each thread, respectively. The maximum number of threads is limited by a constant `MAX_THREADS`.

Command-line options are parsed using `getopt()`, supporting four options: `-n` for the number of worker threads, `-t` for the busy-wait time in seconds, `-s` for the per-thread scheduling policy, and `-p` for the per-thread priority. The `-s` option uses a string such as `NORMAL,FIFO,NORMAL,FIFO`, which we parse with `strtok_r()` and map to internal codes (`0` for `SCHED_NORMAL`/`SCHED_OTHER` and `1` for `SCHED_FIFO`). Similarly, the `-p` option uses a comma-separated list of integers, for example `-1,10,-1,30`, which are converted with `atoi()` and stored in `priorities[]`. We verify that the number of parsed policies and priorities matches `num_threads`, and that all entries have been initialized; otherwise, the program prints an error message and terminates.

### 1.2 CPU Affinity and Thread Creation
Before creating worker threads, we bind the main thread (and thus all children) to CPU 0 using `sched_setaffinity()` and a `cpu_set_t` that contains only core 0. This ensures that all threads contend on the same CPU core, which makes the behavior of `SCHED_FIFO` more observable and avoids interference from other cores.

We then initialize a `pthread_barrier_t` with the count equal to `num_threads`. This barrier serves as a common "starting line" for all worker threads. Each thread receives a `ThreadArg` structure containing its thread ID, the common `time_wait` value, its policy flag, its priority, and a pointer to the shared barrier. This structure is passed as the argument to the worker function `thread_func()`.

For each worker, we configure a `pthread_attr_t` object to explicitly set its scheduling policy and priority. We call `pthread_attr_init()` to initialize the attribute and then use `pthread_attr_setinheritsched()` with `PTHREAD_EXPLICIT_SCHED` so that the thread does not inherit the parent's scheduling settings. Depending on the parsed configuration, we choose `SCHED_OTHER` for "NORMAL" threads and `SCHED_FIFO` for "FIFO" threads, and fill a `struct sched_param` with the desired real-time priority (ignored for `SCHED_OTHER`, taken from the `-p` option for `SCHED_FIFO`). These values are applied to the attribute with `pthread_attr_setschedpolicy()` and `pthread_attr_setschedparam()`. The thread is then created via `pthread_create()` using this attribute.

On some environments, creating a `SCHED_FIFO` thread may fail with error code `EPERM` due to insufficient privileges. In that case, we print a warning message and fall back to calling `pthread_create()` again with `NULL` attributes, which uses the default scheduling policy. This fallback allows the same binary to run both on the host Docker environment and inside the RISC-V QEMU guest. In the QEMU environment, after disabling real-time throttling, the `SCHED_FIFO` configuration is applied successfully.

### 1.3 Worker Thread Behavior
The worker entry function `thread_func()` first calls `pthread_barrier_wait()` on the shared barrier. This ensures that all threads start their work at approximately the same time, which is important for demonstrating how higher-priority `SCHED_FIFO` threads preempt lower-priority or `SCHED_OTHER` threads.

After the barrier is released, each worker executes a fixed loop of three iterations. In every iteration, the thread prints a message of the form `"Thread %d is running"` with its own ID and calls `fflush(stdout)` to flush the output immediately, making the interleaving between threads visible in the terminal. It then calls our custom `busy_wait()` function with the configured `time_wait` value. After completing three iterations, the thread simply returns, and the main thread later collects all workers using `pthread_join()` and destroys the barrier.

### 1.4 Busy-Waiting Mechanism
The `busy_wait()` function implements CPU-bound busy waiting while excluding the time during which the thread is preempted, as required by the assignment. Instead of using `sleep()` or `nanosleep()`, which would put the thread into a sleeping state, we rely on the per-thread CPU-time clock. At the beginning of `busy_wait()`, we record the starting CPU time of the current thread using `clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start)`. Inside a loop, we repeatedly obtain the current thread CPU time, compute the elapsed CPU time in seconds, and exit the loop once the elapsed time reaches or exceeds the target `time_wait`.

Because `CLOCK_THREAD_CPUTIME_ID` only advances when the thread is actually executing on the CPU, any time interval during which the thread is preempted by another thread is not counted towards the busy-wait duration. As a result, when we measure the program using the `time` command, the total CPU time (user plus system) is approximately:

```text
time_wait * num_threads * 3
```

which matches the expected behavior: each of the `num_threads` threads performs three busy-wait iterations of length `time_wait`.

---

## 2. Scheduling Results and Analysis

### 2.1 Case 1

**Command Tested:**
```bash
./sched_demo -n 3 -t 1.0 -s NORMAL,FIFO,FIFO -p -1,10,30
```

**Results:**
```text
~ # ./sched_demo -n 3 -t 1.0 -s NORMAL,FIFO,FIFO -p -1,10,30
Thread 2 is running
Thread 2 is running
Thread 2 is running
Thread 1 is running
Thread 1 is running
Thread 1 is running
Thread 0 is running
Thread 0 is running
Thread 0 is running
```

#### Thread Configuration
* **Thread 0:** `SCHED_NORMAL`
* **Thread 1:** `SCHED_FIFO`, priority = 10
* **Thread 2:** `SCHED_FIFO`, priority = 30 (highest)

#### Observed Behavior & Explanation
* At the beginning, the output shows multiple lines of `Thread 2 is running`.
  * **Explanation:** The highest-priority FIFO thread (Thread 2) runs first and completes all three iterations.
* After Thread 2 finishes, the scheduler prints `Thread 1 is running`.
  * **Explanation:** The lower-priority FIFO thread (Thread 1) runs next and completes its three iterations.
* Finally, `Thread 0 is running` appears.
  * **Explanation:** The `SCHED_NORMAL` thread gets CPU only after all FIFO threads are done.

This case demonstrates two important scheduling rules:
1. `SCHED_FIFO` threads always have higher priority than `SCHED_NORMAL` tasks.
2. Among FIFO threads, the one with higher priority (Thread 2 with prio 30) runs until it finishes; then the next FIFO thread (Thread 1 with prio 10) runs.

### 2.2 Case 2

**Command Tested:**
```bash
./sched_demo -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30
```

**Results:**
```text
~ # ./sched_demo -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30
Thread 3 is running
Thread 3 is running
Thread 3 is running
Thread 1 is running
Thread 1 is running
Thread 1 is running
Thread 0 is running
Thread 2 is running
Thread 0 is running
Thread 2 is running
Thread 0 is running
Thread 2 is running
```

#### Thread Configuration
* **Thread 0:** `SCHED_NORMAL`
* **Thread 1:** `SCHED_FIFO`, priority = 10
* **Thread 2:** `SCHED_NORMAL`
* **Thread 3:** `SCHED_FIFO`, priority = 30 (highest)

#### Observed Behavior & Explanation
* The output starts with several lines of `Thread 3 is running`.
  * **Explanation:** The highest-priority FIFO thread completes all its iterations first.
* Next, `Thread 1 is running` appears three times.
  * **Explanation:** The second FIFO thread (priority 10) then runs until completion.
* Finally, `Thread 0` and `Thread 2` alternate in the output.
  * **Explanation:** After the FIFO threads finish, the two `SCHED_NORMAL` threads share the CPU using CFS (Completely Fair Scheduler).

This case shows:
1. FIFO threads (Thread 3 and Thread 1) run first, ordered by their priorities (30 > 10).
2. Normal threads (Thread 0 and Thread 2) only run when no FIFO threads are runnable.
3. Normal threads use Linux CFS scheduling, so they time-share the CPU and their outputs interleave.

### 2.3 `sched_demo_314551147`

```text
~ # ./sched_demo_314551147 -n 3 -t 1.0 -s NORMAL,FIFO,FIFO -p -1,10,30
Thread 2 is running
Thread 2 is running
Thread 2 is running
Thread 1 is running
Thread 1 is running
Thread 1 is running
Thread 0 is running
Thread 0 is running
Thread 0 is running
~ # ./sched_demo_314551147 -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30
Thread 3 is running
Thread 3 is running
Thread 3 is running
Thread 1 is running
Thread 1 is running
Thread 1 is running
Thread 2 is running
Thread 0 is running
Thread 2 is running
Thread 0 is running
Thread 0 is running
Thread 2 is running
```

---

## 3. Implementation of n-second Busy Waiting

To implement an n-second busy waiting for each thread, I wrote a function `busy_wait(double seconds)` that measures *per-thread CPU time* instead of wall-clock time. At the beginning of the function, I call `clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start)` to record the CPU time already consumed by the current thread. Then I enter a `while` loop and repeatedly call `clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now)`, compute the elapsed CPU time:

```c
elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
```

and exit the loop once `elapsed >= seconds`. Since `CLOCK_THREAD_CPUTIME_ID` only increases when the thread is actually running on the CPU, any time during which the thread is preempted by another thread is automatically excluded from the busy-wait duration. This satisfies the requirement that the n-second busy waiting should not count preempted time.

I experimented with two variants of this loop. In the simpler version, the loop only calls `clock_gettime` and checks the elapsed time. In this case, when measuring the whole program with the `time` command, I obtained:

```text
real    0m 6.01s
user    0m 0.01s
sys     0m 5.99s
```
Most of the CPU time is reported as *system* time, because the loop spends almost all of its time inside the kernel handling the `clock_gettime` system call.

In another version, I added a small dummy loop inside the busy-wait loop, e.g., `for (volatile int i = 0; i < 1000; i++) {}`, to do some work in user space. With this modification, the `time` command reports:

```text
real    0m 6.02s
user    0m 0.43s
sys     0m 5.58s
```
The total CPU time (`user + sys`) is still about six seconds, which matches the expected value `0.5 * 4 * 3`, but a larger fraction is now counted as *user* time because part of the busy waiting is spent executing user-space instructions. This shows that the implementation correctly performs CPU-bound busy waiting for n seconds of thread CPU time, while the split between user and system time depends on how much work is done in user space versus kernel space inside the loop.

---

## 4. Effect of `kernel.sched_rt_runtime_us`

Linux provides a real-time bandwidth control mechanism for `SCHED_FIFO` and `SCHED_RR` tasks. The parameter `/proc/sys/kernel/sched_rt_runtime_us` specifies, in microseconds, how much CPU time all real-time tasks on a CPU are allowed to consume during each real-time period. The length of this period is given by `/proc/sys/kernel/sched_rt_period_us`, which is typically `1000000` μs (1 second) by default.

Intuitively, `sched_rt_runtime_us` defines an upper bound on the CPU bandwidth that real-time tasks may use within each period. For example, if `sched_rt_period_us` is 1 second and `kernel.sched_rt_runtime_us` is set to `500000`, then real-time tasks on that CPU may use at most 0.5 seconds of CPU time in each 1-second period (i.e., at most 50% CPU bandwidth), and the remaining 50% of the CPU time is reserved for normal CFS tasks. Similarly, a value of `950000` allows real-time tasks to use up to 95% of the CPU time in each period, leaving only about 5% for non-real-time tasks. If the value is set to `1000000` and the period is also 1 second, then real-time tasks are allowed to use essentially all of the CPU time in each period, and CFS tasks no longer have a guaranteed minimum share.

A special value of `-1` disables the real-time bandwidth control entirely. In this case, `SCHED_FIFO` and `SCHED_RR` tasks are no longer limited by a runtime budget and may continuously occupy the CPU as long as they remain runnable. In our experiments, we write `echo -1 > /proc/sys/kernel/sched_rt_runtime_us` inside the QEMU guest to disable this limitation so that the behavior of `SCHED_FIFO` threads can be observed clearly: once a high-priority real-time thread becomes runnable, it can keep running until it blocks or finishes, while lower-priority and `SCHED_OTHER` threads may be delayed.
