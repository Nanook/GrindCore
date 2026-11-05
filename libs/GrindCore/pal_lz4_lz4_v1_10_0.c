#define LZ4F_STATIC_LINKING_ONLY  // LZ4F_getErrorCode; LZ4 is linked in statically, so its static-only API is safe
#include "pal_lz4_lz4_v1_10_0.h"
#include <stdlib.h>
#include <string.h>

// A Stream serves both directions, so its state holds both LZ4 states (audit/other-codecs.md 4.1.1). The compressor's
// comes first, so internalState is still an LZ4_stream_t* (GetCurrentLZ4Stream, AttachDict, CompressPartial). The
// decoder used to take that same LZ4_stream_t as its LZ4_streamDecode_t: after compressing or LoadDict, it read
// hash-table entries as its history pointers.
typedef struct {
    LZ4_stream_t encode;
    LZ4_streamDecode_t decode;
} SZ_Lz4_v1_10_0_StreamState;

#define SZ_LZ4_ENCODE(stream) (&((SZ_Lz4_v1_10_0_StreamState*)(stream)->internalState)->encode)
#define SZ_LZ4_DECODE(stream) (&((SZ_Lz4_v1_10_0_StreamState*)(stream)->internalState)->decode)

// Init, and the LZ4F context creates below, refuse a struct that already holds a state: they would replace it, and it
// would leak (audit/memory.md M10). End or free it first, which sets it back to NULL. GrindCore.net always passes a
// zeroed struct.
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_Init(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState != NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    SZ_Lz4_v1_10_0_StreamState* state = (SZ_Lz4_v1_10_0_StreamState*)malloc(sizeof(SZ_Lz4_v1_10_0_StreamState));
    stream->internalState = state;
    if (state == NULL)
        return SZ_Lz4_v1_10_0_MEMERROR;
    LZ4_initStream(&state->encode, sizeof(state->encode));
    LZ4_setStreamDecode(&state->decode, NULL, 0);
    return SZ_Lz4_v1_10_0_OK;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_End(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return;

    free(stream->internalState);
    stream->internalState = NULL;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_ResetStream(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return;

    LZ4_resetStream_fast(SZ_LZ4_ENCODE(stream));
}

FUNCTIONEXPORT LZ4_stream_t* FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return NULL;

    return SZ_LZ4_ENCODE(stream);
}

// Copies the compression state (it used to adopt `from`, which End then freed). `to` is initialised first if needed;
// the caller keeps `from`. The copy still refers to the same history and dictionary buffers, so they must stay valid.
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_TransferStateToPalLZ4Stream(const LZ4_stream_t* from, SZ_Lz4_v1_10_0_Stream* to)
{
    if (from == NULL || to == NULL)
        return;
    if (to->internalState == NULL && SZ_Lz4_v1_10_0_Init(to) != SZ_Lz4_v1_10_0_OK)
        return;

    memcpy(SZ_LZ4_ENCODE(to), from, sizeof(LZ4_stream_t));
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressFastContinue(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int srcSize,
    int dstCapacity,
    int acceleration)
{
    if (stream == NULL || stream->internalState == NULL || src == NULL || dst == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    int compressedSize = LZ4_compress_fast_continue(SZ_LZ4_ENCODE(stream), src, dst, srcSize, dstCapacity, acceleration);

    return (compressedSize >= 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressPartial(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int* srcSize,
    int targetSize,
    int acceleration)
{
    if (stream == NULL || stream->internalState == NULL || src == NULL || dst == NULL || srcSize == NULL)
        return SZ_Lz4_v1_10_0_ERROR;
    if (*srcSize < 0 || targetSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    // Uses the stream's LZ4_stream_t as the external state; LZ4 re-initialises it before and after (lz4.c,
    // LZ4_compress_destSize_extState), so the block is independent and the stream is left reset.
    int compressedSize = LZ4_compress_destSize_extState(SZ_LZ4_ENCODE(stream), src, dst, srcSize, targetSize, acceleration);

    return (compressedSize > 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressSafeContinue(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int compressedSize,
    int dstCapacity)
{
    if (stream == NULL || stream->internalState == NULL || src == NULL || dst == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    int decompressedSize = LZ4_decompress_safe_continue(SZ_LZ4_DECODE(stream), src, dst, compressedSize, dstCapacity);

    return (decompressedSize >= 0) ? decompressedSize : SZ_Lz4_v1_10_0_DECOMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_LoadDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* dictionary,
    int dictSize)
{
    if (stream == NULL || stream->internalState == NULL || dictionary == NULL || dictSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    int result = LZ4_loadDict(SZ_LZ4_ENCODE(stream), dictionary, dictSize);

    return (result >= 0) ? SZ_Lz4_v1_10_0_OK : SZ_Lz4_v1_10_0_ERROR;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_SaveDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    char* safeBuffer,
    int maxDictSize)
{
    if (stream == NULL || stream->internalState == NULL || safeBuffer == NULL || maxDictSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    int savedSize = LZ4_saveDict(SZ_LZ4_ENCODE(stream), safeBuffer, maxDictSize);

    return (savedSize >= 0) ? savedSize : SZ_Lz4_v1_10_0_ERROR;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_AttachDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    SZ_Lz4_v1_10_0_Stream* dictStream)
{
    if (stream == NULL || stream->internalState == NULL || dictStream == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    // a dictionary stream without state detaches the dictionary, as LZ4_attach_dictionary(stream, NULL) does
    LZ4_attach_dictionary(SZ_LZ4_ENCODE(stream), dictStream->internalState ? SZ_LZ4_ENCODE(dictStream) : NULL);
    return SZ_Lz4_v1_10_0_OK;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_Flush(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    // Ensures stream flushing happens (LZ4 does not have an explicit flush function, but resetting achieves similar effect)
    LZ4_resetStream_fast(SZ_LZ4_ENCODE(stream));
    return SZ_Lz4_v1_10_0_OK;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressUsingDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int srcSize,
    int dstCapacity,
    const char* dictStart,
    int dictSize)
{
    if (stream == NULL || src == NULL || dst == NULL || dictStart == NULL || dictSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    int decompressedSize = LZ4_decompress_safe_usingDict(src, dst, srcSize, dstCapacity, dictStart, dictSize);
    return (decompressedSize >= 0) ? decompressedSize : SZ_Lz4_v1_10_0_DECOMPRESSFAIL; // 0: a valid empty block
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressPartialUsingDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int compressedSize,
    int targetOutputSize,
    int maxOutputSize,
    const char* dictStart,
    int dictSize)
{
    if (stream == NULL || src == NULL || dst == NULL || dictStart == NULL || dictSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    int decompressedSize = LZ4_decompress_safe_partial_usingDict(src, dst, compressedSize, targetOutputSize, maxOutputSize, dictStart, dictSize);
    return (decompressedSize >= 0) ? decompressedSize : SZ_Lz4_v1_10_0_DECOMPRESSFAIL;
}

/////////////////////////////////////////////////////////////////////
// High Compression Methods

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressHC(
    const char* src,
    char* dst,
    int srcSize,
    int dstCapacity,
    int compressionLevel)
{
    if (src == NULL || dst == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    int compressedSize = LZ4_compress_HC(src, dst, srcSize, dstCapacity, compressionLevel);

    return (compressedSize > 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressHC_ExtState(
    void* stateHC,
    const char* src,
    char* dst,
    int srcSize,
    int maxDstSize,
    int compressionLevel)
{
    if (stateHC == NULL || src == NULL || dst == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    int compressedSize = LZ4_compress_HC_extStateHC(stateHC, src, dst, srcSize, maxDstSize, compressionLevel);

    return (compressedSize > 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressHC_DestSize(
    void* stateHC,
    const char* src,
    char* dst,
    int* srcSizePtr,
    int targetDstSize,
    int compressionLevel)
{
    if (stateHC == NULL || src == NULL || dst == NULL || srcSizePtr == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    int compressedSize = LZ4_compress_HC_destSize(stateHC, src, dst, srcSizePtr, targetDstSize, compressionLevel);

    return (compressedSize > 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
}

/////////////////////////////////////////////////////////////////////
// Frame Methods

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressHC_Stream(
    SZ_Lz4F_v1_10_0_CompressionContext* ctx,
    void* dstBuffer, size_t dstCapacity,
    const void* srcBuffer, int srcSize,  // Change srcSize to int
    int compressionLevel,
    const LZ4F_compressOptions_t* cOptPtr)
{
    // Not supported (audit/other-codecs.md 4.1.1). This used the LZ4F context's LZ4F_cctx, a couple of hundred bytes, as
    // an LZ4_streamHC_t (LZ4_STREAMHC_MINSIZE, 262200 bytes), resetting and compressing into it: heap corruption. An
    // LZ4F context has no HC stream of its own; HC frames come from the LZ4F functions with compressionLevel >= LZ4HC_CLEVEL_MIN (2).
    (void)ctx; (void)dstBuffer; (void)dstCapacity; (void)srcBuffer; (void)srcSize; (void)compressionLevel; (void)cOptPtr;
    return SZ_Lz4_v1_10_0_ERROR;
}

// Returns the upper bound on the size of a compressed frame for a given input size
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressFrameBound(size_t srcSize, const LZ4F_preferences_t* prefsPtr)
{
    return LZ4F_compressFrameBound(srcSize, prefsPtr);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressBound(size_t srcSize, const LZ4F_preferences_t* prefsPtr)
{
    return LZ4F_compressBound(srcSize, prefsPtr);
}

/* Compression Context Management */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CreateCompressionContext(SZ_Lz4F_v1_10_0_CompressionContext* ctx)
{
    if (ctx == NULL || ctx->internalState != NULL) return SZ_Lz4_v1_10_0_ERROR;   // M10, as Init
    return (int32_t)LZ4F_createCompressionContext((LZ4F_cctx**)&ctx->internalState, LZ4F_VERSION);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_FreeCompressionContext(SZ_Lz4F_v1_10_0_CompressionContext* ctx)
{
    if (ctx == NULL || ctx->internalState == NULL) return;
    LZ4F_freeCompressionContext((LZ4F_cctx*)ctx->internalState);
    ctx->internalState = NULL;
}

/* Frame Compression Functions */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressBegin(
    SZ_Lz4F_v1_10_0_CompressionContext* ctx,
    void* dstBuffer, size_t dstCapacity,
    const LZ4F_preferences_t* prefsPtr)
{
    if (ctx == NULL || ctx->internalState == NULL || dstBuffer == NULL) return SZ_Lz4_v1_10_0_ERROR;
    return LZ4F_compressBegin((LZ4F_cctx*)ctx->internalState, dstBuffer, dstCapacity, prefsPtr);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressUpdate(
    SZ_Lz4F_v1_10_0_CompressionContext* ctx,
    void* dstBuffer, size_t dstCapacity,
    const void* srcBuffer, size_t srcSize,
    const LZ4F_compressOptions_t* cOptPtr)
{
    if (ctx == NULL || ctx->internalState == NULL || dstBuffer == NULL || srcBuffer == NULL) return SZ_Lz4_v1_10_0_ERROR;
    return LZ4F_compressUpdate((LZ4F_cctx*)ctx->internalState, dstBuffer, dstCapacity, srcBuffer, srcSize, cOptPtr);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_Flush(
    SZ_Lz4F_v1_10_0_CompressionContext* ctx,
    void* dstBuffer, size_t dstCapacity,
    const LZ4F_compressOptions_t* cOptPtr)
{
    if (ctx == NULL || ctx->internalState == NULL || dstBuffer == NULL) return SZ_Lz4_v1_10_0_ERROR;
    return LZ4F_flush((LZ4F_cctx*)ctx->internalState, dstBuffer, dstCapacity, cOptPtr);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressEnd(
    SZ_Lz4F_v1_10_0_CompressionContext* ctx,
    void* dstBuffer, size_t dstCapacity,
    const LZ4F_compressOptions_t* cOptPtr)
{
    if (ctx == NULL || ctx->internalState == NULL || dstBuffer == NULL) return SZ_Lz4_v1_10_0_ERROR;
    return LZ4F_compressEnd((LZ4F_cctx*)ctx->internalState, dstBuffer, dstCapacity, cOptPtr);
}

/* Decompression Context Management */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CreateDecompressionContext(SZ_Lz4F_v1_10_0_DecompressionContext* ctx)
{
    if (ctx == NULL || ctx->internalState != NULL) return SZ_Lz4_v1_10_0_ERROR;   // M10, as Init
    return (int32_t)LZ4F_createDecompressionContext((LZ4F_dctx**)&ctx->internalState, LZ4F_VERSION);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_FreeDecompressionContext(SZ_Lz4F_v1_10_0_DecompressionContext* ctx)
{
    if (ctx == NULL || ctx->internalState == NULL) return;
    LZ4F_freeDecompressionContext((LZ4F_dctx*)ctx->internalState);
    ctx->internalState = NULL;
}

/* Frame Decompression Functions */
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_GetFrameInfo(
    SZ_Lz4F_v1_10_0_DecompressionContext* ctx,
    LZ4F_frameInfo_t* frameInfoPtr,
    const void* srcBuffer, size_t* srcSizePtr)
{
    if (ctx == NULL || ctx->internalState == NULL || frameInfoPtr == NULL || srcBuffer == NULL) return SZ_Lz4_v1_10_0_ERROR;
    return LZ4F_getFrameInfo((LZ4F_dctx*)ctx->internalState, frameInfoPtr, srcBuffer, srcSizePtr);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_Decompress(
    SZ_Lz4F_v1_10_0_DecompressionContext* ctx,
    void* dstBuffer, size_t* dstSizePtr,
    const void* srcBuffer, size_t* srcSizePtr,
    const LZ4F_decompressOptions_t* dOptPtr)
{
    if (ctx == NULL || ctx->internalState == NULL || dstBuffer == NULL || srcBuffer == NULL) return SZ_Lz4_v1_10_0_ERROR;
    return LZ4F_decompress((LZ4F_dctx*)ctx->internalState, dstBuffer, dstSizePtr, srcBuffer, srcSizePtr, dOptPtr);
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_ResetDecompressionContext(SZ_Lz4F_v1_10_0_DecompressionContext* ctx)
{
    if (ctx == NULL || ctx->internalState == NULL) return;
    LZ4F_resetDecompressionContext((LZ4F_dctx*)ctx->internalState);
}

/* Error Handling */
FUNCTIONEXPORT unsigned FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_IsError(size_t result)
{
    return LZ4F_isError(result);
}

FUNCTIONEXPORT const char* FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_GetErrorName(size_t result)
{
    return LZ4F_getErrorName(result);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_GetErrorCode(size_t result)
{
    return (int32_t)LZ4F_getErrorCode(result);
}
