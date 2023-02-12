#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "compute.h"

/// Replace/erase the following line:
#include "ref1.c"
/** 
*location:
***********
*****123***
*****405***
*****678***
***********
**/
struct location{
    int loc_0;
    int loc_1;
    int loc_2;
    int loc_3;
    int loc_4;
    int loc_5;
    int loc_6;
    int loc_7;
    int loc_8;
};
void to_result(double* t, struct results *r, int length){
    double max, min;
    max = t[0];
    min = t[0];
    double sum = 0.0;
    for(int len = 0; len < length; len++){
        if(t[len]>max){
            max = t[len];
        }
        if(t[len]<min){
            min = t[len];
        }
        sum += t[len];
    }
    r->tmax = max;
    r->tmin = min;
    r->maxdiff = max - min;
    r->tavg = sum/length;
}
void do_compute(const struct parameters* p, struct results *r)
{
    /// Replace/erase the following line:
    //#include "ref2.c"
    /*initialize result*/
    r->maxdiff = -1.0;
    r->niter = 0;
    r->tavg = 0.0;
    r->time = 0.0;
    r->tmax = 0.0;
    r->tmin = 0.0;

    /*creat a temp data to store last temperature*/
    double* tmp = calloc(p->N * p->M, sizeof(double));
    double* heat_iter = calloc(p->N * p->M, sizeof(double));
    memcpy(tmp, p->tinit, p->M*p->N);
    memcpy(heat_iter, p->tinit, p->M*p->N);
    double t_self; //self-temperature
    double t_dir; //temperature from direct neighbours
    double t_dia; //temperature from diagonal neighbours
    double weight_1 = sqrt(2)/(sqrt(2)+1);
    double weight_2 = 1 - weight_1;
    struct location loc;
    clock_t t_start, t_end, t_last, t_cur;
    
    t_start = clock();
    for(int iter = 0; iter < p->maxiter; iter++){
        for(int i =1; i<p->N-1; i++){
            for(int j=0; j<p->M; j++){
                //loc = (i*p->N) + (j%p->M);
                loc.loc_0 = (i*p->N) + (j%p->M);
                loc.loc_2 = ((i-1)*p->N) + ((j)%p->M);
                loc.loc_3 = ((i-1)*p->N) + ((j+1)%p->M);
                loc.loc_5 = (i*p->N) + ((j+1)%p->M);
                loc.loc_7 = ((i+1)*p->N) + ((j)%p->M);
                loc.loc_8 = ((i+1)*p->N) + ((j+1)%p->M);
                if(j == 0){
                    loc.loc_1 = ((i-1)*p->N) + p->M-1;
                    loc.loc_6 = ((i+1)*p->N) + p->M-1;
                    loc.loc_4 = (i*p->N) + p->M-1;
                }
                else{
                    loc.loc_1 = ((i-1)*p->N) + ((j-1)%p->M);
                    loc.loc_4 = (i*p->N) + ((j-1)%p->M);
                    loc.loc_6 = ((i+1)*p->N) + ((j-1)%p->M);
                }
                t_self = tmp[loc.loc_0] * (*(p->conductivity+loc.loc_0));
                t_dir = (tmp[loc.loc_2] + tmp[loc.loc_4] + tmp[loc.loc_5] + tmp[loc.loc_7])/4 * weight_1;
                t_dia = (tmp[loc.loc_1] + tmp[loc.loc_3] + tmp[loc.loc_6] + tmp[loc.loc_8])/4 * weight_2;
                heat_iter[loc.loc_0] = t_self + t_dir + t_dia; 
            }
        }
        if(iter%p->period == 0 && iter > 0){
            t_cur = clock();
            to_result(heat_iter, r, p->M*p->N);
            r->niter = iter;
            r->time = t_cur - t_last;
            t_last = t_cur;
            report_results(p, r);
        }
        memcpy(tmp, heat_iter, p->M*p->N);
    }
    t_end = clock();
}