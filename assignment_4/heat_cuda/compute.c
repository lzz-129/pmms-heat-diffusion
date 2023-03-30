#define _GNU_SOURCE
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "cuda_compute.h"
#include "compute.h"

/* ... */
#define DAS5CPU

#ifdef DAS5CPU
#define CPUNUM 32
#endif

static int fill_report_thread(struct results *r, double iter, double maxdiff);
static int clear_report_thread(struct results *r);
static void *thread_main(void *args);

struct thread_parameters
{
    const struct parameters *p;
    struct results *r;
    size_t h;
    size_t w;
    size_t h_per_thread;
    double *src_dev;
    double *dst_dev;
    double *c_dev;
    int tid;
    size_t num_threads;
};

pthread_mutex_t *arrival_lock;
pthread_mutex_t *convergence_lock;
pthread_mutex_t report_lock;
pthread_barrier_t barrier;

size_t *arrival_g = NULL;
int *convergence_g = NULL;
struct timespec before;

void do_compute(const struct parameters *p, struct results *r)
{
    size_t i, j;

    /* alias input parameters */
    const double (*restrict tinit)[p->N][p->M] = (const double (*)[p->N][p->M])p->tinit;
    const double (*restrict cinit)[p->N][p->M] = (const double (*)[p->N][p->M])p->conductivity;

    /* allocate grid data */
    const size_t h = p->N + 2;
    const size_t w = p->M + 2;
    double g1[h][w];
    double g2[h][w];

    /* allocate halo for conductivities */
    double c[h][w];

    for (i = 1; i < h - 1; ++i) {
        for (j = 1; j < w - 1; ++j) {
            g1[i][j] = (*tinit)[i - 1][j - 1];
            c[i][j] = (*cinit)[i - 1][j - 1];
        }
    }

    for (j = 1; j < w - 1; ++j) {
        g1[0][j] = g2[0][j] = g1[1][j];
        g1[h - 1][j] = g2[h - 1][j] = g1[h - 2][j];
    }

    // printf("g: %lf, %lf\n", g1[1][w - 2], g1[200][w - 2]);
    // printf("g: %lf, %lf, %p, %p\n", g1[2][1], g1[502][1]);

    /* init counter */
    arrival_g = (size_t *)malloc((p->nthreads + 2) * sizeof(size_t));
    memset(arrival_g, 0, (p->nthreads + 2) * sizeof(size_t));
    arrival_g[0] = arrival_g[p->nthreads + 1] = p->maxiter + 1;

    /* init convergence */
    convergence_g = (int *)malloc((p->nthreads) * sizeof(int));
    for(i = 0; i < p->nthreads; i++){
        convergence_g[i] = 0;
    }

    /* init lock */
    arrival_lock = (pthread_mutex_t *)malloc((p->nthreads + 2) * sizeof(pthread_mutex_t));
    for(i = 0; i < p->nthreads + 2; i++){
        if(pthread_mutex_init(&arrival_lock[i], NULL)){
            perror("spinlock init");
        }
    }
    convergence_lock = (pthread_mutex_t *)malloc(p->nthreads * sizeof(pthread_mutex_t));
    for(i = 0; i < p->nthreads; i++){
        if(pthread_mutex_init(&convergence_lock[i], NULL)){
            perror("spinlock init");
        }
    }
    if(pthread_mutex_init(&report_lock, NULL)){
        perror("spinlock init");
    }

    /* barrier init */
    pthread_barrier_init(&barrier, NULL, p->nthreads);

    /* report init */
    clear_report_thread(r);


    /* init gpu */
    double *src_dev = NULL;   //device memory
    double *dst_dev = NULL;   //device memory
    double *c_dev = NULL;     //device memory
    cuda_do_init(h, w, (double *)&g1, (double *)&g2, (double *)&c, &src_dev, &dst_dev, &c_dev);

    /* cpu info */
    #ifdef DAS5CPU
    cpu_set_t cpuset[CPUNUM];
    for(i = 0; i < CPUNUM; i++){
        CPU_ZERO(&cpuset[i]);
        CPU_SET(i, &cpuset[i]);;
    }
    /* set main */
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset[0]);
    #endif

    //the src, dst need be swap
    // cuda_do_report(h, w, src_dev, src_dev);

    /* create pthread */
    pthread_t tid[p->nthreads];
    struct thread_parameters tp[p->nthreads];
    for(i = 0; i < p->nthreads; i++){
        tp[i] = (struct thread_parameters){
            .p = p, .r = r,
            .h = h, .w = w,
            .h_per_thread = (p->N)/p->nthreads + (int)(((p->N)%p->nthreads) > i) + 2,
            .num_threads = p->nthreads, .tid = i + 1
        };
        if(i == 0){
            // tp[i].h_start = 1;
            tp[i].src_dev = src_dev;
            tp[i].dst_dev = dst_dev;
            tp[i].c_dev = c_dev;
        }else{
            // tp[i].h_start = tp[i - 1].h_end;
            tp[i].src_dev = tp[i - 1].src_dev + w * (tp[i - 1].h_per_thread - 2);
            tp[i].dst_dev = tp[i - 1].dst_dev + w * (tp[i - 1].h_per_thread - 2);
            tp[i].c_dev = tp[i - 1].c_dev + w * (tp[i - 1].h_per_thread - 2);
        }
        // tp[i].h_end = tp[i].h_start + p->N/p->nthreads + (int)((p->N%p->nthreads) > i);
        pthread_create(&tid[i], NULL, &thread_main, &tp[i]);
        #ifdef DAS5CPU
        pthread_setaffinity_np(tid[i], sizeof(cpu_set_t), &cpuset[i]);
        #endif
    }

    for(i = 0; i < p->nthreads; i++){
        pthread_join(tid[i], NULL);
    }

    //the src, dst need be swap
    // cuda_do_report(h, w, src_dev, dst_dev, r);

    /* deinit gpu */
    cuda_do_deinit(src_dev, dst_dev, c_dev);

    /* free & destroy lock */
    for(i = 0; i < p->nthreads + 2; i++){
        pthread_mutex_destroy(&arrival_lock[i]);
    }
    for(i = 0; i < p->nthreads; i++){
        pthread_mutex_destroy(&convergence_lock[i]);
    }
    pthread_mutex_destroy(&report_lock);

    /* free */
    free(arrival_g);
    free(convergence_g);
    free(arrival_lock);
    free(convergence_lock);
}
static void *thread_main(void *args)
{

    const struct thread_parameters *tp = (const struct thread_parameters *)args;

    static const double c_cdir = 0.25 * M_SQRT2 / (M_SQRT2 + 1.0);
    static const double c_cdiag = 0.25 / (M_SQRT2 + 1.0);
    const double threshold = (const double)(tp->p->threshold);
    const size_t num_threads = tp->p->nthreads, maxiter = tp->p->maxiter;
    const size_t h = tp->h, w = tp->w, h_per_thread = tp->h_per_thread;
    const int tid = tp->tid;
    /* swap before first iteration*/
    double *src = tp->dst_dev;
    double *dst = tp->src_dev;
    double *c = tp->c_dev;
    double *maxdiff_dev = NULL;

    int *convergence = convergence_g;
    size_t *arrival = arrival_g;
    size_t period = tp->p->period;
    size_t i, j, iter;


    /* start timer */
    pthread_barrier_wait(&barrier);
    if(tid == 1){
        pthread_mutex_lock(&report_lock);
        clock_gettime(CLOCK_MONOTONIC, &before);
        pthread_mutex_unlock(&report_lock);
    }

    for (iter = 1; iter <= maxiter; ++iter){
        const int cnvg_idx = iter % num_threads;

        {void *tmp = src; src = dst; dst = tmp;}

        // move compute to GPU
        double maxdiff = cuda_do_compute(h_per_thread, w, src, dst, c);

        /* if this block converge, stuck here */
        if (maxdiff < threshold){
            pthread_mutex_lock(&convergence_lock[cnvg_idx]);
            ++convergence[cnvg_idx];
            pthread_mutex_unlock(&convergence_lock[cnvg_idx]);
            do{
                /* check whether all is convergence */
                pthread_mutex_lock(&convergence_lock[cnvg_idx]);
                /* mean it is converge */
                if(convergence[cnvg_idx] == num_threads){
                    pthread_mutex_unlock(&convergence_lock[cnvg_idx]);
                    fill_report_thread(tp->r, iter, maxdiff);
                    pthread_barrier_wait(&barrier);
                    if(tid == 1){
                        cuda_do_report(h, w, src, dst, tp->r);
                    }
                    return NULL;
                }
                pthread_mutex_unlock(&convergence_lock[cnvg_idx]);

                /* check if anyone is not converge */
                for(i = 1; i <= num_threads; i++){
                    pthread_mutex_lock(&arrival_lock[i]);
                    int flag = arrival[i] >= iter;
                    pthread_mutex_unlock(&arrival_lock[i]);
                    /*  leave when someone not convergence */
                    if(flag){
                        pthread_mutex_lock(&convergence_lock[cnvg_idx]);
                        convergence[cnvg_idx] = 0;
                        pthread_mutex_unlock(&convergence_lock[cnvg_idx]);
                        break;
                    }
                }
            }while(i > num_threads);
        }

        /* set arrival flag */
        pthread_mutex_lock(&arrival_lock[tid]);
        ++arrival[tid];
        pthread_mutex_unlock(&arrival_lock[tid]);

        /* stuck in this while until up and down block also down */
        while(1){
            size_t l1, l2;
            /* first lock */
            pthread_mutex_lock(&arrival_lock[tid - 1]);
            l1 = arrival[tid - 1];
            pthread_mutex_unlock(&arrival_lock[tid - 1]);
            if(l1 < iter) continue;
            /* second lock */
            pthread_mutex_lock(&arrival_lock[tid + 1]);
            l2 = arrival[tid + 1];
            pthread_mutex_unlock(&arrival_lock[tid + 1]);
            if(l2 >= iter) break;
            // if((l1 >= iter) && (l2 >= iter)) break;
        }

        // TODO: maybe can merge with the above while(1)
        /* conditional reporting */
        if (iter % period == 0 || iter == maxiter) {
            fill_report_thread(tp->r, iter, maxdiff);
            pthread_barrier_wait(&barrier);
            /* print by first thread */
            if(tid == 1){
                cuda_do_report(h, w, src, dst, tp->r);
                pthread_mutex_lock(&report_lock);
                if(tp->p->printreports || iter == maxiter) report_results(tp->p, tp->r);
                clear_report_thread(tp->r);
                pthread_mutex_unlock(&report_lock);
            }
        }
    }

}

static int fill_report_thread(struct results *r, double iter, double maxdiff)
{
    /* compute min/max/avg */
    double tmin = INFINITY, tmax = -INFINITY;
    double avg = 0.0;
    struct timespec after;

    /* We have said that the final reduction does not need to be included. */
    clock_gettime(CLOCK_MONOTONIC, &after);
 
    /* report lock */
    pthread_mutex_lock(&report_lock);
    r->niter = iter;
    if(r->maxdiff < maxdiff) r->maxdiff = maxdiff;
    r->time = (double)(after.tv_sec - before.tv_sec) + (double)(after.tv_nsec - before.tv_nsec) / 1e9;
    pthread_mutex_unlock(&report_lock);
    /* report unlock */

    return 0;
}

static int clear_report_thread(struct results *r)
{
    r->niter = 0;
    r->tmin = INFINITY;
    r->tmax = -INFINITY;
    r->tavg = 0;
    r->maxdiff = 0;

    return 0;
}