#define ZSTD_STATIC_LINKING_ONLY
#define ZSTD_v1_5_2_STATIC_LINKING_ONLY
// For ZSTD_v1_5_2_compress_usingCDict_advanced (CompressBlockWithDictAdvanced): see the 1.5.7 PAL
#define ZSTD_v1_5_2_DISABLE_DEPRECATE_WARNINGS
#include "pal_facebook_zstd_v1_5_2.h"
// ZSTD_v1_5_2_getErrorCode. Only here, not in the header: entrypoints.c also includes 1.5.7's zstd.h, which brings
// 1.5.7's zstd_errors.h, and the two define ZSTDERRORLIB_API differently.
#include "external/facebook/zstd_v1_5_2/zstd_errors.h"
#include <stdlib.h>
#include <stdint.h>

// Argument checks for the size_t functions (audit/zstd.md 2.2). A bad argument returns SZ_ZSTD_ARG_ERROR, zstd's
// generic error, which IsError recognises. It never returns 0: that's a valid decoded size, and from DecompressStream
// it means the frame is complete. A NULL buffer is only bad with a nonzero size. With size 0 it goes to zstd, whose
// API allows it, so an empty array (which .NET pins as NULL) behaves like any other empty buffer.
#define SZ_ZSTD_ARG_ERROR ((size_t)-1)
#define SZ_ZSTD_NULL_BUF(p, n) (!(p) && (n) != 0)

//
// ===== Compression Context Management =====
//
// Each create refuses a struct that already holds an object: it would replace it, and the old one would leak
// (audit/memory.md M10). Free first, which sets the field back to NULL. GrindCore.net always passes a zeroed struct.
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateCompressionContext(SZ_ZStd_v1_5_2_CompressionContext* ctx) {
    if (!ctx || ctx->cctx) return -1;
    ctx->cctx = ZSTD_v1_5_2_createCCtx();
    return ctx->cctx ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateDecompressionContext(SZ_ZStd_v1_5_2_DecompressionContext* ctx) {
    if (!ctx || ctx->dctx) return -1;
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
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateCompressionDict(SZ_ZStd_v1_5_2_CompressionDict* dict, const void* dictBuffer, size_t dictSize, int32_t compressionLevel, int32_t windowLog) {
    if (!dict || dict->cdict || !dictBuffer || dictSize == 0) return -1;   // a dictionary already there: M10, above
    if (windowLog > 0) {
        /* Advanced path: override windowLog explicitly - see the matching v1.5.7 comment for why. */
        ZSTD_v1_5_2_CCtx_params* params = ZSTD_v1_5_2_createCCtxParams();
        if (!params) return -1;
        ZSTD_v1_5_2_CCtxParams_init(params, compressionLevel);
        ZSTD_v1_5_2_CCtxParams_setParameter(params, ZSTD_v1_5_2_c_windowLog, windowLog);
        dict->cdict = ZSTD_v1_5_2_createCDict_advanced2(dictBuffer, dictSize, ZSTD_v1_5_2_dlm_byCopy, ZSTD_v1_5_2_dct_auto, params, ZSTD_v1_5_2_defaultCMem);
        ZSTD_v1_5_2_freeCCtxParams(params);
    } else {
        dict->cdict = ZSTD_v1_5_2_createCDict(dictBuffer, dictSize, compressionLevel);
    }
    return dict->cdict ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CreateDecompressionDict(SZ_ZStd_v1_5_2_DecompressionDict* dict, const void* dictBuffer, size_t dictSize) {
    if (!dict || dict->ddict || !dictBuffer || dictSize == 0) return -1;
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
    if (!ctx || !ctx->cctx || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcSize)) return SZ_ZSTD_ARG_ERROR;
    return ZSTD_v1_5_2_compressCCtx(ctx->cctx, dst, dstCapacity, src, srcSize, compressionLevel);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_DecompressBlock(SZ_ZStd_v1_5_2_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->dctx || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcSize)) return SZ_ZSTD_ARG_ERROR;
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
    if (!ctx || !ctx->cctx || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcCapacity) || !outSize)
        return SZ_ZSTD_ARG_ERROR;
    if (endOp == ZSTD_v1_5_2_e_continue && !inSize) return SZ_ZSTD_ARG_ERROR;

    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_v1_5_2_inBuffer input = { src, srcCapacity, 0 };

    size_t toFlush = ZSTD_v1_5_2_compressStream2(ctx->cctx, &output, &input, endOp);

    if (inSize) *inSize = input.pos; // optional for FlushStream and EndStream
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
    if (!ctx || !ctx->dctx || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcCapacity) || !inSize || !outSize)
        return SZ_ZSTD_ARG_ERROR;

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
    if (!ctx || !ctx->cctx) return -1;
    return (int32_t)ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_targetCBlockSize, (int)blockSize);
}

//
// ===== Multithreading Configuration =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetNbWorkers(SZ_ZStd_v1_5_2_CompressionContext* ctx, int32_t nbWorkers) {
    if (!ctx || !ctx->cctx || nbWorkers < 0) return -1;
    return (int32_t)ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_nbWorkers, nbWorkers);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetJobSize(SZ_ZStd_v1_5_2_CompressionContext* ctx, size_t jobSize) {
    if (!ctx || !ctx->cctx) return -1;
    return (int32_t)ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_jobSize, (int)jobSize);
}

