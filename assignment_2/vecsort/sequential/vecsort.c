#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <omp.h>
#include <string.h>
#include <assert.h>

/* Ordering of the vector */
typedef enum Ordering {ASCENDING, DESCENDING, RANDOM} Order;

int debug = 0;

void msort(int *v, long l){
    long loop_num=l/2l,width=1l,left,mid,right,j,k,i;
    int *b = (int *)malloc(l * sizeof(int));
    do{
        width*=2;

        for (long left = 0l; left < l; left+=width) {
            right=left+width<l?left+width:l,mid=left+width/2,i=left,j=left,k=mid;
            if (mid>=l){
                while (j<l){
                    b[i++]=v[j++];
                }
            } else{
                while (j<mid&&k<right){
                    b[i++]=v[j]<=v[k]?v[j++]:v[k++];
                }
                while (j<mid){
                    b[i++]=v[j++];
                }
                while (k<right){
                    b[i++]=v[k++];
                }
            }


        }

        memcpy(v, b, l * sizeof(int ));
    }while (width<l);
    free(b);
    b=NULL;
}

int compare_vec(int *v1, int *v2, int *v_l, long j, long k){
    long i=0l,l1=v_l[j],l2=v_l[k];
    int r=-1;
    while (i<l1 && i<l2){
        if(v1[i]!=v2[i]) {
            r= v1[i]<=v2[i]?1:0; //1: ascending
            break;
        }
        i++;
    }

    r = r==-1?l1<=l2?1:0:r;

    return r;

}

void vecsort(int **v, int *v_l, long l){
    //TODO: Just Do It. Don't let your dreams be dreams.
    long loop_num=l/2l,width=1l,left,mid,right,j,k,i;
    int **v_order = (int **)  calloc(l , sizeof(int*));
    int *len_v = calloc(l , sizeof(int));
    int compare_r;
    for (int m = 0; m < l; m++) {

        msort(v[m],v_l[m]);//sort inner
    }
    do{
        //parallel
        width*=2;

        for (long left = 0l; left < l; left+=width) {
            right=left+width<l?left+width:l,mid=left+width/2,i=left,j=left,k=mid;
            if (mid>=l) {
                while (j<l){
                    len_v[i] = v_l[j];
                    v_order[i++]=v[j++];
                }
            } else{
                while (j<mid&&k<right){
                    compare_r=compare_vec(v[j],v[k],v_l,j,k);
                    len_v[i] = compare_r==1 ? v_l[j]:v_l[k];
                    v_order[i++]= compare_r==1?v[j++]:v[k++];

                }
                while (j<mid){
                    len_v[i] = v_l[j];
                    v_order[i++]=v[j++];
                }
                while (k<right){
                    len_v[i] = v_l[k];
                    v_order[i++]=v[k++];
                }
            }


        }

        memcpy(v, v_order, l * sizeof(int* ));
        memcpy(v_l, len_v, l * sizeof(int ));
    }while (width<l);

}

void print_v(int **vector_vectors, int *vector_lengths, long length_outer) {
    printf("\n");
    for(long i = 0; i < length_outer; i++) {
        for (int j = 0; j < vector_lengths[i]; j++){
            if(j != 0 && (j % 10 == 0)) {
                printf("\n");
            }
            printf("%d ", vector_vectors[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

int main(int argc, char **argv) {

    int c;
    int seed = 42;
    long length_outer = 1e4;
    int num_threads = 1;
    Order order = ASCENDING;
    int length_inner_min = 100;
    int length_inner_max = 1000;

    int **vector_vectors;
    int *vector_lengths;

    struct timespec before, after;


    /* Read command-line options. */
    while ((c = getopt(argc, argv, "adrgn:x:l:p:s:")) != -1) {
        switch (c) {
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
                length_outer = atol(optarg);
                break;
            case 'n':
                length_inner_min = atoi(optarg);
                break;
            case 'x':
                length_inner_max = atoi(optarg);
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
                if (optopt == 'l' || optopt == 's') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                } else {
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
    vector_vectors = (int **) malloc(length_outer * sizeof(int*));
    vector_lengths = (int *) malloc(length_outer * sizeof(int));
    if (vector_vectors == NULL || vector_lengths == NULL) {
        fprintf(stderr, "Malloc failed...\n");
        return -1;
    }

    assert(length_inner_min < length_inner_max);

    /* Determine length of inner vectors and fill them. */
    for (long i = 0; i < length_outer; i++) {
        int length_inner = (rand() % (length_inner_max + 1 - length_inner_min)) + length_inner_min ; //random number inclusive between min and max
        vector_vectors[i] = (int *) malloc(length_inner * sizeof(int));
        vector_lengths[i] = length_inner;

        /* Allocate and fill inner vector. */
        switch (order) {
            case ASCENDING:
                for (long j = 0; j < length_inner; j++) {
                    vector_vectors[i][j] = (int) j;
                }
                break;
            case DESCENDING:
                for (long j = 0; j < length_inner; j++) {
                    vector_vectors[i][j] = (int) (length_inner - j);
                }
                break;
            case RANDOM:
                for (long j = 0; j < length_inner; j++) {
                    vector_vectors[i][j] = rand();
                }
                break;
        }
    }

    if(debug) {
        print_v(vector_vectors, vector_lengths, length_outer);
    }

    clock_gettime(CLOCK_MONOTONIC, &before);

    /* Sort */
    vecsort(vector_vectors,vector_lengths,length_outer);

    clock_gettime(CLOCK_MONOTONIC, &after);
    double time = (double)(after.tv_sec - before.tv_sec) +
              (double)(after.tv_nsec - before.tv_nsec) / 1e9;

    printf("Vecsort took: % .6e \n", time);

    if(debug) {
        print_v(vector_vectors, vector_lengths, length_outer);
    }

    return 0;
}

