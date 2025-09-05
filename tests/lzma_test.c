#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "lzma_test_minimal.h"

#define BLOCK_SIZE (128 * 1024)
#define NUM_THREADS 16

struct thread_arg {
    int tid;
    int fail;
};

void* thread_func(void* arg) {
    struct thread_arg* t_arg = (struct thread_arg*)arg;
    int tid = t_arg->tid;
    for (int iter = 0; iter < 1000; ++iter) {
        uint8_t* input = malloc(BLOCK_SIZE);
        if (!input) {
            fprintf(stderr, "Thread %d: Failed to allocate input buffer\n", tid);
            t_arg->fail = 1;
            return NULL;
        }
        for (size_t i = 0; i < BLOCK_SIZE; ++i)
            input[i] = (uint8_t)(((i + tid + iter) % 251));

        size_t max_output = BLOCK_SIZE + (BLOCK_SIZE / 2) + 0x10;
        uint8_t* output = malloc(max_output);
        if (!output) {
            fprintf(stderr, "Thread %d: Failed to allocate output buffer\n", tid);
            free(input);
            t_arg->fail = 1;
            return NULL;
        }
        size_t output_size = max_output;

        CLzmaEncProps props;
        SZ_Lzma_v25_01_EncProps_Init(&props);
        props.level = 5;
        props.dictSize = (UInt32)BLOCK_SIZE;
        SZ_Lzma_v25_01_EncProps_Normalize(&props);

        void* enc = SZ_Lzma_v25_01_Enc_Create();
        if (!enc) {
            fprintf(stderr, "Thread %d: Failed to create encoder handle\n", tid);
            free(input); free(output);
            t_arg->fail = 1;
            return NULL;
        }
        SZ_Lzma_v25_01_Enc_SetProps(enc, &props);
        SZ_Lzma_v25_01_Enc_SetDataSize(enc, (uint64_t)BLOCK_SIZE);

        uint8_t propBytes[16];
        size_t propSize = sizeof(propBytes);
        SZ_Lzma_v25_01_Enc_WriteProperties(enc, propBytes, &propSize);

        int res = SZ_Lzma_v25_01_Enc_MemEncode(
            enc, output, &output_size, input, BLOCK_SIZE, 0, NULL
        );
        SZ_Lzma_v25_01_Enc_Destroy(enc);

        if (res != 0) {
            fprintf(stderr, "Thread %d: Compression failed: %d (iter %d)\n", tid, res, iter);
            free(input); free(output);
            t_arg->fail = 1;
            return NULL;
        }

        uint8_t* decoded = malloc(BLOCK_SIZE);
        if (!decoded) {
            fprintf(stderr, "Thread %d: Failed to allocate decode buffer\n", tid);
            free(input); free(output);
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
            free(input); free(output); free(decoded);
            t_arg->fail = 1;
            return NULL;
        }

        if (decoded_size != BLOCK_SIZE || memcmp(decoded, input, BLOCK_SIZE) != 0) {
            fprintf(stderr, "Thread %d: Data mismatch after decode! (iter %d)\n", tid, iter);
            free(input); free(output); free(decoded);
            t_arg->fail = 1;
            return NULL;
        }

        free(input); free(output); free(decoded);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    struct thread_arg args[NUM_THREADS];
    int fail = 0;
    for (int i = 0; i < NUM_THREADS; ++i) {
        args[i].tid = i;
        args[i].fail = 0;
        if (pthread_create(&threads[i], NULL, thread_func, &args[i]) != 0) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            fail = 1;
        }
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        if (args[i].fail) fail = 1;
    }
    if (fail) {
        fprintf(stderr, "Parallel block encode/decode failed.\n");
        return 1;
    }
    printf("Parallel block encode/decode succeeded.\n");
    return 0;
}
