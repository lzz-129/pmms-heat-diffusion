#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <error.h>
#include <stdio.h>

#include <string.h>
// #define GEN_PICTURES

#include "compute.h"

#ifdef GEN_PICTURES
static void do_draw(const struct parameters *p,
             size_t key, size_t h, size_t w,
             double (*restrict g)[h][w])
{
    begin_picture(key, w-2, h-2, p->io_tmin, p->io_tmax);
    size_t i, j;
    for (i = 1; i < h-1; ++i)
        for (j = 1; j < w-1; ++j)
            draw_point(j-1, i-1, (*g)[i][j]);
    end_picture();
}
#endif

static int fill_report_thread(const struct parameters *p, struct results *r,
                        size_t h_start, size_t h_end, 
                        size_t h, size_t w, 
                        double (*restrict a)[h][w],
                        double iter, double maxdiff);
static int clear_report_thread(struct results *r);
static void *thread_main(void *args);

struct thread_parameters
{
    const struct parameters *p;
    struct results *r;
    size_t h;
    size_t w;
    size_t h_start;
    size_t h_end;
    int tid;
    size_t num_threads;
};

pthread_mutex_t *arrival_lock;
pthread_mutex_t *convergence_lock;
pthread_mutex_t report_lock;
pthread_barrier_t barrier;

size_t *arrival_g = NULL;
int *convergence_g = NULL;
double *src_g = NULL;
double *dst_g = NULL;
double *c_g = NULL;
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

    /* set matric */
    src_g = (double *)&g2;
    dst_g = (double *)&g1;
    c_g = (double *)&c;

    /* create pthread */
    pthread_t tid[p->nthreads];
    struct thread_parameters tp[p->nthreads];
    for(i = 0; i < p->nthreads; i++){
        tp[i] = (struct thread_parameters){
            .p = p, .r = r,
            .h = h, .w = w,
            .num_threads = p->nthreads, .tid = i + 1
        };
        if(i == 0){
            tp[i].h_start = 1;
        }else{
            tp[i].h_start = tp[i - 1].h_end;
        }
        tp[i].h_end = tp[i].h_start + p->N/p->nthreads + (int)((p->N%p->nthreads) > i);
        // printf("start = %d\tend = %d\n", tp[i].h_start,tp[i].h_end);
        pthread_create(&tid[i], NULL, &thread_main, &tp[i]);
    }

    for(i = 0; i < p->nthreads; i++){
        pthread_join(tid[i], NULL);
    }


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
    const size_t h_start = tp->h_start, h_end = tp->h_end;
    const int tid = tp->tid;

    double (*restrict src)[tp->h][tp->w] = (double (* restrict)[tp->h][tp->w])src_g;
    double (*restrict dst)[tp->h][tp->w] = (double (* restrict)[tp->h][tp->w])dst_g;
    double (*restrict c)[tp->h][tp->w] = (double (* restrict)[tp->h][tp->w])c_g;
    int *convergence = convergence_g;
    size_t *arrival = arrival_g;
    size_t period = tp->p->period;
    size_t h = tp->h, w = tp->w;
    size_t i, j, iter;


    /* start timer */
    pthread_barrier_wait(&barrier);
    if(tid == 1){
        pthread_mutex_lock(&report_lock);
        clock_gettime(CLOCK_MONOTONIC, &before);
        pthread_mutex_unlock(&report_lock);
    }

    for (iter = 1; iter <= maxiter; ++iter)
    {
        const int cnvg_idx = iter % num_threads;

        {void *tmp = src; src = dst; dst = tmp;}

        for (i = h_start - 1; i < h_end + 1; ++i) {
            (*src)[i][w - 1] = (*src)[i][1];
            (*src)[i][0] = (*src)[i][w - 2];
        }

        double maxdiff = 0.0;

        for (i = h_start; i < h_end; ++i){
            for (j = 1; j < w - 1; ++j){

                double coef = (*c)[i][j];
                double restcoef = 1.0 - coef;

                (*dst)[i][j] = coef * (*src)[i][j] + 
                    /* direct neighbors */
                    ((*src)[i + 1][j    ] + (*src)[i - 1][j    ] + 
                     (*src)[i    ][j + 1] + (*src)[i    ][j - 1]) * (restcoef * c_cdir) +
                    /* diagonal neighbors */
                    ((*src)[i - 1][j - 1] + (*src)[i - 1][j + 1] + 
                     (*src)[i + 1][j - 1] + (*src)[i + 1][j + 1]) * (restcoef * c_cdiag);
            }
            for (j = 1; j < w - 1; ++j) {
                double diff = fabs((*dst)[i][j] - (*src)[i][j]);
                if (diff > maxdiff) maxdiff = diff;
            }
        }

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
                    fill_report_thread(tp->p, tp->r, h_start, h_end, h, w, dst, iter, maxdiff);
                    pthread_barrier_wait(&barrier);
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
            fill_report_thread(tp->p, tp->r, h_start, h_end, h, w, dst, iter, maxdiff);
            pthread_barrier_wait(&barrier);
            /* print by first thread */
            if((tid == 1) && tp->p->printreports){
                pthread_mutex_lock(&report_lock);
                report_results(tp->p, tp->r);
                clear_report_thread(tp->r);
                pthread_mutex_unlock(&report_lock);
            }
        }
    }
}

static int fill_report_thread(const struct parameters *p, struct results *r,
                        size_t h_start, size_t h_end, 
                        size_t h, size_t w, 
                        double (*restrict a)[h][w],
                        double iter, double maxdiff)
{
    /* compute min/max/avg */
    double tmin = INFINITY, tmax = -INFINITY;
    double avg = 0.0;
    struct timespec after;

    /* We have said that the final reduction does not need to be included. */
    clock_gettime(CLOCK_MONOTONIC, &after);
 

    for (size_t i = h_start; i < h_end; ++i){
        for (size_t j = 1; j < w - 1; ++j) 
        {
            double v = (*a)[i][j];
            avg += v;
            if (tmin > v) tmin = v;
            if (tmax < v) tmax = v;
        }
    }
    avg = avg / ((h_end - h_start) * p->M) / p->nthreads;

    /* report lock */
    pthread_mutex_lock(&report_lock);
    r->niter = iter;
    if(r->tmin > tmin) r->tmin = tmin;
    if(r->tmax < tmax) r->tmax = tmax;
    if(r->maxdiff < maxdiff) r->maxdiff = maxdiff;
    r->tavg += avg;
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