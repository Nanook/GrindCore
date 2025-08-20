#include "pal_lz4_lz4_v1_10_0.h"

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_Init(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    stream->internalState = (void*)LZ4_createStream();
    return (stream->internalState != NULL) ? SZ_Lz4_v1_10_0_OK : SZ_Lz4_v1_10_0_MEMERROR;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_End(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return;

    LZ4_freeStream((LZ4_stream_t*)stream->internalState);
    stream->internalState = NULL;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_ResetStream(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return;

    LZ4_resetStream_fast((LZ4_stream_t*)stream->internalState);
}

FUNCTIONEXPORT LZ4_stream_t* FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return NULL;

    return (LZ4_stream_t*)stream->internalState;
}

FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_TransferStateToPalLZ4Stream(const LZ4_stream_t* from, SZ_Lz4_v1_10_0_Stream* to)
{
    if (from == NULL || to == NULL)
        return;

    to->internalState = (void*)from;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressFastContinue(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int srcSize,
    int dstCapacity,
    int acceleration)
{
    if (stream == NULL || src == NULL || dst == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    LZ4_stream_t* lz4Stream = (LZ4_stream_t*)stream->internalState;
    int compressedSize = LZ4_compress_fast_continue(lz4Stream, src, dst, srcSize, dstCapacity, acceleration);

    return (compressedSize >= 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressSafeContinue(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int compressedSize,
    int dstCapacity)
{
    if (stream == NULL || src == NULL || dst == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    LZ4_streamDecode_t* lz4StreamDecode = (LZ4_streamDecode_t*)stream->internalState;
    int decompressedSize = LZ4_decompress_safe_continue(lz4StreamDecode, src, dst, compressedSize, dstCapacity);

    return (decompressedSize >= 0) ? decompressedSize : SZ_Lz4_v1_10_0_DECOMPRESSFAIL;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_LoadDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* dictionary,
    int dictSize)
{
    if (stream == NULL || dictionary == NULL || dictSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    LZ4_stream_t* lz4Stream = (LZ4_stream_t*)stream->internalState;
    int result = LZ4_loadDict(lz4Stream, dictionary, dictSize);

    return (result >= 0) ? SZ_Lz4_v1_10_0_OK : SZ_Lz4_v1_10_0_ERROR;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_SaveDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    char* safeBuffer,
    int maxDictSize)
{
    if (stream == NULL || safeBuffer == NULL || maxDictSize <= 0)
        return SZ_Lz4_v1_10_0_ERROR;

    LZ4_stream_t* lz4Stream = (LZ4_stream_t*)stream->internalState;
    int savedSize = LZ4_saveDict(lz4Stream, safeBuffer, maxDictSize);

    return (savedSize >= 0) ? savedSize : SZ_Lz4_v1_10_0_ERROR;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_AttachDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    SZ_Lz4_v1_10_0_Stream* dictStream)
{
    if (stream == NULL || dictStream == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    LZ4_stream_t* lz4Stream = (LZ4_stream_t*)stream->internalState;
    LZ4_stream_t* dictLZ4Stream = (LZ4_stream_t*)dictStream->internalState;

    LZ4_attach_dictionary(lz4Stream, dictLZ4Stream);
    return SZ_Lz4_v1_10_0_OK;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_Flush(SZ_Lz4_v1_10_0_Stream* stream)
{
    if (stream == NULL || stream->internalState == NULL)
        return SZ_Lz4_v1_10_0_ERROR;

    // Ensures stream flushing happens (LZ4 does not have an explicit flush function, but resetting achieves similar effect)
    LZ4_resetStream_fast((LZ4_stream_t*)stream->internalState);
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
    return (decompressedSize > 0) ? decompressedSize : SZ_Lz4_v1_10_0_DECOMPRESSFAIL;
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
    if (ctx == NULL || ctx->internalState == NULL || dstBuffer == NULL || srcBuffer == NULL) 
        return SZ_Lz4_v1_10_0_ERROR;

    // Ensure HC streaming context is correctly handled
    LZ4_streamHC_t* hcStream = (LZ4_streamHC_t*)ctx->internalState;
    if (hcStream == NULL) 
        return SZ_Lz4_v1_10_0_ERROR;

    // Reset stream with the requested compression level
    LZ4_resetStreamHC_fast(hcStream, compressionLevel);

    // Perform HC streaming compression
    int32_t compressedSize = LZ4_compress_HC_continue(hcStream, srcBuffer, dstBuffer, srcSize, (int)dstCapacity);

    return compressedSize;

    //return (compressedSize > 0) ? compressedSize : SZ_Lz4_v1_10_0_COMPRESSFAIL;
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
    if (ctx == NULL) return SZ_Lz4_v1_10_0_ERROR;
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
    if (ctx == NULL) return SZ_Lz4_v1_10_0_ERROR;
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
