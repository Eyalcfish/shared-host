#include "shared_host.h"
#include <stdio.h>
#include <time.h>

#define ITERS 10000

int main()
{
    printf("Starting benchmark...\n");
    clock_t start = clock();

    for (int i = 0; i < ITERS; i++)
    {
        shared_host_connection *conn = NULL;
        char port[64];
        sprintf(port, "bench_port_%d", i);
        create_shared_host_connection(port, &conn);
        close_shared_host_connection(conn);
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Benchmark: %f seconds for %d connection creations and closures.\n", time_spent, ITERS);
    return 0;
}
