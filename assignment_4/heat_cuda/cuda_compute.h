#ifndef CUDA_COMPUTE_H
#define CUDA_COMPUTE_H

#include "output.h"

#ifdef __cplusplus
extern "C" 
#endif 


double cuda_do_compute(size_t h, size_t w, double *src, double *dst, double *c);
void cuda_do_init(size_t h, size_t w, double *src, double *dst, double *c, double **src_dev, double **dst_dev, double **c_dev);
void cuda_do_deinit(double *src_dev, double *dst_dev, double *c_dev);
void cuda_do_report(const size_t h, const size_t w, double *src_dev, double *dst_dev, struct results *r);


#endif
