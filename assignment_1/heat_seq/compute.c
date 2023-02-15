#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "compute.h"

/// Replace/erase the following line:
#include "ref1.c"

//void generate_report();
void find_extvalue(struct results *r, double *tinit_cur);
int is_convergence(double* c, double* l, int M, int N);
void do_compute(const struct parameters* p, struct results *r)
{
    int N = p->N; //rows
    int M = p->M; //columns
    int iterations = 0;
    double weight_dir = sqrt(2)/(sqrt(2)+1);
    double weight_dia = 1/(sqrt(2)+1);
    double t_max, t_min, sum;
    int convergence = 1;

    struct timespec begin, end; //time

    r->tmax = p->io_tmax;
    r->tmin = p->io_tmin;
    r->maxdiff = r->tmax - r->tmin;

    /*initilize temperature matrix*/
    double* tinit_cur= calloc((N+2) * M, sizeof(double));
    double* tinit_last = calloc((N+2) * M, sizeof(double));
    memcpy(tinit_last, p->tinit, M*sizeof(double));
    memcpy(tinit_last+M, p->tinit, M*N*sizeof(double));
    memcpy(tinit_last+M*(N+1), p->tinit+M*(N-1), M*sizeof(double));
    memcpy(tinit_cur, tinit_last, M*(N+2)*sizeof(double));
    printf("%.2f", tinit_last[0]);
    printf("%.2f", tinit_cur[0]);

    /*initilize conduct matrix*/
    double* conductivity = calloc(p->N * p->M, sizeof(double));
    memcpy(conductivity+p->M, p->conductivity, M*N*sizeof(double));
    
    clock_gettime(CLOCK_MONOTONIC, &begin);
    for(iterations=0; iterations < p->maxiter; iterations++){
        
        for(int i = 1; i <= N; i++){
            for(int j = 0; j<M; j++){
                int site_c = (i-1)*M + j;
                int site = i*M + j;
                int site_r = i*M + (j+1)%M;
                int site_l = i*M + (j-1)%M;
                if(j==0){
                    site_l = i*M + (j-1+M)%M;
                }
                double c_dir = (1-conductivity[site_c])*weight_dir/4; //single conductivity weight from direct neighbor
                double c_dia = (1-conductivity[site_c])*weight_dia/4; //single conductivity weight from diagonal neighbor
                double tem_dir = tinit_last[site_l] + tinit_last[site_r] + tinit_last[site-M] + tinit_last[site+M];
                double tem_dia = tinit_last[site_l-M] + tinit_last[site_l+M] + tinit_last[site_r-M] + tinit_last[site_r+M];
                double tem_new = tinit_last[site]*conductivity[site_c] + tem_dir*c_dir + tem_dia*c_dia;   
                if(fabs(tem_new - tinit_last[site]) < p->threshold){
                    convergence = 1 * convergence;
                }
                else{
                    convergence = 0;
                }
                tinit_cur[site] = tem_new;   
            }
        }
        if(iterations%p->period==0){
            clock_gettime(CLOCK_MONOTONIC, &end);
            r->time = (double)(end.tv_nsec-begin.tv_nsec) + (double)(end.tv_nsec-begin.tv_nsec)/1e9;
            r->niter = iterations;
            t_max = t_min = 0;
            sum = 0;
            for(int k=0; k<M*(N+2); k++){
                if(tinit_cur[k] > t_max){
                    t_max = tinit_cur[k];
                }
                if(tinit_cur[k] < t_min){
                    t_min = tinit_cur[k];
                }
                sum += tinit_cur[k];
            }
            r->tmax = t_max;
            r->tmin = t_min;
            r->maxdiff = t_max - t_min;
            r->tavg = (double) sum/(M*(N+2));
            report_results(p,r);
        }
        if(convergence) break;
        memcpy(tinit_last, tinit_cur, (N+2)*M*sizeof(double));
        //to_report(tinit_last);
    }
    if(iterations%p->period>0){
        clock_gettime(CLOCK_MONOTONIC, &end);
        r->time = (double)(end.tv_nsec-begin.tv_nsec) + (double)(end.tv_nsec-begin.tv_nsec)/1e9;
        r->niter = iterations;
        t_max = t_min = 0;
        sum = 0;
        for(int k=0; k<M*(N+2); k++){
            if(tinit_cur[k] > t_max){
                t_max = tinit_cur[k];
            }
            if(tinit_cur[k] < t_min){
                t_min = tinit_cur[k];
            }
            sum += tinit_cur[k];
        }
        r->tmax = t_max;
        r->tmin = t_min;
        r->maxdiff = t_max - t_min;
        r->tavg = (double) sum/(M*(N+2));
        //report_results(p,r);
    }
    //begin_picture(iterations/p->period+1, M, N, 0, 255);
    //for(int i = 0; i < M*N; i++){
    //    draw_point(i/M, i%M, tinit_last[i+M]);
    //}
    //end_picture();
    free(tinit_cur);
    free(tinit_last);
    free(conductivity);
}
