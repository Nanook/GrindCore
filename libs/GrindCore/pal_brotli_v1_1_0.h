#ifndef DN9_BRT_V1_1_0_H_
#define DN9_BRT_V1_1_0_H_

#ifdef _WIN32
    #include <stdint.h>
    #include <windows.h>
    #define FUNCTIONEXPORT
    #define FUNCTIONCALLINGCONVENCTION WINAPI
#else
    #include "pal_types.h"
    #include "pal_compiler.h"
    #define FUNCTIONEXPORT PALEXPORT
    #define FUNCTIONCALLINGCONVENCTION
#endif

#include <brotli/decode.h>
#include <brotli/encode.h>
#include <brotli/types.h>

#ifdef __cplusplus
extern "C" {
#endif

FUNCTIONEXPORT BrotliDecoderState* FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliDecoderCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

FUNCTIONEXPORT BrotliDecoderResult FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliDecoderDecompress(
    size_t encoded_size, const uint8_t* encoded_buffer,
    size_t* decoded_size, uint8_t* decoded_buffer);

FUNCTIONEXPORT BrotliDecoderResult FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliDecoderDecompressStream(
    BrotliDecoderState* state, size_t* available_in, const uint8_t** next_in,
    size_t* available_out, uint8_t** next_out, size_t* total_out);

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliDecoderDestroyInstance(BrotliDecoderState* state);

FUNCTIONEXPORT BROTLI_BOOL FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliDecoderIsFinished(const BrotliDecoderState* state);

FUNCTIONEXPORT BROTLI_BOOL FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderCompress(
    int quality, int lgwin, BrotliEncoderMode mode, size_t input_size, const uint8_t* input_buffer,
    size_t* encoded_size, uint8_t* encoded_buffer);

FUNCTIONEXPORT BROTLI_BOOL FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderCompressStream(
    BrotliEncoderState* state, BrotliEncoderOperation op,
    size_t* available_in, const uint8_t** next_in,
    size_t* available_out, uint8_t** next_out, size_t* total_out);

FUNCTIONEXPORT BrotliEncoderState* FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderCreateInstance(
    brotli_alloc_func alloc_func, brotli_free_func free_func, void* opaque);

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderDestroyInstance(BrotliEncoderState* state);

FUNCTIONEXPORT BROTLI_BOOL FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderHasMoreOutput(BrotliEncoderState* state);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderMaxCompressedSize(size_t input_size);

FUNCTIONEXPORT BROTLI_BOOL FUNCTIONCALLINGCONVENCTION DN9_BRT_v1_1_0_BrotliEncoderSetParameter(
    BrotliEncoderState* state, BrotliEncoderParameter param, uint32_t value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // DN9_BRT_V1_1_0_H_