//
// ===== Dictionary Compression & Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressBlockWithDict(SZ_ZStd_v1_5_2_CompressionContext* ctx, SZ_ZStd_v1_5_2_CompressionDict* dict, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->cctx || !dict || !dict->cdict || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcSize))
        return SZ_ZSTD_ARG_ERROR;
    return ZSTD_v1_5_2_compress_usingCDict(ctx->cctx, dst, dstCapacity, src, srcSize, dict->cdict);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_DecompressBlockWithDict(SZ_ZStd_v1_5_2_DecompressionContext* ctx, SZ_ZStd_v1_5_2_DecompressionDict* dict, void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    if (!ctx || !ctx->dctx || !dict || !dict->ddict || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcSize))
        return SZ_ZSTD_ARG_ERROR;
    return ZSTD_v1_5_2_decompress_usingDDict(ctx->dctx, dst, dstCapacity, src, srcSize, dict->ddict);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetCompressionDict(SZ_ZStd_v1_5_2_CompressionContext* ctx, SZ_ZStd_v1_5_2_CompressionDict* dict) {
    if (!ctx || !ctx->cctx) return -1;
    size_t result = ZSTD_v1_5_2_CCtx_refCDict(ctx->cctx, dict ? dict->cdict : NULL);
    return ZSTD_v1_5_2_isError(result) ? -1 : 0;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_SetDecompressionDict(SZ_ZStd_v1_5_2_DecompressionContext* ctx, SZ_ZStd_v1_5_2_DecompressionDict* dict) {
    if (!ctx || !ctx->dctx) return -1;
    size_t result = ZSTD_v1_5_2_DCtx_refDDict(ctx->dctx, dict ? dict->ddict : NULL);
    return ZSTD_v1_5_2_isError(result) ? -1 : 0;
}

//
// ===== Block Compression with Frame Parameters (audit/zstd.md 2.3) =====
//
// As in the 1.5.7 PAL: CompressBlock and CompressBlockWithDict with the content-size, checksum and dictionary-ID
// flags chosen by the caller (0 or 1 each); 1, 0, 1 give exactly the plain functions' output.
#define SZ_ZSTD_FLAG(f) ((f) == 0 || (f) == 1)

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressBlockAdvanced(SZ_ZStd_v1_5_2_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t compressionLevel, int32_t contentSizeFlag, int32_t checksumFlag, int32_t dictIDFlag) {
    if (!ctx || !ctx->cctx || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcSize)
        || !SZ_ZSTD_FLAG(contentSizeFlag) || !SZ_ZSTD_FLAG(checksumFlag) || !SZ_ZSTD_FLAG(dictIDFlag))
        return SZ_ZSTD_ARG_ERROR;
    size_t r = ZSTD_v1_5_2_CCtx_reset(ctx->cctx, ZSTD_v1_5_2_reset_session_and_parameters);
    if (!ZSTD_v1_5_2_isError(r)) r = ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_compressionLevel, compressionLevel);
    if (!ZSTD_v1_5_2_isError(r)) r = ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_contentSizeFlag, contentSizeFlag);
    if (!ZSTD_v1_5_2_isError(r)) r = ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_checksumFlag, checksumFlag);
    if (!ZSTD_v1_5_2_isError(r)) r = ZSTD_v1_5_2_CCtx_setParameter(ctx->cctx, ZSTD_v1_5_2_c_dictIDFlag, dictIDFlag);
    return ZSTD_v1_5_2_isError(r) ? r : ZSTD_v1_5_2_compress2(ctx->cctx, dst, dstCapacity, src, srcSize);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_CompressBlockWithDictAdvanced(SZ_ZStd_v1_5_2_CompressionContext* ctx, SZ_ZStd_v1_5_2_CompressionDict* dict, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t contentSizeFlag, int32_t checksumFlag, int32_t dictIDFlag) {
    if (!ctx || !ctx->cctx || !dict || !dict->cdict || SZ_ZSTD_NULL_BUF(dst, dstCapacity) || SZ_ZSTD_NULL_BUF(src, srcSize)
        || !SZ_ZSTD_FLAG(contentSizeFlag) || !SZ_ZSTD_FLAG(checksumFlag) || !SZ_ZSTD_FLAG(dictIDFlag))
        return SZ_ZSTD_ARG_ERROR;
    ZSTD_v1_5_2_frameParameters const fParams = { contentSizeFlag, checksumFlag, !dictIDFlag };
    return ZSTD_v1_5_2_compress_usingCDict_advanced(ctx->cctx, dst, dstCapacity, src, srcSize, dict->cdict, fParams);
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

//
// ===== Error Handling =====
//
FUNCTIONEXPORT unsigned FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_IsError(size_t result) {
    return ZSTD_v1_5_2_isError(result);
}

FUNCTIONEXPORT const char* FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_GetErrorName(size_t result) {
    return ZSTD_v1_5_2_getErrorName(result);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_GetErrorCode(size_t result) {
    return (int32_t)ZSTD_v1_5_2_getErrorCode(result);
}

//
// ===== Skippable Frame Functions =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_WriteSkippableFrame(
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcSize,
    uint32_t magicVariant)
{
    return ZSTD_v1_5_2_writeSkippableFrame(dst, dstCapacity, src, srcSize, magicVariant);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_ReadSkippableFrame(
    void* dst,
    size_t dstCapacity,
    uint32_t* magicVariant,
    const void* src,
    size_t srcSize)
{
    return ZSTD_v1_5_2_readSkippableFrame(dst, dstCapacity, magicVariant, src, srcSize);
}

FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_IsSkippableFrame(
    const void* buffer,
    size_t size)
{
    return ZSTD_v1_5_2_isSkippableFrame(buffer, size);
}

