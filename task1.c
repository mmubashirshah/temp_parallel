#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <mpi.h>

#define MAX 100000000
#define MIN 1

int upper_bound;

// Statically allocating boolean arrays in the data segment (avoids stack overflow and malloc)
// We use unsigned char because it maps perfectly to MPI_UNSIGNED_CHAR for bitwise reductions.
static unsigned char local_is_prime_array[MAX + 1];
static unsigned char global_is_prime_array[MAX + 1];

bool is_prime(int value) {
    // if number is 2 -> is prime
    if (value == 2) return true;
    
    // if number less than 1 or is even number -> not prime 
    if (value <= 1 || value % 2 == 0) return false;
    
    // Check for factors from 3 up to the square root of value
    int limit = sqrt(value);
    for (int i = 3; i <= limit; i += 2) {
        if (value % i == 0) return false;
    }
    
    // If no factors are found, the number is prime
    return true;
}

int main(int argc, char *argv[]) {
    
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) { // Input Safety Checks
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

    // Broadcast the upper bound from root to all other processes
    MPI_Bcast(&upper_bound, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Synchronize for precise timing
    MPI_Barrier(MPI_COMM_WORLD);
    
    double t_total_start;
    if (rank == 0) t_total_start = MPI_Wtime();

    // === PARALLEL SECTION START ===
    double t_parallel_start = MPI_Wtime();
    
    // Cyclic Workload Distribution: each process jumps by 'size' to perfectly balance the load
    for (int current_number = rank + MIN; current_number < upper_bound; current_number += size) {
        if (is_prime(current_number)) {
            local_is_prime_array[current_number] = 1;
        }
    }
    
    double t_parallel_end = MPI_Wtime();
    // === PARALLEL SECTION END ===

    double local_parallel_time = t_parallel_end - t_parallel_start;
    double max_parallel_time;
    // Find the maximum time any single process took (this represents the parallel bottleneck)
    MPI_Reduce(&local_parallel_time, &max_parallel_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // === SERIAL SECTION START ===
    double t_serial_start;
    if (rank == 0) t_serial_start = MPI_Wtime();

    // Reduce all local boolean arrays into the global boolean array using Bitwise OR (MPI_BOR).
    // This perfectly combines everyone's arrays directly, completely avoiding malloc and qsort!
    MPI_Reduce(local_is_prime_array, global_is_prime_array, upper_bound + 1, MPI_UNSIGNED_CHAR, MPI_BOR, 0, MPI_COMM_WORLD);

    // Root process wraps up, counts, and writes to file
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

        // CSV OUTPUT: Task, N, Procs, Threads, T_Serial, T_Parallel, T_Total
        printf("Task1, %d, %d, 1, %f, %f, %f\n", upper_bound, size, t_serial, max_parallel_time, t_total);
    }

    MPI_Finalize();
    return 0; 
}
