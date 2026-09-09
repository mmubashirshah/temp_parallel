#include <stdio.h> // for IO
#include <stdlib.h> // for atoi
#include <math.h> // for sqrt
#include <stdbool.h> // for boolean values
#include <time.h> // for measuring time taken by program
#include <omp.h> // for OpenMP

#define MAX 100000000
#define MIN 1

int upper_bound;
int num_threads;

// Allocate memory as a static boolean array in the data segment
static unsigned char is_prime_array[MAX + 1];

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
    if (argc != 3) {
        printf("Usage: %s <upper_bound> <num_threads>\n", argv[0]);
        return 1;
    }
    
    upper_bound = atoi(argv[1]);
    num_threads = atoi(argv[2]);
    
    if (upper_bound < MIN || upper_bound > MAX) {
        printf("Invalid input. Upper bound must be between %d and %d.\n", MIN, MAX);
        return 1;
    }
    
    // set number of threads in OpenMP
    omp_set_num_threads(num_threads);

    // record start time
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    #pragma omp parallel for schedule(dynamic, 1000)
    for (int current_number = MIN; current_number < upper_bound; current_number++){
        if (is_prime(current_number)){
            is_prime_array[current_number] = 1;
        }
    }

    // record end time
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    // Calculate the time taken in wall-clock seconds
    double time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e9; 
    time_taken = (time_taken + (end_time.tv_nsec - start_time.tv_nsec)) * 1e-9; 

    // Write primes to file (this also naturally prevents compiler dead-code elimination)
    int total_primes_found = 0;
    FILE *output_file = fopen("primes.txt", "w");
    
    for (int i = MIN; i < upper_bound; i++) {
        if (is_prime_array[i] == 1) {
            total_primes_found++;
            if (output_file != NULL) {
                fprintf(output_file, "%d ", i);
            }
        }
    }
    
    if (output_file != NULL) fclose(output_file);
    
    // Output strictly in the CSV format required for plotting
    // Format: Task, N, Procs, Threads, T_Serial, T_Parallel, T_Total
    printf("Baseline_OMP, %d, 1, %d, 0.000000, %f, %f\n", upper_bound, num_threads, time_taken, time_taken);

    return 0; 
}
