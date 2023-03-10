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

#define OMP

/* Does the reduction step and return if the convergence has setteled */
static int fill_report(const struct parameters *p, struct results *r,
                        size_t h, size_t w, 
                        double (*restrict a)[h][w],
                        double (*restrict b)[h][w],
                        double iter,
                        struct timespec *before)
{
    /* compute min/max/avg */
    double tmin = INFINITY, tmax = -INFINITY;
    double sum = 0.0;
    double maxdiff = 0.0;
    struct timespec after;

    /* We have said that the final reduction does not need to be included. */
    clock_gettime(CLOCK_MONOTONIC, &after);
 
    #ifdef OMP
    #pragma omp parallel for collapse(2) reduction (+: sum) reduction (min: tmin) reduction (max: tmax) schedule(static, (int)((p->N*p->M + p->nthreads - 1)/p->nthreads))
    #endif
    for (size_t i = 1; i < h - 1; ++i){
        for (size_t j = 1; j < w - 1; ++j) 
        {
            double v = (*a)[i][j];
            // double v_old = (*b)[i][j];
            sum += v;
            if (tmin > v) tmin = v;
            if (tmax < v) tmax = v;
        }
    }

    r->niter = iter;
    r->tmin = tmin;
    r->tmax = tmax;
    r->tavg = sum / (p->N * p->M);

    r->time = (double)(after.tv_sec - before->tv_sec) + (double)(after.tv_nsec - before->tv_nsec) / 1e9;


    return 0;
}
