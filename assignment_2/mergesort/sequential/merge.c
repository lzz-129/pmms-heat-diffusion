#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
//#include <omp.h>

/* Ordering of the vector */
typedef enum Ordering {ASCENDING, DESCENDING, RANDOM} Order;

int debug = 0;

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
/*merge two arrays*/
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
/*split the array until the length of subarray is 1*/
void mergeSort_UpToDown(int* v, int left, long right, long l){
    int *temp = (int*)malloc(l * sizeof(int));
    for(int i = 1; i < right; i *= 2){
        for(int j = 0; j < right-left; j += (i*2)){
            if(j+i*2 < right){
                merge(j, j+i, j+i*2, v, temp);
            }
            else{
                merge(j, j+i, right, v, temp);
            }
            
        }
    }
}

/* Sort vector v of l elements using mergesort, up to down*/
void msort(int *v, long l){
    mergeSort_UpToDown(v, 0, l, l);
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

    return 0;
}

