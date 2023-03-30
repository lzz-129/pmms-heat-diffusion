#include "compute.h"

int main(int argc, char **argv)
{
    struct parameters p;
    struct results r;

    read_parameters(&p, argc, argv);

    do_compute(&p, &r);

    if(r.niter){
        report_results(&p, &r);
    }
    

    return 0;
}
