#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <unistd.h>

enum {
    CONSUMED = -1,
    END = -2,
    BUF_SIZE = 96
};

typedef struct {
    int *buf;
    pthread_mutex_t *lock;
    pthread_cond_t *item;
    pthread_cond_t *space;
} buf_set_t;

typedef struct {
    buf_set_t input;
    buf_set_t output;
} inout_set_t;

static void *comparator(void *inout_set);
static void *outputter(void *output);

int main(int argc, char *argv[])
{
    // Set limit of the number of threads
    struct rlimit rl = {0};
    getrlimit(RLIMIT_NPROC, &rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_NPROC, &rl);
    // Argument
    unsigned seed = 42;
    long length = 1e4;
    for (int c = getopt(argc, argv, "l:s:"); c != -1; c = getopt(argc, argv, "l:s:")) {
        switch(c) {
            case 'l':
                length = atol(optarg);
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                return -1;
            default:
                return -1;
        }
    }
    srand(seed);
    // Thread setting
    int buf[(length + 1) * BUF_SIZE];
    pthread_mutex_t lock[(length + 1) * BUF_SIZE];
    pthread_cond_t item[length + 1];
    pthread_cond_t space[length + 1];
    for (long i = 0; i < (length + 1) * BUF_SIZE; ++i) {
        buf[i] = CONSUMED;
        pthread_mutex_init(&lock[i], NULL);
    }
    for (long i = 0; i < length + 1; ++i) {
        pthread_cond_init(&item[i], NULL);
        pthread_cond_init(&space[i], NULL);
    }
    int (*buf_block)[length + 1][BUF_SIZE] = (int (*)[length + 1][BUF_SIZE])buf;
    pthread_mutex_t (*lock_block)[length + 1][BUF_SIZE] = (pthread_mutex_t (*)[length + 1][BUF_SIZE])lock;
    inout_set_t inout_set[length];
    for (long i = 0; i < length; ++i) {
        inout_set[i].input.buf = (*buf_block)[i];
        inout_set[i].input.lock = (*lock_block)[i];
        inout_set[i].input.item = &item[i];
        inout_set[i].input.space = &space[i];
        inout_set[i].output.buf = (*buf_block)[i + 1];
        inout_set[i].output.lock = (*lock_block)[i + 1];
        inout_set[i].output.item = &item[i + 1];
        inout_set[i].output.space = &space[i + 1];
    }
    buf_set_t buf_set = {
        .buf = (*buf_block)[length],
        .lock = (*lock_block)[length],
        .item = &item[length],
        .space = &space[length]
    };
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
    pthread_t comparator_thread[length];
    pthread_t successor_thread;
    // Create threads
    for (long i = 0; i < length; ++i) {
        if (pthread_create(&comparator_thread[i], &attr, comparator, &inout_set[i])) {
            perror("Creating comparator");
            // Destroy attr, mutex and cond
            pthread_attr_destroy(&attr);
            for (long i = 0; i < (length + 1) * BUF_SIZE; ++i)
                pthread_mutex_destroy(&lock[i]);
            for (long i = 0; i < length + 1; ++i) {
                pthread_cond_destroy(&item[i]);
                pthread_cond_destroy(&space[i]);
            }
            return 1;
        }
    }
    if (pthread_create(&successor_thread, &attr, outputter, &buf_set)) {
        perror("Creating outputter");
        // Destroy attr, mutex and cond
        pthread_attr_destroy(&attr);
        for (long i = 0; i < (length + 1) * BUF_SIZE; ++i)
            pthread_mutex_destroy(&lock[i]);
        for (long i = 0; i < length + 1; ++i) {
            pthread_cond_destroy(&item[i]);
            pthread_cond_destroy(&space[i]);
        }
        return 1;
    }
    int *input = &buf[0];
    pthread_mutex_t *input_lock = &lock[0];
    // Calculation start
    struct timespec before = {0};
    clock_gettime(CLOCK_MONOTONIC, &before);
    for (long i = 0; i < length; ++i) {
        pthread_mutex_lock(input_lock);
        if (*input != CONSUMED)
            pthread_cond_wait(&space[0], input_lock);
        *input = rand();
        pthread_cond_signal(&item[0]);
        pthread_mutex_unlock(input_lock);
        input = &buf[0] + (input - &buf[0] + 1) % BUF_SIZE;
        input_lock = &lock[0] + (input_lock - &lock[0] + 1) % BUF_SIZE;
    }
    for (int i = 0; i < 2; ++i) {
        pthread_mutex_lock(input_lock);
        if (*input != CONSUMED)
            pthread_cond_wait(&space[0], input_lock);
        *input = END;
        pthread_cond_signal(&item[0]);
        pthread_mutex_unlock(input_lock);
        input = &buf[0] + (input - &buf[0] + 1) % BUF_SIZE;
        input_lock = &lock[0] + (input_lock - &lock[0] + 1) % BUF_SIZE;
    }
    for (long i = 0; i < length; ++i)
        pthread_join(comparator_thread[i], NULL);
    pthread_join(successor_thread, NULL);
    struct timespec after = {0};
    clock_gettime(CLOCK_MONOTONIC, &after);
    // Destroy attr, mutex and cond
    pthread_attr_destroy(&attr);
    for (long i = 0; i < (length + 1) * BUF_SIZE; ++i)
        pthread_mutex_destroy(&lock[i]);
    for (long i = 0; i < length + 1; ++i) {
        pthread_cond_destroy(&item[i]);
        pthread_cond_destroy(&space[i]);
    }
    // Print result
    double time = (double)(after.tv_sec - before.tv_sec) +
                  (double)(after.tv_nsec - before.tv_nsec) / 1e9;
    printf("Pipesort took: % .6e seconds \n", time);
    return 0;
}

