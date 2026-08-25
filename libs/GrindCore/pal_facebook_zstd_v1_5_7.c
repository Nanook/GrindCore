#include "pal_facebook_zstd_v1_5_7.h"
#include <stdlib.h>
#include <stdint.h>

//
// ===== Compression Context Management =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateCompressionContext(SZ_ZStd_v1_5_7_CompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->cctx = ZSTD_createCCtx();
    return ctx->cctx ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateDecompressionContext(SZ_ZStd_v1_5_7_DecompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->dctx = ZSTD_createDCtx();
    return ctx->dctx ? 0 : -1;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeCompressionContext(SZ_ZStd_v1_5_7_CompressionContext* ctx) {
    if (ctx && ctx->cctx) {
    ZSTD_freeCCtx(ctx->cctx);
        ctx->cctx = NULL;
    }
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeDecompressionContext(SZ_ZStd_v1_5_7_DecompressionContext* ctx) {
    if (ctx && ctx->dctx) {
    ZSTD_freeDCtx(ctx->dctx);
        ctx->dctx = NULL;
    }
}

//
// ===== Dictionary Handling =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateCompressionDict(SZ_ZStd_v1_5_7_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel, int32_t windowLog) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    if (windowLog > 0) {
        /* Advanced path: override windowLog explicitly. ZSTD_createCDict()'s implicit sizing (used below)
         * caps the window at 8MB for any dictionary >256KB at level 19 (its size-tiered default table
         * never grows the window beyond the level's own default) - this lets a caller with a larger
         * dictionary force full coverage instead of silently losing match material beyond 8MB. */
        ZSTD_CCtx_params* params = ZSTD_createCCtxParams();
        if (!params) return -1;
        ZSTD_CCtxParams_init(params, compressionLevel);
        ZSTD_CCtxParams_setParameter(params, ZSTD_c_windowLog, windowLog);
        dict->cdict = ZSTD_createCDict_advanced2(dictBuffer, dictSize, ZSTD_dlm_byCopy, ZSTD_dct_auto, params, ZSTD_defaultCMem);
        ZSTD_freeCCtxParams(params);
    } else {
        dict->cdict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
    }
    return dict->cdict ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateDecompressionDict(SZ_ZStd_v1_5_7_DecompressionDict* dict, const void* dictBuffer, size_t dictSize) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->ddict = ZSTD_createDDict(dictBuffer, dictSize);
    return dict->ddict ? 0 : -1;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeCompressionDict(SZ_ZStd_v1_5_7_CompressionDict* dict) {
    if (dict && dict->cdict) {
    ZSTD_freeCDict(dict->cdict);
        dict->cdict = NULL;
    }
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeDecompressionDict(SZ_ZStd_v1_5_7_DecompressionDict* dict) {
    if (dict && dict->ddict) {
    ZSTD_freeDDict(dict->ddict);
        dict->ddict = NULL;
    }
}

//
// ===== Block Compression & Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressBlock(SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel) {
    if (!ctx || !ctx->cctx || !dst || !src) return 0;
    return ZSTD_compressCCtx(ctx->cctx, dst, dstCapacity, src, srcSize, compressionLevel);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressBlock(SZ_ZStd_v1_5_7_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->dctx || !dst || !src) return 0;
    return ZSTD_decompressDCtx(ctx->dctx, dst, dstCapacity, src, srcSize);
}

//
// ===== Streaming Compression =====
//
static size_t compressStream_152(
    SZ_ZStd_v1_5_7_CompressionContext* ctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity,
    int64_t* inSize, int64_t* outSize,
    ZSTD_EndDirective endOp)
{
    if (!ctx || !ctx->cctx || !dst || !src || !outSize) return -1;
    if (endOp == ZSTD_e_continue && !inSize) return -1;

    ZSTD_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_inBuffer input = { src, srcCapacity, 0 };

    size_t toFlush = ZSTD_compressStream2(ctx->cctx, &output, &input, endOp);

    *inSize = input.pos;
    *outSize = output.pos;

    return toFlush; // Bytes left in buffer
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_continue);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FlushStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_flush);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_EndStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_end);
}

