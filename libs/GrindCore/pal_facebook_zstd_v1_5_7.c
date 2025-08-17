#include "pal_facebook_zstd_v1_5_7.h"
#include <stdlib.h>
#include <stdint.h>

//
// ===== Compression Context Management =====
//
int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateCompressionContext(SZ_ZStd_v1_5_7_CompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->cctx = ZSTD_createCCtx();
    return ctx->cctx ? 0 : -1;
}

int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateDecompressionContext(SZ_ZStd_v1_5_7_DecompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->dctx = ZSTD_createDCtx();
    return ctx->dctx ? 0 : -1;
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeCompressionContext(SZ_ZStd_v1_5_7_CompressionContext* ctx) {
    if (ctx && ctx->cctx) {
    ZSTD_freeCCtx(ctx->cctx);
        ctx->cctx = NULL;
    }
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeDecompressionContext(SZ_ZStd_v1_5_7_DecompressionContext* ctx) {
    if (ctx && ctx->dctx) {
    ZSTD_freeDCtx(ctx->dctx);
        ctx->dctx = NULL;
    }
}

//
// ===== Dictionary Handling =====
//
int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateCompressionDict(SZ_ZStd_v1_5_7_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->cdict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
    return dict->cdict ? 0 : -1;
}

int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CreateDecompressionDict(SZ_ZStd_v1_5_7_DecompressionDict* dict, const void* dictBuffer, size_t dictSize) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->ddict = ZSTD_createDDict(dictBuffer, dictSize);
    return dict->ddict ? 0 : -1;
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeCompressionDict(SZ_ZStd_v1_5_7_CompressionDict* dict) {
    if (dict && dict->cdict) {
    ZSTD_freeCDict(dict->cdict);
        dict->cdict = NULL;
    }
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FreeDecompressionDict(SZ_ZStd_v1_5_7_DecompressionDict* dict) {
    if (dict && dict->ddict) {
    ZSTD_freeDDict(dict->ddict);
        dict->ddict = NULL;
    }
}

//
// ===== Block Compression & Decompression =====
//
size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressBlock(SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel) {
    if (!ctx || !ctx->cctx || !dst || !src) return 0;
    return ZSTD_compressCCtx(ctx->cctx, dst, dstCapacity, src, srcSize, compressionLevel);
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressBlock(SZ_ZStd_v1_5_7_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
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

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CompressStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_continue);
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_FlushStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_flush);
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_EndStream(
    SZ_ZStd_v1_5_7_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream_152(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_end);
}

//
// ===== Streaming Decompression =====
//
size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_DecompressStream(SZ_ZStd_v1_5_7_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize) {
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
int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetCompressionLevel(SZ_ZStd_v1_5_7_CompressionContext* ctx, int32_t level) {
    if (!ctx || !ctx->cctx || level < 0 || level > 22) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_compressionLevel, level);
}

int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_SetBlockSize(SZ_ZStd_v1_5_7_CompressionContext* ctx, size_t blockSize) {
    // Not supported in zstd 1.5.2
    (void)ctx;
    (void)blockSize;
    return -1;
}

//
// ===== Recommended Buffer Sizes =====
//
size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CStreamInSize(void) {
    return ZSTD_CStreamInSize();
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_7_CStreamOutSize(void) {
    return ZSTD_CStreamOutSize();
}