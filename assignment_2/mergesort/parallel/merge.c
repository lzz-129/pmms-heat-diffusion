#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <omp.h>
/* Ordering of the vector */
typedef enum Ordering {ASCENDING, DESCENDING, RANDOM} Order;

#define TASK_SIZE 1000

int debug = 0;
int nthrds = 0;

/*
void merge(int start, int mid, int end, int* v, int* temp){
    //int* temp = (int *)malloc((end-start) * sizeof(int));
    int i = start;
    int j = mid;
    int k = start;
    while(i<mid && j<end){
        if(v[i]<= v[j]){
            temp[k++] = v[i++];
        }
        else{
            temp[k++] = v[j++];
        }
    }
    while(i < mid){
        temp[k++] = v[i++];
    }
    while(j<end){
        temp[k++] = v[j++];
    }
    memcpy(v+start, temp+start, (end-start)*sizeof(int));
}
void mergeSort_UpToDown(int* v, int left, long right, long l){
    int *temp = (int*)malloc(l * sizeof(int));
    int i, j;
    for(i = 1; i < right; i *= 2){
        #pragma omp parallel for private(j) shared(right,i)
        for(j = 0; j < right-left; j += (i*2)){
            if(j+i*2 < right){
                merge(j, j+i, j+i*2, v, temp);
            }
            else{
                merge(j, j+i, right, v, temp);
            }
            
        }
    }
}*/

/*merge two list*/
void merge(int* v, long left, long mid, long right, int* temp){
    long i = left;
    long j = mid;
    long k = left;
    while(i<mid && j<right){
        if(v[i]<=v[j]){
            temp[k++] = v[i++];
        }
        else{
            temp[k++] = v[j++];
        }
    }
    while(i < mid){
        temp[k++] = v[i++];
    }
    while(j < right){
        temp[k++] = v[j++];
    }
    memcpy(v+left ,temp+left,(right-left)*sizeof(int));
}
/*split the array untill the length of sub-array is 1*/
void mergeSort_UpToDown(int* v, long left, long right, int* temp){
    if(right-left <= 1){
        return;
    }
    long mid = left + (right-left)/2;
    //printf("left:%ld,mid:%ld,right:%ld,t_n:%d\n", left,mid,right,omp_get_thread_num());

    #pragma omp task shared(v) firstprivate(mid, left, right) if (right-left>TASK_SIZE)
    mergeSort_UpToDown(v, left, mid, temp);

    #pragma omp task shared(v) firstprivate(mid, left, right) if (right-left>TASK_SIZE)
    mergeSort_UpToDown(v, mid, right, temp);
        
    #pragma omp taskwait
    merge(v, left, mid, right, temp);
}

/* Sort vector v of l elements using mergesort, up to down*/
void msort(int *v, long l){
    int *temp = (int*)malloc(l*sizeof(int));
    long left = 0;
    long right = l;
    #pragma omp parallel
    {
        #pragma omp single
        mergeSort_UpToDown(v, left, right, temp);
    }
    free(temp);
}

void print_v(int *v, long l) {
    printf("\n");
    for(long i = 0; i < l; i++) {
        if(i != 0 && (i % 10 == 0)) {
            printf("\n");
        }
        printf("%d ", v[i]);
    }
    printf("\n");
}

int main(int argc, char **argv) {

    int c;
    int seed = 42;
    long length = 1e4;
    int num_threads = 1;
    Order order = ASCENDING;
    int *vector;

    struct timespec before, after;

    /* Read command-line options. */
    while((c = getopt(argc, argv, "adrgp:l:s:")) != -1) {
        switch(c) {
            case 'a':
                order = ASCENDING;
                break;
            case 'd':
                order = DESCENDING;
                break;
            case 'r':
                order = RANDOM;
                break;
            case 'l':
                length = atol(optarg);
                break;
            case 'g':
                debug = 1;
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'p':
                num_threads = atoi(optarg);
                break;
            case '?':
                if(optopt == 'l' || optopt == 's') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                }
                else if(isprint(optopt)) {
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                }
                else {
                    fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                }
                return -1;
            default:
                return -1;
        }
    }

    /* Seed such that we can always reproduce the same random vector */
    srand(seed);

    /* Allocate vector. */
    vector = (int*)malloc(length*sizeof(int));
    if(vector == NULL) {
        fprintf(stderr, "Malloc failed...\n");
        return -1;
    }

    /* Fill vector. */
    switch(order){
        case ASCENDING:
            for(long i = 0; i < length; i++) {
                vector[i] = (int)i;
            }
            break;
        case DESCENDING:
            for(long i = 0; i < length; i++) {
                vector[i] = (int)(length - i);
            }
            break;
        case RANDOM:
            for(long i = 0; i < length; i++) {
                vector[i] = rand();
            }
            break;
    }

    if(debug) {
        print_v(vector, length);
    }

    omp_set_dynamic(0);
    //omp_set_num_threads(num_threads);
    nthrds = num_threads;
    omp_set_num_threads(nthrds);

    clock_gettime(CLOCK_MONOTONIC, &before);

    /* Sort */
    msort(vector, length);

    clock_gettime(CLOCK_MONOTONIC, &after);
    double time = (double)(after.tv_sec - before.tv_sec) +
                  (double)(after.tv_nsec - before.tv_nsec) / 1e9;

    printf("Mergesort took: % .6e seconds \n", time);

    if(debug) {
        print_v(vector, length);
    }
    //printf("Mergesort took: % .6e seconds \n", time);

    return 0;
}

