#include <stdio.h> // for IO
#include <stdlib.h> // for atoi
#include <math.h> // for sqrt
#include <stdbool.h> // for boolean values
#include <time.h> // for measuring time taken by program

#define MAX 100000000
#define MIN 1

int upper_bound;

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
    if (argc != 2) {
        printf("Usage: %s <upper_bound>\n", argv[0]);
        return 1;
    }
    
    upper_bound = atoi(argv[1]);
    if (upper_bound < MIN || upper_bound > MAX) {
        printf("Invalid input. Upper bound must be between %d and %d.\n", MIN, MAX);
        return 1;
    }
    
    // Record the start time
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Parallel-equivalent section
    for (int current_number = MIN; current_number < upper_bound; current_number++){
        if (is_prime(current_number)){ 
            is_prime_array[current_number] = 1;
        }
    }

    // Record the end time
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
    // For pure serial, T_Parallel is the loop, T_Serial is 0
    printf("Baseline_Serial, %d, 1, 1, 0.000000, %f, %f\n", upper_bound, time_taken, time_taken);

    return 0; 
}