//
// ===== Streaming Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressStream(SZ_ZStd_v1_5_7_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize) {
    if (!ctx || !ctx->dctx || !dst || !src) return 0;

    ZSTD_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_inBuffer input = { src, srcCapacity, 0 };

    size_t toFlush =  ZSTD_decompressStream(ctx->dctx, &output, &input);

    // Return processed sizes
    *inSize = input.pos;
    *outSize = output.pos;
    return toFlush;
}

//
// ===== Configuration =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetCompressionLevel(SZ_ZStd_v1_5_7_CompressionContext* ctx, int32_t level) {
    if (!ctx || !ctx->cctx || level < 0 || level > 22) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_compressionLevel, level);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetBlockSize(SZ_ZStd_v1_5_7_CompressionContext* ctx, size_t blockSize) {
    if (!ctx || !ctx->cctx) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_targetCBlockSize, (int)blockSize);
}

//
// ===== Multithreading Configuration =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetNbWorkers(SZ_ZStd_v1_5_7_CompressionContext* ctx, int32_t nbWorkers) {
    if (!ctx || !ctx->cctx || nbWorkers < 0) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_nbWorkers, nbWorkers);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetJobSize(SZ_ZStd_v1_5_7_CompressionContext* ctx, size_t jobSize) {
    if (!ctx || !ctx->cctx) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_jobSize, (int)jobSize);
}

//
// ===== Dictionary Compression & Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressBlockWithDict(SZ_ZStd_v1_5_7_CompressionContext* ctx, SZ_ZStd_v1_5_7_CompressionDict* dict, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->cctx || !dict || !dict->cdict || !dst || !src) return 0;
    return ZSTD_compress_usingCDict(ctx->cctx, dst, dstCapacity, src, srcSize, dict->cdict);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressBlockWithDict(SZ_ZStd_v1_5_7_DecompressionContext* ctx, SZ_ZStd_v1_5_7_DecompressionDict* dict, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->dctx || !dict || !dict->ddict || !dst || !src) return 0;
    return ZSTD_decompress_usingDDict(ctx->dctx, dst, dstCapacity, src, srcSize, dict->ddict);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetCompressionDict(SZ_ZStd_v1_5_7_CompressionContext* ctx, SZ_ZStd_v1_5_7_CompressionDict* dict) {
    if (!ctx || !ctx->cctx) return -1;
    // Pass NULL cdict to clear the dictionary
    size_t result = ZSTD_CCtx_refCDict(ctx->cctx, dict ? dict->cdict : NULL);
    return ZSTD_isError(result) ? -1 : 0;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetDecompressionDict(SZ_ZStd_v1_5_7_DecompressionContext* ctx, SZ_ZStd_v1_5_7_DecompressionDict* dict) {
    if (!ctx || !ctx->dctx) return -1;
    // Pass NULL ddict to clear the dictionary
    size_t result = ZSTD_DCtx_refDDict(ctx->dctx, dict ? dict->ddict : NULL);
    return ZSTD_isError(result) ? -1 : 0;
}

//
// ===== Error Handling =====
//
FUNCTIONEXPORT unsigned FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_IsError(size_t result) {
    return ZSTD_isError(result);
}

FUNCTIONEXPORT const char* FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_GetErrorName(size_t result) {
    return ZSTD_getErrorName(result);
}

//
// ===== Recommended Buffer Sizes =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CStreamInSize(void) {
    return ZSTD_CStreamInSize();
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CStreamOutSize(void) {
    return ZSTD_CStreamOutSize();
}

//
// ===== Skippable Frame Functions =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_WriteSkippableFrame(
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcSize,
    uint32_t magicVariant)
{
    return ZSTD_writeSkippableFrame(dst, dstCapacity, src, srcSize, magicVariant);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_ReadSkippableFrame(
    void* dst,
    size_t dstCapacity,
    uint32_t* magicVariant,
    const void* src,
    size_t srcSize)
{
    return ZSTD_readSkippableFrame(dst, dstCapacity, magicVariant, src, srcSize);
}

FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_IsSkippableFrame(
    const void* buffer,
    size_t size)
{
    return ZSTD_isSkippableFrame(buffer, size);
}

