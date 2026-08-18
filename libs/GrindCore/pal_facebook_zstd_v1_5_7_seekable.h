#ifndef PAL_FACEBOOK_ZSTD_V1_5_7_SEEKABLE_H
#define PAL_FACEBOOK_ZSTD_V1_5_7_SEEKABLE_H

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
#include "external/facebook/zstd_v1_5_7/seekable/zstd_seekable.h"
#include <stddef.h>

/* Seekable Compression Context Management */
typedef struct {
    ZSTD_seekable_CStream* zcs;
} SZ_ZStd_v1_5_7_SeekableCStream;

/* Seekable Decompression Context Management */
typedef struct {
    ZSTD_seekable* zs;
} SZ_ZStd_v1_5_7_Seekable;

/* Function Prototypes */
#ifdef __cplusplus
extern "C" {
#endif

/*===== Seekable Compression Functions =====*/

/* Context Creation */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_CreateCStream(SZ_ZStd_v1_5_7_SeekableCStream* ctx);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_FreeCStream(SZ_ZStd_v1_5_7_SeekableCStream* ctx);

/* Initialization */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_InitCStream(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    int32_t compressionLevel,
    int32_t checksumFlag,
    uint32_t maxFrameSize);

/* Streaming Compression */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_CompressStream(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcCapacity,
    int64_t* inSize,
    int64_t* outSize);

/* Frame Control */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_EndFrame(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    int64_t* outSize);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_EndStream(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    int64_t* outSize);

/*===== Seekable Decompression Functions =====*/

/* Context Creation */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_Create(SZ_ZStd_v1_5_7_Seekable* ctx);
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_Free(SZ_ZStd_v1_5_7_Seekable* ctx);

/* Initialization - In-Memory */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_InitBuff(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    const void* src,
    size_t srcSize);

/* Initialization - Streaming (Custom I/O) */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_InitAdvanced(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    void* opaque,
    ZSTD_seekable_read* readFn,
    ZSTD_seekable_seek* seekFn);

/* Decompression */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_Decompress(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    void* dst,
    size_t dstSize,
    uint64_t offset);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_DecompressFrame(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    void* dst,
    size_t dstSize,
    uint32_t frameIndex);

/* Seek Table Inspection */
FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetNumFrames(
    const SZ_ZStd_v1_5_7_Seekable* ctx);

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameCompressedOffset(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex);

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameDecompressedOffset(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameCompressedSize(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameDecompressedSize(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex);

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetDecompressedSize(
    const SZ_ZStd_v1_5_7_Seekable* ctx);

FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_OffsetToFrameIndex(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint64_t offset);

#ifdef __cplusplus
}
#endif

#endif /* PAL_FACEBOOK_ZSTD_V1_5_7_SEEKABLE_H */
