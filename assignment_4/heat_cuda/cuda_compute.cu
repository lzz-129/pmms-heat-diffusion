#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <cuda.h>
#include "output.h"

#define BLOCK_SIZE 256
// #define GRID_SIZE 32
#define WARP_SIZE 32
static void checkCudaCall(cudaError_t result) {
    if (result != cudaSuccess) {
        printf("cuda error \n");
        exit(1);
    }
}

__global__ void heatPaddingKernel(const size_t h, const size_t w, double *src) {
    
    unsigned int tid = blockIdx.x * blockDim.x + threadIdx.x;

    if(tid >= h) return;

    int curpos = tid * w;
    //one thread do one line
    src[curpos] = src[curpos + w - 2];
    src[curpos + w - 1] = src[curpos + 1];


}
__global__ void heatComputeKernel(const size_t h, const size_t w, double *src, double *dst, double *c) {
    
    unsigned int tid = blockIdx.x * blockDim.x + threadIdx.x;


    if(tid >= (h - 2) * (w - 2)) return;

    
    int curpos = tid + w + (int)(tid / (w-2)) * 2 + 1;
    int uppos = curpos - w;
    int lowpos = curpos + w;

    const double coef = c[curpos];
    const double restcoef = 1.0 - coef;
    const double c_cdir = 0.25 * M_SQRT2 / (M_SQRT2 + 1.0);
    const double c_cdiag = 0.25 / (M_SQRT2 + 1.0);

    dst[curpos] = coef * *(src + curpos) +
        /* direct neighbors */
        (*(src + curpos + 1) + *(src + curpos - 1) + 
        *(src + uppos) + *(src + lowpos)) * (restcoef * c_cdir) +
        /* diagonal neighbors */
        (*(src + uppos - 1) + *(src + uppos + 1) + 
        *(src + lowpos - 1) + *(src + lowpos + 1)) * (restcoef * c_cdiag);


}
__global__ void heatMaxdiffKernel(const size_t h, const size_t w, double *src, double *dst, double *output) {

    unsigned int tid = blockIdx.x * blockDim.x + threadIdx.x;
    __shared__ double maxdiff[BLOCK_SIZE];
    

    if(tid >= (h - 2) * (w - 2)){
        maxdiff[threadIdx.x] = 0.0;
    }else{
        int curpos = tid + w + (int)(tid / (w-2)) * 2 + 1;
        maxdiff[threadIdx.x] = fabs(dst[curpos] - src[curpos]);
    }

    __syncthreads();


    for(unsigned int i = BLOCK_SIZE / 2; i > 0; i /= 2) {
        if(threadIdx.x < i){
            if(maxdiff[threadIdx.x + i] > maxdiff[threadIdx.x]) maxdiff[threadIdx.x] = maxdiff[threadIdx.x + i];
        }
        __syncthreads();
    }
    

    if(threadIdx.x == 0){
        output[blockIdx.x] = maxdiff[0];
    }
    
}

__global__ void heatFillReportKernel(const size_t h, const size_t w, double *src, double *dst, double *output) {

    unsigned int tid = blockIdx.x * blockDim.x + threadIdx.x;

    __shared__ double sh_max[BLOCK_SIZE];
    __shared__ double sh_min[BLOCK_SIZE];
    __shared__ double sh_avg[BLOCK_SIZE];

    // printf("tid = %ld\n", tid);
    if(tid >= (h - 2) * (w - 2)){
        sh_max[threadIdx.x] = -INFINITY;
        sh_min[threadIdx.x] = INFINITY;
        sh_avg[threadIdx.x] = 0;
    }else{
        int curpos = tid + w + (int)(tid / (w-2)) * 2 + 1;
        double val = dst[curpos];
        sh_max[threadIdx.x] = val;
        sh_min[threadIdx.x] = val;
        sh_avg[threadIdx.x] = val;
    }

    __syncthreads();


    for(unsigned int i = BLOCK_SIZE / 2; i > 0; i /= 2) {
        if(threadIdx.x < i){
            if(sh_max[threadIdx.x + i] > sh_max[threadIdx.x]) sh_max[threadIdx.x] = sh_max[threadIdx.x + i];
            if(sh_min[threadIdx.x + i] < sh_min[threadIdx.x]) sh_min[threadIdx.x] = sh_min[threadIdx.x + i];
            sh_avg[threadIdx.x] += sh_avg[threadIdx.x + i];
        }
        __syncthreads();
    }

    if(threadIdx.x == 0){
        output[blockIdx.x * 3] = sh_max[0];
        output[blockIdx.x * 3 + 1] = sh_min[0];
        output[blockIdx.x * 3 + 2] = sh_avg[0];
    }    

}

