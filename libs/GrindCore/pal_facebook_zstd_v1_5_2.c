#include "pal_facebook_zstd_v1_5_2.h"
#include <stdlib.h>
#include <stdint.h>

//
// ===== Compression Context Management =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateCompressionContext(SZ_ZStd_v1_5_2_CompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->cctx = ZSTD_v1_5_2_createCCtx();
    return ctx->cctx ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateDecompressionContext(SZ_ZStd_v1_5_2_DecompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->dctx = ZSTD_v1_5_2_createDCtx();
    return ctx->dctx ? 0 : -1;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeCompressionContext(SZ_ZStd_v1_5_2_CompressionContext* ctx) {
    if (ctx && ctx->cctx) {
    ZSTD_v1_5_2_freeCCtx(ctx->cctx);
        ctx->cctx = NULL;
    }
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeDecompressionContext(SZ_ZStd_v1_5_2_DecompressionContext* ctx) {
    if (ctx && ctx->dctx) {
    ZSTD_v1_5_2_freeDCtx(ctx->dctx);
        ctx->dctx = NULL;
    }
}

//
// ===== Dictionary Handling =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateCompressionDict(SZ_ZStd_v1_5_2_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->cdict = ZSTD_v1_5_2_createCDict(dictBuffer, dictSize, compressionLevel);
    return dict->cdict ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateDecompressionDict(SZ_ZStd_v1_5_2_DecompressionDict* dict, const void* dictBuffer, size_t dictSize) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->ddict = ZSTD_v1_5_2_createDDict(dictBuffer, dictSize);
    return dict->ddict ? 0 : -1;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeCompressionDict(SZ_ZStd_v1_5_2_CompressionDict* dict) {
    if (dict && dict->cdict) {
    ZSTD_v1_5_2_freeCDict(dict->cdict);
        dict->cdict = NULL;
    }
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FreeDecompressionDict(SZ_ZStd_v1_5_2_DecompressionDict* dict) {
    if (dict && dict->ddict) {
    ZSTD_v1_5_2_freeDDict(dict->ddict);
        dict->ddict = NULL;
    }
}

//
// ===== Block Compression & Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressBlock(SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel) {
    if (!ctx || !ctx->cctx || !dst || !src) return 0;
    return ZSTD_v1_5_2_compressCCtx(ctx->cctx, dst, dstCapacity, src, srcSize, compressionLevel);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_DecompressBlock(SZ_ZStd_v1_5_2_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->dctx || !dst || !src) return 0;
    return ZSTD_v1_5_2_decompressDCtx(ctx->dctx, dst, dstCapacity, src, srcSize);
}

//
// ===== Streaming Compression =====
//
static size_t compressStream_152(
    SZ_ZStd_v1_5_2_CompressionContext* ctx,
    void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity,
    int64_t* inSize, int64_t* outSize,
    ZSTD_v1_5_2_EndDirective endOp)
{
    if (!ctx || !ctx->cctx || !dst || !src || !outSize) return -1;
    if (endOp == ZSTD_v1_5_2_e_continue && !inSize) return -1;

    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_v1_5_2_inBuffer input = { src, srcCapacity, 0 };

    size_t toFlush = ZSTD_v1_5_2_compressStream2(ctx->cctx, &output, &input, endOp);

    *inSize = input.pos;
    *outSize = output.pos;

    return toFlush; // Bytes left in buffer
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressStream(
    SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_v1_5_2_e_continue);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_FlushStream(
    SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_v1_5_2_e_flush);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_EndStream(
    SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_v1_5_2_e_end);
}

//
// ===== Streaming Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_DecompressStream(SZ_ZStd_v1_5_2_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize) {
    if (!ctx || !ctx->dctx || !dst || !src) return 0;

    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_v1_5_2_inBuffer input = { src, srcCapacity, 0 };

    size_t toFlush =  ZSTD_v1_5_2_decompressStream(ctx->dctx, &output, &input);

    // Return processed sizes
    *inSize = input.pos;
    *outSize = output.pos;
    return toFlush;
}

//
// ===== Configuration =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetCompressionLevel(SZ_ZStd_v1_5_2_CompressionContext* ctx, int32_t level) {
    if (!ctx || !ctx->cctx || level < 0 || level > 22) return -1;
    return (int32_t)ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_compressionLevel, level);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetBlockSize(SZ_ZStd_v1_5_2_CompressionContext* ctx, size_t blockSize) {
    // Not supported in zstd 1.5.2
    (void)ctx;
    (void)blockSize;
    return -1;
}

//
// ===== Recommended Buffer Sizes =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CStreamInSize(void) {
    return ZSTD_v1_5_2_CStreamInSize();
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CStreamOutSize(void) {
    return ZSTD_v1_5_2_CStreamOutSize();
}