#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <omp.h>
#include <stdio.h>

// #define GEN_PICTURES

#include "compute.h"
#include "ref1.c"

void do_compute(const struct parameters* p, struct results *r)
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

    struct timespec before;

    static const double c_cdir = 0.25 * M_SQRT2 / (M_SQRT2 + 1.0);
    static const double c_cdiag = 0.25 / (M_SQRT2 + 1.0);

    /* set thread */
    #ifdef OMP
    omp_set_num_threads(p->nthreads);
    #endif

    /* set initial temperatures and conductivities */
    #ifdef OMP
    #pragma omp parallel for schedule(static, (int)((p->N + p->nthreads - 1)/p->nthreads))
    #endif
    for (i = 1; i < h - 1; ++i) {
        for (j = 1; j < w - 1; ++j) {
            g1[i][j] = (*tinit)[i - 1][j - 1];
            c[i][j] = (*cinit)[i - 1][j - 1];
        }
    }

    /* smear outermost row to border */
    #ifdef OMP
    #pragma omp parallel for schedule(static, (int)((p->M + p->nthreads - 1)/p->nthreads))
    #endif
    for (j = 1; j < w - 1; ++j) {
        g1[0][j] = g2[0][j] = g1[1][j];
        g1[h - 1][j] = g2[h - 1][j] = g1[h - 2][j];
    }

    /* compute */
    size_t iter;
    double (*restrict src)[h][w] = &g2;
    double (*restrict dst)[h][w] = &g1;

    /* 
     * If initialization should be included in the timings
     * could be a point of discussion. 
     */
    clock_gettime(CLOCK_MONOTONIC, &before);

    for (iter = 1; iter <= p->maxiter; ++iter)
    {
        
        {void *tmp = src; src = dst; dst = tmp;}

        double maxdiff = 0.0;
        #ifdef OMP
        #pragma omp parallel shared(maxdiff)
        #endif
        {

        #ifdef OMP
        #pragma omp for schedule(static, (int)((h + p->nthreads - 1)/p->nthreads))
        #endif
        for (i = 0; i < h; ++i) {
            (*src)[i][w - 1] = (*src)[i][1];
            (*src)[i][0] = (*src)[i][w - 2];
        }
        
        #ifdef OMP
        #pragma omp for reduction (max: maxdiff) schedule(static, (int)((p->N + p->nthreads - 1)/p->nthreads))
        #endif
        for (i = 1; i < h - 1; ++i){
            for (j = 1; j < w - 1; ++j){

                double coef = c[i][j];
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
        }
        r->maxdiff = maxdiff;
        if (maxdiff < p->threshold) {
            ++iter;
            break;
        }
        /* conditional reporting */
        if (iter % p->period == 0) {
            fill_report(p, r, h, w, dst, src, iter, &before);
            if(p->printreports) report_results(p, r);
        }
    }

    // #ifdef GEN_PICTURES
    // do_draw(p, iter, h, w, src);
    // #endif

    /* report at end in all cases */
    --iter;
    fill_report(p, r, h, w, dst, src, iter, &before);

}
