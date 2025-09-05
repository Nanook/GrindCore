#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "lzma_test_minimal.h"

#define BLOCK_SIZE (64 * 1024)
#define NUM_THREADS 16
#define ITERATIONS_PER_THREAD 100

struct thread_arg {
    int tid;
    int fail;
    int completed;
};

// Simulate rapid create/destroy pattern similar to C# LzmaBlock
void* stress_thread_func(void* arg) {
    struct thread_arg* t_arg = (struct thread_arg*)arg;
    int tid = t_arg->tid;
    
    printf("Thread %d starting...\n", tid);
    
    for (int iter = 0; iter < ITERATIONS_PER_THREAD; ++iter) {
        // Create test data
        uint8_t* input = malloc(BLOCK_SIZE);
        if (!input) {
            fprintf(stderr, "Thread %d: Failed to allocate input buffer\n", tid);
            t_arg->fail = 1;
            return NULL;
        }
        
        // Fill with semi-random data
        for (size_t i = 0; i < BLOCK_SIZE; ++i)
            input[i] = (uint8_t)(((i + tid + iter) % 251));
        
        // Allocate output buffer
        size_t max_output = BLOCK_SIZE + (BLOCK_SIZE / 2) + 0x10;
        uint8_t* output = malloc(max_output);
        if (!output) {
            fprintf(stderr, "Thread %d: Failed to allocate output buffer\n", tid);
            free(input);
            t_arg->fail = 1;
            return NULL;
        }
        
        // Create encoder - this is where the race condition likely occurs
        void* enc = SZ_Lzma_v25_01_Enc_Create();
        if (!enc) {
            fprintf(stderr, "Thread %d: Failed to create encoder handle (iter %d)\n", tid, iter);
            free(input); 
            free(output);
            t_arg->fail = 1;
            return NULL;
        }
        
        // Set properties
        CLzmaEncProps props;
        SZ_Lzma_v25_01_EncProps_Init(&props);
        props.level = 5;
        props.dictSize = (UInt32)BLOCK_SIZE;
        SZ_Lzma_v25_01_EncProps_Normalize(&props);
        
        int setPropsResult = SZ_Lzma_v25_01_Enc_SetProps(enc, &props);
        if (setPropsResult != 0) {
            fprintf(stderr, "Thread %d: Failed to set encoder props: %d (iter %d)\n", tid, setPropsResult, iter);
            SZ_Lzma_v25_01_Enc_Destroy(enc);
            free(input); 
            free(output);
            t_arg->fail = 1;
            return NULL;
        }
        
        SZ_Lzma_v25_01_Enc_SetDataSize(enc, (uint64_t)BLOCK_SIZE);
        
        // Get properties
        uint8_t propBytes[16];
        size_t propSize = sizeof(propBytes);
        int writePropsResult = SZ_Lzma_v25_01_Enc_WriteProperties(enc, propBytes, &propSize);
        if (writePropsResult != 0) {
            fprintf(stderr, "Thread %d: Failed to write properties: %d (iter %d)\n", tid, writePropsResult, iter);
            SZ_Lzma_v25_01_Enc_Destroy(enc);
            free(input); 
            free(output);
            t_arg->fail = 1;
            return NULL;
        }
        
        // Compress
        size_t output_size = max_output;
        int res = SZ_Lzma_v25_01_Enc_MemEncode(
            enc, output, &output_size, input, BLOCK_SIZE, 0, NULL
        );
        
        // Destroy encoder immediately after use (like C# pattern)
        SZ_Lzma_v25_01_Enc_Destroy(enc);
        
        if (res != 0) {
            fprintf(stderr, "Thread %d: Compression failed: %d (iter %d)\n", tid, res, iter);
            free(input); 
            free(output);
            t_arg->fail = 1;
            return NULL;
        }
        
        // Verify decompression
        uint8_t* decoded = malloc(BLOCK_SIZE);
        if (!decoded) {
            fprintf(stderr, "Thread %d: Failed to allocate decode buffer\n", tid);
            free(input); 
            free(output);
            t_arg->fail = 1;
            return NULL;
        }
        
        size_t decoded_size = BLOCK_SIZE;
        size_t srcLen = output_size;
        int status = 0;
        int dec_res = SZ_Lzma_v25_01_Dec_LzmaDecode(
            decoded, &decoded_size,
            output, &srcLen,
            propBytes, (unsigned)propSize,
            1, // finish mode
            &status
        );
        
        if (dec_res != 0) {
            fprintf(stderr, "Thread %d: Decompression failed: %d (iter %d)\n", tid, dec_res, iter);
            free(input); 
            free(output); 
            free(decoded);
            t_arg->fail = 1;
            return NULL;
        }
        
        if (decoded_size != BLOCK_SIZE || memcmp(decoded, input, BLOCK_SIZE) != 0) {
            fprintf(stderr, "Thread %d: Data mismatch after decode! (iter %d)\n", tid, iter);
            free(input); 
            free(output); 
            free(decoded);
            t_arg->fail = 1;
            return NULL;
        }
        
        free(input); 
        free(output); 
        free(decoded);
        
        // Add small random delay to increase chance of race conditions
        if (iter % 10 == 0) {
            usleep(1000); // 1ms
        }
        
        if (iter % 10 == 0) {
            printf("Thread %d: completed %d iterations\n", tid, iter + 1);
        }
    }
    
    t_arg->completed = ITERATIONS_PER_THREAD;
    printf("Thread %d completed successfully\n", tid);
    return NULL;
}

int main(void) {
    printf("Starting LZMA stress test with %d threads, %d iterations each\n", NUM_THREADS, ITERATIONS_PER_THREAD);
    
    pthread_t threads[NUM_THREADS];
    struct thread_arg args[NUM_THREADS];
    
    // Initialize thread arguments
    for (int i = 0; i < NUM_THREADS; ++i) {
        args[i].tid = i;
        args[i].fail = 0;
        args[i].completed = 0;
    }
    
    // Create threads
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        if (pthread_create(&threads[i], NULL, stress_thread_func, &args[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return 1;
        }
    }
    
    // Wait for threads to complete
    int total_failures = 0;
    int total_completed = 0;
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        if (args[i].fail) {
            total_failures++;
            printf("Thread %d FAILED after %d iterations\n", i, args[i].completed);
        } else {
            total_completed += args[i].completed;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed = (end_time.tv_sec - start_time.tv_sec) + 
                    (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    
    printf("\n=== STRESS TEST RESULTS ===\n");
    printf("Total threads: %d\n", NUM_THREADS);
    printf("Target iterations per thread: %d\n", ITERATIONS_PER_THREAD);
    printf("Total successful iterations: %d\n", total_completed);
    printf("Failed threads: %d\n", total_failures);
    printf("Elapsed time: %.2f seconds\n", elapsed);
    printf("Iterations per second: %.2f\n", total_completed / elapsed);
    
    if (total_failures > 0) {
        fprintf(stderr, "STRESS TEST FAILED: %d threads failed\n", total_failures);
        return 1;
    }
    
    printf("STRESS TEST PASSED: All threads completed successfully\n");
    return 0;
}
