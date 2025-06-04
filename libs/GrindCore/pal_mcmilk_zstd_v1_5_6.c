#include "pal_mcmilk_zstd_v1_5_6.h"
#include <stdlib.h>
#include <stdint.h>

/* ===== Compression Context Management ===== */
int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CreateCompressionContext(SZ_ZStd_v1_5_6_CompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->cctx = ZSTD_createCCtx();
    return ctx->cctx ? 0 : -1;
}

int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CreateDecompressionContext(SZ_ZStd_v1_5_6_DecompressionContext* ctx) {
    if (!ctx) return -1;
    ctx->dctx = ZSTD_createDCtx();
    return ctx->dctx ? 0 : -1;
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_FreeCompressionContext(SZ_ZStd_v1_5_6_CompressionContext* ctx) {
    if (ctx && ctx->cctx) {
        ZSTD_freeCCtx(ctx->cctx);
        ctx->cctx = NULL;
    }
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_FreeDecompressionContext(SZ_ZStd_v1_5_6_DecompressionContext* ctx) {
    if (ctx && ctx->dctx) {
        ZSTD_freeDCtx(ctx->dctx);
        ctx->dctx = NULL;
    }
}

/* ===== Dictionary Handling ===== */
int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CreateCompressionDict(SZ_ZStd_v1_5_6_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->cdict = ZSTD_createCDict(dictBuffer, dictSize, compressionLevel);
    return dict->cdict ? 0 : -1;
}

int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CreateDecompressionDict(SZ_ZStd_v1_5_6_DecompressionDict* dict, const void* dictBuffer, size_t dictSize) {
    if (!dict || !dictBuffer || dictSize == 0) return -1;
    dict->ddict = ZSTD_createDDict(dictBuffer, dictSize);
    return dict->ddict ? 0 : -1;
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_FreeCompressionDict(SZ_ZStd_v1_5_6_CompressionDict* dict) {
    if (dict && dict->cdict) {
        ZSTD_freeCDict(dict->cdict);
        dict->cdict = NULL;
    }
}

void FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_FreeDecompressionDict(SZ_ZStd_v1_5_6_DecompressionDict* dict) {
    if (dict && dict->ddict) {
        ZSTD_freeDDict(dict->ddict);
        dict->ddict = NULL;
    }
}

/* ===== Block Compression & Decompression ===== */
size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CompressBlock(SZ_ZStd_v1_5_6_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel) {
    if (!ctx || !ctx->cctx || !dst || !src) return 0;
    return ZSTD_compressCCtx(ctx->cctx, dst, dstCapacity, src, srcSize, compressionLevel);
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_DecompressBlock(SZ_ZStd_v1_5_6_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->dctx || !dst || !src) return 0;
    return ZSTD_decompressDCtx(ctx->dctx, dst, dstCapacity, src, srcSize);
}

/* ===== Streaming Compression ===== */
size_t compressStream(
    SZ_ZStd_v1_5_6_CompressionContext* ctx,
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

    // Return processed sizes
    *inSize = input.pos;
    *outSize = output.pos;

    return toFlush; // Bytes left in buffer
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CompressStream(
    SZ_ZStd_v1_5_6_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_continue);
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_FlushStream(
    SZ_ZStd_v1_5_6_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_flush);
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_EndStream(
    SZ_ZStd_v1_5_6_CompressionContext* ctx, void* dst, size_t dstCapacity,
    const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize)
{
    return compressStream(ctx, dst, dstCapacity, src, srcCapacity, inSize, outSize, ZSTD_e_end);
}

/* ===== Streaming Decompression ===== */
size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_DecompressStream(SZ_ZStd_v1_5_6_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcCapacity, int64_t* inSize, int64_t* outSize) {
    if (!ctx || !ctx->dctx || !dst || !src) return 0;

    ZSTD_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_inBuffer input = { src, srcCapacity, 0 };

    size_t toFlush =  ZSTD_decompressStream(ctx->dctx, &output, &input);

    // Return processed sizes
    *inSize = input.pos;
    *outSize = output.pos;
    return toFlush;
}

/* ===== Configuration ===== */
int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_SetCompressionLevel(SZ_ZStd_v1_5_6_CompressionContext* ctx, int32_t level) {
    if (!ctx || !ctx->cctx || level < 0 || level > 22) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_compressionLevel, level);
}

int32_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_SetBlockSize(SZ_ZStd_v1_5_6_CompressionContext* ctx, size_t blockSize) {
    if (!ctx || !ctx->cctx || blockSize == 0) return -1;
    return (int32_t)ZSTD_CCtx_setParameter(ctx->cctx, ZSTD_c_targetCBlockSize, (int32_t)blockSize);
}

/* ===== Recommended Buffer Sizes ===== */
size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CStreamInSize(void) {
    return ZSTD_CStreamInSize();
}

size_t FUNCTIONEXPORT FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_6_CStreamOutSize(void) {
    return ZSTD_CStreamOutSize();
}