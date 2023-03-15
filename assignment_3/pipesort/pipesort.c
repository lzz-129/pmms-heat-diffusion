#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <pthread.h>
#include <limits.h>

#include <sys/time.h>
#include <sys/resource.h>

#define BUFSIZE 10
#define EMPTYSYMBOL -1
#define ENDSYMBOL -2
#define THREADSTACK (BUFSIZE * sizeof(int) + 2048)

struct thread_parameters
{
    int number;
    int *buf;
    // pthread_spinlock_t *buf_lock;
    pthread_mutex_t *buf_lock;
    pthread_cond_t *item;
    pthread_cond_t *space;
    int *occupied;
};

static void *thread_main(void *args);
static void *output_thread_main(void *args);

int main(int argc, char *argv[]){
    int c;
    int seed = 42;
    long length = 1e4;

    struct timespec before;
    struct timespec after;

    /* Read command-line options. */
    while((c = getopt(argc, argv, "l:s:")) != -1) {
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

    /* Seed such that we can always reproduce the same random vector */
    srand(seed);

    /* set the maximum number of processes (set to hard limit) */
    struct rlimit rl;
    getrlimit(RLIMIT_NPROC, &rl);
    rl.rlim_cur = rl.rlim_max;
    setrlimit(RLIMIT_NPROC, &rl);


    /* for successor */
    pthread_t tid;
    int seccessor_created = 0;
    int buf_successor[BUFSIZE];
    pthread_mutex_t buf_lock_successor = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t item_successor = PTHREAD_COND_INITIALIZER;
    pthread_cond_t space_successor = PTHREAD_COND_INITIALIZER;
    int occupied_successor = 0;
    /* successor thread */
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);
    pthread_attr_setstacksize(&attrs, (PTHREAD_STACK_MIN > THREADSTACK)?PTHREAD_STACK_MIN:THREADSTACK);    

    /* init sucessor parameter */
    struct thread_parameters tp_successor = {
        .buf = buf_successor, .buf_lock = &buf_lock_successor, .item = &item_successor, .space = &space_successor, .occupied = &occupied_successor
    };

    /* timer start */
    clock_gettime(CLOCK_MONOTONIC, &before);
    for(int i = 0; i < length; ++i){
        int buf_number = rand();
        if(!seccessor_created){
            tp_successor.number = buf_number;
            for(int i = 0; i < BUFSIZE; ++i) buf_successor[i] = EMPTYSYMBOL;
            if(pthread_create(&tid, &attrs, &thread_main, &tp_successor)){
                perror("create successor thread");
                fflush(stdout);
            }
            seccessor_created = 1;
        }else{
            pthread_mutex_lock(&buf_lock_successor);
            if(occupied_successor == BUFSIZE){
                pthread_cond_wait(&space_successor, &buf_lock_successor);
            }
            for(int i = 0; i < BUFSIZE; ++i){
                if(buf_successor[i] == EMPTYSYMBOL){
                    buf_successor[i] = buf_number;
                    // printf("put number = %d, occupied = %d\n", buf_number, occupied_successor);
                    fflush(stdout);
                    buf_number = EMPTYSYMBOL;
                    ++occupied_successor;
                    break;
                }
            }
            pthread_cond_signal(&item_successor);
            pthread_mutex_unlock(&buf_lock_successor);
        }
        
    }

    /* insert end symbol when buffer is clear */
    if(length > 0){
        /* insert end symbol two time */
        for(int round = 0; round < 2; ++round){
            pthread_mutex_lock(&buf_lock_successor);
            while(occupied_successor > 0){
                pthread_cond_wait(&space_successor, &buf_lock_successor);
            }
            buf_successor[0] = ENDSYMBOL;
            ++occupied_successor;
            pthread_cond_signal(&item_successor);
            pthread_mutex_unlock(&buf_lock_successor);

            // int buf_empty = 0;
            // do{
            //     buf_empty = 1;
            //     for(int i = 0; i < BUFSIZE; ++i){
            //         pthread_spin_lock(&buf_lock_successor);
            //         if(buf_successor[i] != EMPTYSYMBOL){
            //             buf_empty = 0;
            //             pthread_spin_unlock(&buf_lock_successor);
            //             break;
            //         }
            //         pthread_spin_unlock(&buf_lock_successor);
            //     }
            // }while(!buf_empty);
            // /* put end symbol when buf is empty */
            // pthread_spin_lock(&buf_lock_successor);
            // buf_successor[0] = ENDSYMBOL;
            // pthread_spin_unlock(&buf_lock_successor);
        }
    }

    /* free */
    if(seccessor_created){
        pthread_join(tid, NULL);
        pthread_mutex_destroy(&buf_lock_successor);
    }

    /* timer end */
    clock_gettime(CLOCK_MONOTONIC, &after);
    double time = (double)(after.tv_sec - before.tv_sec) +
                  (double)(after.tv_nsec - before.tv_nsec) / 1e9;

    printf("Pipesort took: % .6e seconds \n", time);


    return 0;
}
static void *thread_main(void *args)
{
    struct thread_parameters *tp = (struct thread_parameters *)args;
    int number = tp->number;
    int *buf = tp->buf;
    pthread_mutex_t *buf_lock = tp->buf_lock;
    pthread_cond_t *item = tp->item;
    pthread_cond_t *space = tp->space;
    int *occupied = tp->occupied;

    /* for successor */
    pthread_t tid;
    int seccessor_created = 0;
    int buf_successor[BUFSIZE];
    pthread_mutex_t buf_lock_successor = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t item_successor = PTHREAD_COND_INITIALIZER;
    pthread_cond_t space_successor = PTHREAD_COND_INITIALIZER;
    int occupied_successor = 0;
    /* successor thread */
    pthread_attr_t attrs;
    pthread_attr_init(&attrs);
    pthread_attr_setstacksize(&attrs, (PTHREAD_STACK_MIN > THREADSTACK)?PTHREAD_STACK_MIN:THREADSTACK);

    /* init sucessor parameter */
    struct thread_parameters tp_successor = {
        .buf = buf_successor, .buf_lock = &buf_lock_successor, .item = &item_successor, .space = &space_successor, .occupied = &occupied_successor
    };

    while(1){
        /* get the number from buffer */
        int buf_number = EMPTYSYMBOL;
        pthread_mutex_lock(buf_lock);
        // while(occupied == 0){
        if(*occupied == 0){
            // printf("wait here\n");
            // fflush(stdout);
            pthread_cond_wait(item, buf_lock);
        }
        for(int i = 0; i < BUFSIZE; ++i){
            if(buf[i] != EMPTYSYMBOL){
                buf_number = buf[i];
                buf[i] = EMPTYSYMBOL;
                fflush(stdout);
                --(*occupied);
                // printf("get number = %d, occupied = %d\n", buf_number, *occupied);
                break;
            }
        }
        // printf("number = %d\n", number);
        // fflush(stdout);
        pthread_cond_signal(space);
        pthread_mutex_unlock(buf_lock);
        // do{
        //     for(int i = 0; i < BUFSIZE; ++i){
        //         pthread_spin_lock(buf_lock);
        //         buf_number = buf[i];
        //         buf[i] = EMPTYSYMBOL;    /* also include clean end symbol */
        //         pthread_spin_unlock(buf_lock);
        //         if(buf_number != EMPTYSYMBOL) break;
        //     }
        // }while(buf_number == EMPTYSYMBOL);

        /* break when found end symbol */
        if(buf_number == ENDSYMBOL){
            break;
        }

        /* large one stay */
        if(number < buf_number){
            int tmp = number;
            number = buf_number;
            buf_number = tmp;
        }
        if(!seccessor_created){
            tp_successor.number = buf_number;
            for(int i = 0; i < BUFSIZE; ++i) buf_successor[i] = EMPTYSYMBOL;
            if(pthread_create(&tid, &attrs, &thread_main, &tp_successor)){
                perror("create successor thread");
                fflush(stdout);
            }
            seccessor_created = 1;
        }else{
            /* find the space to put number */
            pthread_mutex_lock(&buf_lock_successor);
            // while(occupied == 0){
            if(occupied_successor == BUFSIZE){
                pthread_cond_wait(&space_successor, &buf_lock_successor);
            }
            for(int i = 0; i < BUFSIZE; ++i){
                if(buf_successor[i] == EMPTYSYMBOL){
                    buf_successor[i] = buf_number;
                    buf_number = EMPTYSYMBOL;
                    ++occupied_successor;
                    break;
                }
            }
            pthread_cond_signal(&item_successor);
            pthread_mutex_unlock(&buf_lock_successor);
            // do{
            //     for(int i = 0; i < BUFSIZE; ++i){
            //         pthread_spin_lock(&buf_lock_successor);
            //         if(buf_successor[i] == EMPTYSYMBOL){
            //             buf_successor[i] = buf_number;
            //             pthread_spin_unlock(&buf_lock_successor);
            //             buf_number = EMPTYSYMBOL;
            //             break;
            //         }
            //         pthread_spin_unlock(&buf_lock_successor);
            //     }
            // }while(buf_number != EMPTYSYMBOL);
        }   
    }

    /* create the output thread */
    if(!seccessor_created){
        tp_successor.number = number;
        number = EMPTYSYMBOL;
        for(int i = 0; i < BUFSIZE; ++i) buf_successor[i] = EMPTYSYMBOL;
        if(pthread_create(&tid, &attrs, &output_thread_main, &tp_successor)){
            perror("create output thread");
            fflush(stdout);
        }
        seccessor_created = 1;
    }
    else{
        /* put the end symbol to successor when all buffer clean */
        pthread_mutex_lock(&buf_lock_successor);
        while(occupied_successor > 0){
            pthread_cond_wait(&space_successor, &buf_lock_successor);
        }
        buf_successor[0] = ENDSYMBOL;
        ++occupied_successor;
        pthread_cond_signal(&item_successor);
        pthread_mutex_unlock(&buf_lock_successor);

        /* put the end symbol to successor when all buffer clean */
        // int buf_empty = 0;
        // do{
        //     buf_empty = 1;
        //     for(int i = 0; i < BUFSIZE; ++i){
        //         pthread_spin_lock(&buf_lock_successor);
        //         if(buf_successor[i] != EMPTYSYMBOL){
        //             buf_empty = 0;
        //             pthread_spin_unlock(&buf_lock_successor);
        //             break;
        //         }
        //         pthread_spin_unlock(&buf_lock_successor);
        //     }
        // }while(!buf_empty);
        // pthread_spin_lock(&buf_lock_successor);
        // buf_successor[0] = ENDSYMBOL;
        // pthread_spin_unlock(&buf_lock_successor);
    }
    while(number != ENDSYMBOL){
        /* get the number from buffer */
        if(number == EMPTYSYMBOL){
            pthread_mutex_lock(buf_lock);
            // while(occupied == 0){
            if(*occupied == 0){
                pthread_cond_wait(item, buf_lock);
            }
            for(int i = 0; i < BUFSIZE; ++i){
                if(buf[i] != EMPTYSYMBOL){
                    number = buf[i];
                    buf[i] = EMPTYSYMBOL;
                    --(*occupied);
                    break;
                }
            }
            pthread_cond_signal(space);
            pthread_mutex_unlock(buf_lock);
        }
        // while(number == EMPTYSYMBOL){
        //     for(int i = 0; i < BUFSIZE; ++i){
        //         pthread_spin_lock(buf_lock);
        //         number = buf[i];
        //         buf[i] = EMPTYSYMBOL;    /* also include clean end symbol */
        //         pthread_spin_unlock(buf_lock);
        //         if(number != EMPTYSYMBOL) break;
        //     }
        // }

        /* put the number into buffer */
        pthread_mutex_lock(&buf_lock_successor);
        while(buf_successor[0] != EMPTYSYMBOL){
            pthread_cond_wait(&space_successor, &buf_lock_successor);
        }
        buf_successor[0] = number;
        if(number != ENDSYMBOL){
            number = EMPTYSYMBOL;
        }
        ++occupied_successor;
        pthread_cond_signal(&item_successor);
        pthread_mutex_unlock(&buf_lock_successor);

        // do{
        //     pthread_spin_lock(&buf_lock_successor);
        //     if(buf_successor[0] == EMPTYSYMBOL){
        //         buf_successor[0] = number;
        //         /* the end symbol should also be put in buffer */
        //         if(number == ENDSYMBOL){
        //             pthread_spin_unlock(&buf_lock_successor);
        //             break;
        //         }
        //         number = EMPTYSYMBOL;
        //     }
        //     pthread_spin_unlock(&buf_lock_successor);
        // }while(number != EMPTYSYMBOL);
    }
    // printf("thread end\n");
    // fflush(stdout);
    
    if(seccessor_created){
        pthread_join(tid, NULL);
        pthread_mutex_destroy(&buf_lock_successor);
    }
    
}
static void *output_thread_main(void *args)
{
    struct thread_parameters *tp = (struct thread_parameters *)args;
    int number = tp->number;
    int *buf = tp->buf;
    pthread_mutex_t *buf_lock = tp->buf_lock;
    pthread_cond_t *item = tp->item;
    pthread_cond_t *space = tp->space;
    int *occupied = tp->occupied;


    // printf("%d ", number);
    while(1){
        int buf_number = EMPTYSYMBOL;
        pthread_mutex_lock(buf_lock);
        // while(occupied == 0){
        if(buf[0] == EMPTYSYMBOL){
            pthread_cond_wait(item, buf_lock);
        }
        buf_number = buf[0];
        buf[0] = EMPTYSYMBOL;
        --(*occupied);
        pthread_cond_signal(space);
        pthread_mutex_unlock(buf_lock);

        /* break when found end symbol */
        if(buf_number == ENDSYMBOL){
            break;
        }
        printf("%d ", buf_number);
    }
    // printf("print end\n");
    // fflush(stdout);
}
