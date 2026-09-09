#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <mpi.h>
#include <omp.h>

#define MAX 100000000
#define MIN 1

int upper_bound;

// Statically allocating boolean arrays in the data segment (avoids stack overflow and malloc)
// We use unsigned char because it maps perfectly to MPI_UNSIGNED_CHAR for bitwise reductions.
static unsigned char local_is_prime_array[MAX + 1];
static unsigned char global_is_prime_array[MAX + 1];

bool is_prime(int value) {
    if (value == 2) return true;
    if (value <= 1 || value % 2 == 0) return false;
    
    int limit = sqrt(value);
    for (int i = 3; i <= limit; i += 2) {
        if (value % i == 0) return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    int rank, size, provided;
    
    // Initialize MPI with thread support requested for OpenMP
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        if (argc < 2) {
            printf("Error: Missing upper bound argument.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        upper_bound = atoi(argv[1]);
        if (upper_bound < MIN || upper_bound > MAX) {
            printf("Invalid input. Upper bound must be between %d and %d.\n", MIN, MAX);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Bcast(&upper_bound, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    
    double t_total_start;
    if (rank == 0) t_total_start = MPI_Wtime();

    // === PARALLEL SECTION START ===
    double t_parallel_start = MPI_Wtime();
    
    // OpenMP schedule(dynamic, 1000) handles thread load balancing within the node.
    // Cyclic jumping (+= size) handles block-load balancing across the MPI processes/nodes.
    #pragma omp parallel for schedule(dynamic, 1000)
    for (int current_number = rank + MIN; current_number < upper_bound; current_number += size) {
        if (is_prime(current_number)) {
            // Assignment is lock-free
            local_is_prime_array[current_number] = 1;
        }
    }
    
    double t_parallel_end = MPI_Wtime();
    // === PARALLEL SECTION END ===

    double local_parallel_time = t_parallel_end - t_parallel_start;
    double max_parallel_time;
    MPI_Reduce(&local_parallel_time, &max_parallel_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // === SERIAL SECTION START ===
    double t_serial_start;
    if (rank == 0) t_serial_start = MPI_Wtime();

    // Reduce all local boolean arrays into the global boolean array using Bitwise OR (MPI_BOR).
    // This perfectly combines everyone's arrays directly, completely avoiding malloc and qsort!
    MPI_Reduce(local_is_prime_array, global_is_prime_array, upper_bound + 1, MPI_UNSIGNED_CHAR, MPI_BOR, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double t_serial_end = MPI_Wtime();
        // === SERIAL SECTION END ===
        
        double t_total_end = MPI_Wtime();

        double t_serial = t_serial_end - t_serial_start;
        double t_total = t_total_end - t_total_start;

        int total_primes_found = 0;
        FILE *output_file = fopen("primes.txt", "w");
        
        // Iterating through the array sequentially guarantees the primes are naturally sorted in ascending order!
        for (int i = MIN; i < upper_bound; i++) {
            if (global_is_prime_array[i] == 1) {
                total_primes_found++;
                if (output_file != NULL) {
                    fprintf(output_file, "%d ", i);
                }
            }
        }
        
        if (output_file != NULL) fclose(output_file);
        
        // Get OpenMP max threads to log in CSV
        int num_threads = omp_get_max_threads();

        // CSV OUTPUT: Task, N, Procs, Threads, T_Serial, T_Parallel, T_Total
        printf("Task2, %d, %d, %d, %f, %f, %f\n", upper_bound, size, num_threads, t_serial, max_parallel_time, t_total);
    }

    MPI_Finalize();
    return 0; 
}