extern "C"
void cuda_do_report(const size_t h, const size_t w, double *src_dev, double *dst_dev, struct results *r)
{
    int block_size = BLOCK_SIZE;
    int grid_size = (int)ceil(((h - 2) * (w - 2))/(double)block_size);
    double *report_dev, report[grid_size][3];
    /* init globlal memory */
    checkCudaCall(cudaMalloc((void **) &report_dev, 3 * grid_size * sizeof(double)));
    /* launch kernel */
    heatFillReportKernel<<<grid_size, block_size>>>(h, w, src_dev, dst_dev, report_dev);
    cudaDeviceSynchronize();
    /* copy back result */
    checkCudaCall(cudaMemcpy(report, report_dev, 3 * grid_size * sizeof(double), cudaMemcpyDeviceToHost));
    checkCudaCall(cudaFree((void *)report_dev));
    double sum = 0;
    for(int i = 0; i < grid_size; i++){
        if(report[i][0] > r->tmax) r->tmax = report[i][0];
        if(report[i][1] < r->tmin) r->tmin = report[i][1];
        sum += report[i][2];
    }
    r->tavg = r->tavg  + (sum / ((h-2) * (w-2)));

}

extern "C" 
void cuda_do_init(size_t h, size_t w, double *src, double *dst, double *c, double **src_dev, double **dst_dev, double **c_dev){
    
    /* allocate device memory */
    checkCudaCall(cudaMalloc((void **) src_dev, h * w * sizeof(double)));
    checkCudaCall(cudaMalloc((void **) dst_dev, h * w * sizeof(double)));
    checkCudaCall(cudaMalloc((void **) c_dev, h * w * sizeof(double)));


    /* copy memory to device */
    checkCudaCall(cudaMemcpy(*src_dev, src, h * w * sizeof(double), cudaMemcpyHostToDevice));
    checkCudaCall(cudaMemcpy(*dst_dev, dst, h * w * sizeof(double), cudaMemcpyHostToDevice));
    checkCudaCall(cudaMemcpy(*c_dev, c, h * w * sizeof(double), cudaMemcpyHostToDevice));

    cudaDeviceSynchronize();
}
extern "C" 
double cuda_do_compute(size_t h, size_t w, double *src, double *dst, double *c){

    int block_size = BLOCK_SIZE;
    int grid_size = (int)ceil(((h - 2) * (w - 2))/(double)block_size);

    heatPaddingKernel<<<(int)ceil(h/(double)block_size), block_size>>>(h, w, src);
    cudaDeviceSynchronize();

    heatComputeKernel<<<grid_size, block_size>>>(h, w, src, dst, c);
    cudaDeviceSynchronize();

    double *maxdiff_dev, maxdiff[grid_size], diff = 0;
    /* init globlal memory */
    checkCudaCall(cudaMalloc((void **) &maxdiff_dev, grid_size * sizeof(double)));
    /* launch kernel */
    heatMaxdiffKernel<<<grid_size, block_size>>>(h, w, src, dst, maxdiff_dev);
    cudaDeviceSynchronize();
    /* copy back result */
    checkCudaCall(cudaMemcpy(maxdiff, maxdiff_dev, grid_size * sizeof(double), cudaMemcpyDeviceToHost));
    checkCudaCall(cudaFree((void *)maxdiff_dev));
    for(int i = 0; i < grid_size; i++){
        if(maxdiff[i] > diff) diff = maxdiff[i];
    }
    return diff;
}
extern "C" 
void cuda_do_deinit(double *src_dev, double *dst_dev, double *c_dev){

    checkCudaCall(cudaFree((void *)src_dev));
    checkCudaCall(cudaFree((void *)dst_dev));
    checkCudaCall(cudaFree((void *)c_dev));

}