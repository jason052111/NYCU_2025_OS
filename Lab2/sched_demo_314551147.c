// sched_demo_314551147.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      
#include <pthread.h>    
#include <time.h>   
#include <sched.h>  
#include <errno.h>  

#define MAX_THREADS 64

typedef struct {
    int   num_threads;
    double time_wait;
    int   policies[MAX_THREADS];   // 0 = NORMAL, 1 = FIFO
    int   priorities[MAX_THREADS]; // -1 for NORMAL
} Config;

typedef struct {
    int id;
    double time_wait;
    int policy;
    int priority;
    pthread_barrier_t *barrier;
} ThreadArg;

void parse_policies(const char *s, Config *cfg);
void parse_priorities(const char *s, Config *cfg);
void *thread_func(void *arg);
void busy_wait(double seconds);

int main(int argc, char *argv[])
{
    Config cfg;
    cfg.num_threads = -1;
    cfg.time_wait   = -1.0;

    for (int i = 0; i < MAX_THREADS; i++) {
        cfg.policies[i]   = -1;
        cfg.priorities[i] = -9999;
    }

    int opt;
    char *policy_str = NULL;
    char *prio_str   = NULL;

    while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
        switch (opt) {
        case 'n':
            cfg.num_threads = atoi(optarg);
            break;
        case 't':
            cfg.time_wait = atof(optarg);
            break;
        case 's':
            policy_str = optarg;
            break;
        case 'p':
            prio_str = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s -n N -t T -s POLICIES -p PRIORITIES\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (cfg.num_threads <= 0 || cfg.num_threads > MAX_THREADS ||
        cfg.time_wait <= 0.0 || policy_str == NULL || prio_str == NULL) {
        fprintf(stderr, "Invalid arguments.\n");
        fprintf(stderr, "Usage: %s -n N -t T -s POLICIES -p PRIORITIES\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    parse_policies(policy_str, &cfg);
    parse_priorities(prio_str, &cfg);

    for (int i = 0; i < cfg.num_threads; i++) {
        if (cfg.policies[i] == -1 || cfg.priorities[i] == -9999) {
            fprintf(stderr, "Mismatch between -n, -s, -p (index %d not set)\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // force use same CPU
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        perror("sched_setaffinity (main)");
    }

    // use barrier to wait all thread 
    pthread_t threads[MAX_THREADS];
    ThreadArg args[MAX_THREADS];
    pthread_barrier_t barrier;

    // how many threads need to init will let it run 
    if (pthread_barrier_init(&barrier, NULL, cfg.num_threads) != 0) {
        perror("pthread_barrier_init");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < cfg.num_threads; i++) {
        args[i].id        = i;
        args[i].time_wait = cfg.time_wait;
        args[i].policy    = cfg.policies[i];
        args[i].priority  = cfg.priorities[i];
        args[i].barrier   = &barrier;

        pthread_attr_t attr;
        pthread_attr_init(&attr);

	// don't inherit
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

        int policy;
        struct sched_param sp;
        memset(&sp, 0, sizeof(sp));

        if (cfg.policies[i] == 0) {
            policy = SCHED_OTHER;
            sp.sched_priority = 0;
        } else {
            policy = SCHED_FIFO;
            sp.sched_priority = cfg.priorities[i];
        }

        if (pthread_attr_setschedpolicy(&attr, policy) != 0) {
            perror("pthread_attr_setschedpolicy");
        }

        if (pthread_attr_setschedparam(&attr, &sp) != 0) {
            perror("pthread_attr_setschedparam");
        }

        int ret = pthread_create(&threads[i], &attr, thread_func, &args[i]);
        pthread_attr_destroy(&attr);

        if (ret == EPERM && cfg.policies[i] == 1) {
            fprintf(stderr,
                    "Warning: no permission to create SCHED_FIFO thread %d, "
                    "falling back to default policy.\n", i);
            ret = pthread_create(&threads[i], NULL, thread_func, &args[i]);
        }

        if (ret != 0) {
            fprintf(stderr, "pthread_create failed for thread %d (err=%d)\n", i, ret);
            exit(EXIT_FAILURE);
        }

    }

    for (int i = 0; i < cfg.num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);

    return 0;
}

void busy_wait(double seconds)
{
    struct timespec start, now;

    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start) != 0) {
        perror("clock_gettime");
        return;
    }

    while (1) {
        if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) != 0) {
            perror("clock_gettime");
            return;
        }

        double elapsed =
            (now.tv_sec  - start.tv_sec) +
            (now.tv_nsec - start.tv_nsec) / 1e9;

        if (elapsed >= seconds)
            break;

        // for (volatile int i = 0; i < 1000; i++) {
        // }
    }
}

void *thread_func(void *arg)
{
    ThreadArg *targ = (ThreadArg*)arg;

    pthread_barrier_wait(targ->barrier);

    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", targ->id);
        fflush(stdout); 

        busy_wait(targ->time_wait);
    }

    return NULL;
}

void parse_policies(const char *s, Config *cfg)
{
    char *tmp = strdup(s);
    if (!tmp) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    int idx = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(tmp, ",", &saveptr);
    while (tok != NULL && idx < cfg->num_threads) {
        if (strcmp(tok, "NORMAL") == 0) {
            cfg->policies[idx] = 0;
        } else if (strcmp(tok, "FIFO") == 0) {
            cfg->policies[idx] = 1;
        } else {
            fprintf(stderr, "Unknown policy '%s' at index %d\n", tok, idx);
            free(tmp);
            exit(EXIT_FAILURE);
        }
        idx++;
        tok = strtok_r(NULL, ",", &saveptr);
    }

    if (idx != cfg->num_threads) {
        fprintf(stderr, "Number of policies (%d) != num_threads (%d)\n",
                idx, cfg->num_threads);
        free(tmp);
        exit(EXIT_FAILURE);
    }

    free(tmp);
}

void parse_priorities(const char *s, Config *cfg)
{
    char *tmp = strdup(s);
    if (!tmp) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    int idx = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(tmp, ",", &saveptr);
    while (tok != NULL && idx < cfg->num_threads) {
        int prio = atoi(tok);
        cfg->priorities[idx] = prio;
        idx++;
        tok = strtok_r(NULL, ",", &saveptr);
    }

    if (idx != cfg->num_threads) {
        fprintf(stderr, "Number of priorities (%d) != num_threads (%d)\n",
                idx, cfg->num_threads);
        free(tmp);
        exit(EXIT_FAILURE);
    }

    free(tmp);
}

