#ifndef PAL_MCMILK_ZSTD_V1_5_2_H
#define PAL_MCMILK_ZSTD_V1_5_2_H

#ifdef _WIN32
    #include <stdint.h>
    #include <windows.h>
    #define FUNCTIONEXPORT __declspec(dllexport)
    #define FUNCTIONCALLINGCONVENCTION __cdecl
#else
    #include "pal_types.h"
    #include "pal_compiler.h"
    #define FUNCTIONEXPORT PALEXPORT
    #define FUNCTIONCALLINGCONVENCTION
#endif

#undef ZSTD_NAMESPACE
#define ZSTD_NAMESPACE ZSTD_v1_5_2
#include "external/facebook/zstd_v1_5_2/zstd.h"
#include <stddef.h>

/* Compression Context Management */
typedef struct {
    ZSTD_v1_5_2_CCtx* cctx;
} SZ_ZStd_v1_5_2_CompressionContext;

typedef struct {
    ZSTD_v1_5_2_DCtx* dctx;
} SZ_ZStd_v1_5_2_DecompressionContext;

/* Dictionary Management */
typedef struct {
    ZSTD_v1_5_2_CDict* cdict;
} SZ_ZStd_v1_5_2_CompressionDict;

typedef struct {
    ZSTD_v1_5_2_DDict* ddict;
} SZ_ZStd_v1_5_2_DecompressionDict;

/* Function Prototypes */
#ifdef __cplusplus
extern "C" {
#endif

/* Context Creation */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateCompressionContext(SZ_ZStd_v1_5_2_CompressionContext* ctx);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateDecompressionContext(SZ_ZStd_v1_5_2_DecompressionContext* ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeCompressionContext(SZ_ZStd_v1_5_2_CompressionContext* ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeDecompressionContext(SZ_ZStd_v1_5_2_DecompressionContext* ctx);

/* Dictionary Handling */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateCompressionDict(SZ_ZStd_v1_5_2_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateDecompressionDict(SZ_ZStd_v1_5_2_DecompressionDict* dict, const void* dictBuffer, size_t dictSize);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeCompressionDict(SZ_ZStd_v1_5_2_CompressionDict* dict);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeDecompressionDict(SZ_ZStd_v1_5_2_DecompressionDict* dict);

/* Block Compression & Decompression */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressBlock(SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_DecompressBlock(SZ_ZStd_v1_5_2_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

/* Streaming Compression (Fixed) */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressStream(
    SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

/* Streaming Compression Helpers */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FlushStream(
    SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_EndStream(
    SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

/* Streaming Decompression */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_DecompressStream(
    SZ_ZStd_v1_5_2_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

/* Configuration */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetCompressionLevel(SZ_ZStd_v1_5_2_CompressionContext* ctx, int32_t level);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetBlockSize(SZ_ZStd_v1_5_2_CompressionContext* ctx, size_t blockSize);

/* Recommended Buffer Sizes */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CStreamInSize(void);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CStreamOutSize(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_MCMILK_ZSTD_V1_5_6_H */