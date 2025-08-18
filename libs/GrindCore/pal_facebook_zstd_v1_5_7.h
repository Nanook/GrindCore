#ifndef PAL_MCMILK_ZSTD_V1_5_7_H
#define PAL_MCMILK_ZSTD_V1_5_7_H

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
#define ZSTD_NAMESPACE ZSTD_v1_5_7
#include "external/facebook/zstd_v1_5_7/zstd.h"
#include <stddef.h>

/* Compression Context Management */
typedef struct {
    ZSTD_CCtx* cctx;
} SZ_ZStd_v1_5_7_CompressionContext;

typedef struct {
    ZSTD_DCtx* dctx;
} SZ_ZStd_v1_5_7_DecompressionContext;

/* Dictionary Management */
typedef struct {
    ZSTD_CDict* cdict;
} SZ_ZStd_v1_5_7_CompressionDict;

typedef struct {
    ZSTD_DDict* ddict;
} SZ_ZStd_v1_5_7_DecompressionDict;

/* Function Prototypes */
#ifdef __cplusplus
extern "C" {
#endif

/* Context Creation */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateCompressionContext(SZ_ZStd_v1_5_7_CompressionContext* ctx);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateDecompressionContext(SZ_ZStd_v1_5_7_DecompressionContext* ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeCompressionContext(SZ_ZStd_v1_5_7_CompressionContext* ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeDecompressionContext(SZ_ZStd_v1_5_7_DecompressionContext* ctx);

/* Dictionary Handling */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateCompressionDict(SZ_ZStd_v1_5_7_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateDecompressionDict(SZ_ZStd_v1_5_7_DecompressionDict* dict, const void* dictBuffer, size_t dictSize);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeCompressionDict(SZ_ZStd_v1_5_7_CompressionDict* dict);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeDecompressionDict(SZ_ZStd_v1_5_7_DecompressionDict* dict);

/* Block Compression & Decompression */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressBlock(SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressBlock(SZ_ZStd_v1_5_7_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

/* Streaming Compression (Fixed) */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

/* Streaming Compression Helpers */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FlushStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_EndStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

/* Streaming Decompression */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressStream(
    SZ_ZStd_v1_5_7_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize);

/* Configuration */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetCompressionLevel(SZ_ZStd_v1_5_7_CompressionContext* ctx, int32_t level);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetBlockSize(SZ_ZStd_v1_5_7_CompressionContext* ctx, size_t blockSize);

/* Recommended Buffer Sizes */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CStreamInSize(void);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CStreamOutSize(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_FACEBOOK_ZSTD_V1_5_7_H */