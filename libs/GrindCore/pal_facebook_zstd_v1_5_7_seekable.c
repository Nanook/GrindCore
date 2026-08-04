#include "pal_facebook_zstd_v1_5_7_seekable.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//
// ===== Seekable Compression Context Management =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_CreateCStream(SZ_ZStd_v1_5_7_SeekableCStream* ctx) {
    if (!ctx) return -1;
    ctx->zcs = ZSTD_seekable_createCStream();
    return ctx->zcs ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_FreeCStream(SZ_ZStd_v1_5_7_SeekableCStream* ctx) {
    if (ctx && ctx->zcs) {
        ZSTD_seekable_freeCStream(ctx->zcs);
        ctx->zcs = NULL;
        return 0;
    }
    return -1;
}

//
// ===== Seekable Compression Initialization =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_InitCStream(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    int32_t compressionLevel,
    int32_t checksumFlag,
    uint32_t maxFrameSize)
{
    if (!ctx || !ctx->zcs) return (size_t)-1;
    return ZSTD_seekable_initCStream(ctx->zcs, compressionLevel, checksumFlag, maxFrameSize);
}

//
// ===== Seekable Streaming Compression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_CompressStream(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcCapacity,
    int64_t* inSize,
    int64_t* outSize)
{
    if (!ctx || !ctx->zcs || !dst || !src || !inSize || !outSize) return (size_t)-1;

    ZSTD_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_inBuffer input = { src, srcCapacity, 0 };

    size_t ret = ZSTD_seekable_compressStream(ctx->zcs, &output, &input);

    *inSize = (int64_t)input.pos;
    *outSize = (int64_t)output.pos;

    return ret;
}

//
// ===== Seekable Frame Control =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_EndFrame(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    int64_t* outSize)
{
    if (!ctx || !ctx->zcs || !dst || !outSize) return (size_t)-1;

    ZSTD_outBuffer output = { dst, dstCapacity, 0 };

    size_t ret = ZSTD_seekable_endFrame(ctx->zcs, &output);

    *outSize = (int64_t)output.pos;

    return ret;
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_EndStream(
    SZ_ZStd_v1_5_7_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    int64_t* outSize)
{
    if (!ctx || !ctx->zcs || !dst || !outSize) return (size_t)-1;

    ZSTD_outBuffer output = { dst, dstCapacity, 0 };

    size_t ret = ZSTD_seekable_endStream(ctx->zcs, &output);

    *outSize = (int64_t)output.pos;

    return ret;
}

//
// ===== Seekable Decompression Context Management =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_Create(SZ_ZStd_v1_5_7_Seekable* ctx) {
    if (!ctx) return -1;
    ctx->zs = ZSTD_seekable_create();
    return ctx->zs ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_Free(SZ_ZStd_v1_5_7_Seekable* ctx) {
    if (ctx && ctx->zs) {
        ZSTD_seekable_free(ctx->zs);
        ctx->zs = NULL;
        return 0;
    }
    return -1;
}

//
// ===== Seekable Decompression Initialization - In-Memory =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_InitBuff(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    const void* src,
    size_t srcSize)
{
    if (!ctx || !ctx->zs || !src) return (size_t)-1;
    return ZSTD_seekable_initBuff(ctx->zs, src, srcSize);
}

//
// ===== Seekable Decompression Initialization - Streaming =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_InitAdvanced(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    void* opaque,
    ZSTD_seekable_read* readFn,
    ZSTD_seekable_seek* seekFn)
{
    if (!ctx || !ctx->zs || !readFn || !seekFn) return (size_t)-1;

    ZSTD_seekable_customFile customFile;
    customFile.opaque = opaque;
    customFile.read = readFn;
    customFile.seek = seekFn;

    return ZSTD_seekable_initAdvanced(ctx->zs, customFile);
}

//
// ===== Seekable Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_Decompress(
    SZ_ZStd_v1_5_7_Seekable* ctx,
    void* dst,
    size_t dstSize,
    uint64_t offset)
{
    if (!ctx || !ctx->zs || !dst) return (size_t)-1;
    return ZSTD_seekable_decompress(ctx->zs, dst, dstSize, (unsigned long long)offset);
}

//
// ===== Seekable Seek Table Inspection =====
//
FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetNumFrames(
    const SZ_ZStd_v1_5_7_Seekable* ctx)
{
    if (!ctx || !ctx->zs) return 0;
    return ZSTD_seekable_getNumFrames(ctx->zs);
}

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameCompressedOffset(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (uint64_t)-1;
    return ZSTD_seekable_getFrameCompressedOffset(ctx->zs, frameIndex);
}

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameDecompressedOffset(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (uint64_t)-1;
    return ZSTD_seekable_getFrameDecompressedOffset(ctx->zs, frameIndex);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameCompressedSize(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (size_t)-1;
    return ZSTD_seekable_getFrameCompressedSize(ctx->zs, frameIndex);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetFrameDecompressedSize(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (size_t)-1;
    return ZSTD_seekable_getFrameDecompressedSize(ctx->zs, frameIndex);
}

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_GetDecompressedSize(
    const SZ_ZStd_v1_5_7_Seekable* ctx)
{
    if (!ctx || !ctx->zs) return 0;

    // Get total decompressed size by summing all frame sizes
    uint32_t numFrames = ZSTD_seekable_getNumFrames(ctx->zs);
    if (numFrames == 0) return 0;

    // The last frame's decompressed offset plus its size gives us the total
    uint64_t lastFrameOffset = ZSTD_seekable_getFrameDecompressedOffset(ctx->zs, numFrames - 1);
    size_t lastFrameSize = ZSTD_seekable_getFrameDecompressedSize(ctx->zs, numFrames - 1);

    return lastFrameOffset + lastFrameSize;
}

FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_Seekable_OffsetToFrameIndex(
    const SZ_ZStd_v1_5_7_Seekable* ctx,
    uint64_t offset)
{
    if (!ctx || !ctx->zs) return (uint32_t)-1;
    return ZSTD_seekable_offsetToFrameIndex(ctx->zs, (unsigned long long)offset);
}
