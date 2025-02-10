#include <pal_brotli_v1_1_0.h>

BrotliDecoderState* DN9_BRT_v1_1_0_BrotliDecoderCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque) {
    return BrotliDecoderCreateInstance(alloc_func, free_func, opaque);
}

BrotliDecoderResult DN9_BRT_v1_1_0_BrotliDecoderDecompress(
    size_t encoded_size, const uint8_t* encoded_buffer,
    size_t* decoded_size, uint8_t* decoded_buffer) {
    return BrotliDecoderDecompress(encoded_size, encoded_buffer, decoded_size, decoded_buffer);
}

BrotliDecoderResult DN9_BRT_v1_1_0_BrotliDecoderDecompressStream(
    BrotliDecoderState* state, size_t* available_in, const uint8_t** next_in,
    size_t* available_out, uint8_t** next_out, size_t* total_out) {
    return BrotliDecoderDecompressStream(state, available_in, next_in, available_out, next_out, total_out);
}

void DN9_BRT_v1_1_0_BrotliDecoderDestroyInstance(BrotliDecoderState* state) {
    BrotliDecoderDestroyInstance(state);
}

BROTLI_BOOL DN9_BRT_v1_1_0_BrotliDecoderIsFinished(const BrotliDecoderState* state) {
    return BrotliDecoderIsFinished(state);
}

BROTLI_BOOL DN9_BRT_v1_1_0_BrotliEncoderCompress(
    int quality, int lgwin, BrotliEncoderMode mode, size_t input_size, const uint8_t* input_buffer,
    size_t* encoded_size, uint8_t* encoded_buffer) {
    return BrotliEncoderCompress(quality, lgwin, mode, input_size, input_buffer, encoded_size, encoded_buffer);
}

BROTLI_BOOL DN9_BRT_v1_1_0_BrotliEncoderCompressStream(
    BrotliEncoderState* state, BrotliEncoderOperation op,
    size_t* available_in, const uint8_t** next_in,
    size_t* available_out, uint8_t** next_out, size_t* total_out) {
    return BrotliEncoderCompressStream(state, op, available_in, next_in, available_out, next_out, total_out);
}

BrotliEncoderState* DN9_BRT_v1_1_0_BrotliEncoderCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque) {
    return BrotliEncoderCreateInstance(alloc_func, free_func, opaque);
}

void DN9_BRT_v1_1_0_BrotliEncoderDestroyInstance(BrotliEncoderState* state) {
    BrotliEncoderDestroyInstance(state);
}

BROTLI_BOOL DN9_BRT_v1_1_0_BrotliEncoderHasMoreOutput(BrotliEncoderState* state) {
    return BrotliEncoderHasMoreOutput(state);
}

size_t DN9_BRT_v1_1_0_BrotliEncoderMaxCompressedSize(size_t input_size) {
    return BrotliEncoderMaxCompressedSize(input_size);
}

BROTLI_BOOL DN9_BRT_v1_1_0_BrotliEncoderSetParameter(
    BrotliEncoderState* state, BrotliEncoderParameter param, uint32_t value) {
    return BrotliEncoderSetParameter(state, param, value);
}