static void *comparator(void *inout_set)
{
    inout_set_t *io_set = inout_set;
    buf_set_t *in_set = &io_set->input;
    buf_set_t *out_set = &io_set->output;
    int *input = in_set->buf;
    int *output = out_set->buf;
    pthread_mutex_t *input_lock = in_set->lock;
    pthread_mutex_t *output_lock = out_set->lock;
    // First element
    pthread_mutex_lock(input_lock);
    if (*input == CONSUMED)
        pthread_cond_wait(in_set->item, input_lock);
    int buf = *input;
    *input = CONSUMED;
    pthread_cond_signal(in_set->space);
    pthread_mutex_unlock(input_lock);
    input = in_set->buf + (input - in_set->buf + 1) % BUF_SIZE;
    input_lock = in_set->lock + (input_lock - in_set->lock + 1) % BUF_SIZE;
    // Other elements
    pthread_mutex_lock(input_lock);
    if (*input == CONSUMED)
        pthread_cond_wait(in_set->item, input_lock);
    pthread_mutex_lock(output_lock);
    if (*output != CONSUMED)
        pthread_cond_wait(out_set->space, output_lock);
    while (*input != END) {
        if (*input <= buf) {
            *output = *input;
            pthread_cond_signal(out_set->item);
            pthread_mutex_unlock(output_lock);
            *input = CONSUMED;
            pthread_cond_signal(in_set->space);
            pthread_mutex_unlock(input_lock);
        } else {
            *output = buf;
            pthread_cond_signal(out_set->item);
            pthread_mutex_unlock(output_lock);
            buf = *input;
            *input = CONSUMED;
            pthread_cond_signal(in_set->space);
            pthread_mutex_unlock(input_lock);
        }
        output = out_set->buf + (output - out_set->buf + 1) % BUF_SIZE;
        output_lock = out_set->lock + (output_lock - out_set->lock + 1) % BUF_SIZE;
        input = in_set->buf + (input - in_set->buf + 1) % BUF_SIZE;
        input_lock = in_set->lock + (input_lock - in_set->lock + 1) % BUF_SIZE;
        pthread_mutex_lock(input_lock);
        if (*input == CONSUMED)
            pthread_cond_wait(in_set->item, input_lock);
        pthread_mutex_lock(output_lock);
        if (*output != CONSUMED)
            pthread_cond_wait(out_set->space, output_lock);
    }
    // First END
    *output = *input;
    pthread_cond_signal(out_set->item);
    pthread_mutex_unlock(output_lock);
    *input = CONSUMED;
    pthread_cond_signal(in_set->space);
    pthread_mutex_unlock(input_lock);
    output = out_set->buf + (output - out_set->buf + 1) % BUF_SIZE;
    output_lock = out_set->lock + (output_lock - out_set->lock + 1) % BUF_SIZE;
    input = in_set->buf + (input - in_set->buf + 1) % BUF_SIZE;
    input_lock = in_set->lock + (input_lock - in_set->lock + 1) % BUF_SIZE;
    // Buf elements
    while (buf != END) {
        pthread_mutex_lock(input_lock);
        if (*input == CONSUMED)
            pthread_cond_wait(in_set->item, input_lock);
        pthread_mutex_lock(output_lock);
        if (*output != CONSUMED)
            pthread_cond_wait(out_set->space, output_lock);
        *output = buf;
        pthread_cond_signal(out_set->item);
        pthread_mutex_unlock(output_lock);
        buf = *input;
        *input = CONSUMED;
        pthread_cond_signal(in_set->space);
        pthread_mutex_unlock(input_lock);
        output = out_set->buf + (output - out_set->buf + 1) % BUF_SIZE;
        output_lock = out_set->lock + (output_lock - out_set->lock + 1) % BUF_SIZE;
        input = in_set->buf + (input - in_set->buf + 1) % BUF_SIZE;
        input_lock = in_set->lock + (input_lock - in_set->lock + 1) % BUF_SIZE;
    }
    // Second END
    pthread_mutex_lock(output_lock);
    if (*output != CONSUMED)
        pthread_cond_wait(out_set->space, output_lock);
    *output = buf;
    pthread_cond_signal(out_set->item);
    pthread_mutex_unlock(output_lock);
    return NULL;
}

static void *outputter(void *buf_set)
{
    buf_set_t *b_set = buf_set;
    int *buf = b_set->buf;
    pthread_mutex_t *lock = b_set->lock;
    pthread_mutex_lock(lock);
    if (*buf == CONSUMED)
        pthread_cond_wait(b_set->item, lock);
    *buf = CONSUMED;
    pthread_cond_signal(b_set->space);
    pthread_mutex_unlock(lock);
    buf = b_set->buf + (buf - b_set->buf + 1) % BUF_SIZE;
    lock = b_set->lock + (lock - b_set->lock + 1) % BUF_SIZE;
    for (;;) {
        pthread_mutex_lock(lock);
        if (*buf == CONSUMED)
            pthread_cond_wait(b_set->item, lock);
        if (*buf == END) {
            pthread_mutex_unlock(lock);
            puts("");
            return NULL;
        }
        printf("%d ", *buf);
        *buf = CONSUMED;
        pthread_cond_signal(b_set->space);
        pthread_mutex_unlock(lock);
        buf = b_set->buf + (buf - b_set->buf + 1) % BUF_SIZE;
        lock = b_set->lock + (lock - b_set->lock + 1) % BUF_SIZE;
    }
}
