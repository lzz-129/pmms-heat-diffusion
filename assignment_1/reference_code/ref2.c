    size_t i, j;

    /* alias input parameters */
    const double (*restrict tinit)[p->N][p->M] = (const double (*)[p->N][p->M])p->tinit;
    const double (*restrict cinit)[p->N][p->M] = (const double (*)[p->N][p->M])p->conductivity;

    /* allocate grid data */
    const size_t h = p->N + 2;
    const size_t w = p->M + 2;
    double (*restrict g1)[h][w] = malloc(h * w * sizeof(double));
    double (*restrict g2)[h][w] = malloc(h * w * sizeof(double));

    /* allocate halo for conductivities */
    double (*restrict c)[h][w] = malloc(h * w * sizeof(double));

    struct timespec before;

    static const double c_cdir = 0.25 * M_SQRT2 / (M_SQRT2 + 1.0);
    static const double c_cdiag = 0.25 / (M_SQRT2 + 1.0);

    /* set initial temperatures and conductivities */
    for (i = 1; i < h - 1; ++i)
        for (j = 1; j < w - 1; ++j) 
        {
            (*g1)[i][j] = (*tinit)[i-1][j-1];
            (*c)[i][j] = (*cinit)[i-1][j-1];
        }

    /* smear outermost row to border */
    for (j = 1; j < w-1; ++j) {
        (*g1)[0][j] = (*g2)[0][j] = (*g1)[1][j];
        (*g1)[h-1][j] = (*g2)[h-1][j] = (*g1)[h-2][j];
    }

    /* compute */
    size_t iter;
    double (*restrict src)[h][w] = g2;
    double (*restrict dst)[h][w] = g1;

    /* 
     * If initialization should be included in the timings
     * could be a point of discussion. 
     */
    clock_gettime(CLOCK_MONOTONIC, &before);

    for (iter = 1; iter <= p->maxiter; ++iter)
    {
        /* swap source and destination */
        { void *tmp = src; src = dst; dst = tmp; }

        /* initialize halo on source */
        do_copy(h, w, src);

        double maxdiff = 0.0;
        /* compute */
        for (i = 1; i < h - 1; ++i)
            for (j = 1; j < w - 1; ++j)
            {
                double w = (*c)[i][j];
                double restw = 1.0 - w;

                (*dst)[i][j] = w * (*src)[i][j] + 

                    ((*src)[i+1][j  ] + (*src)[i-1][j  ] + 
                     (*src)[i  ][j+1] + (*src)[i  ][j-1]) * (restw * c_cdir) +

                    ((*src)[i-1][j-1] + (*src)[i-1][j+1] + 
                     (*src)[i+1][j-1] + (*src)[i+1][j+1]) * (restw * c_cdiag);

                double diff = fabs((*dst)[i][j] - (*src)[i][j]);
                if (diff > maxdiff) maxdiff = diff;
            }
           r->maxdiff=maxdiff;
        if(maxdiff<p->threshold){iter++;break;} 
        /* conditional reporting */
        if (iter % p->period == 0) {
           fill_report(p, r, h, w, dst, src, iter, &before);
            if(p->printreports) report_results(p, r);
        }
        //#ifdef GEN_PICTURES
        //do_draw(p, iter, h, w, src);
        //#endif
    }

    /* report at end in all cases */
    iter--;
    fill_report(p, r, h, w, dst, src, iter, &before);

    free(c);
    free(g2);
    free(g1);
