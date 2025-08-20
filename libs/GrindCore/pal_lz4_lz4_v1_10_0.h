#ifndef SZ_Lz4_v1_10_0_H
#define SZ_Lz4_v1_10_0_H

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

#define LZ4_STATIC_LINKING_ONLY
#include "external/lz4/lz4/lz4.h"
#include "external/lz4/lz4/lz4hc.h"
#include "external/lz4/lz4/lz4frame.h"

// Define error codes for consistency
#define SZ_Lz4_v1_10_0_OK             0
#define SZ_Lz4_v1_10_0_ERROR         -1
#define SZ_Lz4_v1_10_0_MEMERROR      -2
#define SZ_Lz4_v1_10_0_COMPRESSFAIL  -3
#define SZ_Lz4_v1_10_0_DECOMPRESSFAIL -4

typedef struct {
    void* internalState;
} SZ_Lz4_v1_10_0_Stream;

size_t LZ4F_uncompressedUpdate(LZ4F_cctx* cctxPtr,
                               void* dstBuffer, size_t dstCapacity,
                         const void* srcBuffer, size_t srcSize,
                         const LZ4F_compressOptions_t* compressOptionsPtr);
                         
/* Initializes the LZ4 streaming object. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_Init(SZ_Lz4_v1_10_0_Stream* stream);

/* Frees memory allocated for the LZ4 streaming object. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_End(SZ_Lz4_v1_10_0_Stream* stream);

/* Resets the LZ4 stream object for fresh compression cycles. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_ResetStream(SZ_Lz4_v1_10_0_Stream* stream);

/* Transfers the state from an LZ4 stream object to PAL. */
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_TransferStateToPalLZ4Stream(const LZ4_stream_t* from, SZ_Lz4_v1_10_0_Stream* to);

/* Retrieves the current LZ4 stream object. */
FUNCTIONEXPORT LZ4_stream_t* FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(SZ_Lz4_v1_10_0_Stream* stream);

/* Compresses data using LZ4's streaming API. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressFastContinue(
    SZ_Lz4_v1_10_0_Stream* stream, 
    const char* src, 
    char* dst, 
    int srcSize, 
    int dstCapacity, 
    int acceleration);

/* Compresses data using partial output size. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressPartial(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int srcSize,
    int targetSize);

/* Decompresses data using LZ4's streaming API. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressSafeContinue(
    SZ_Lz4_v1_10_0_Stream* stream, 
    const char* src, 
    char* dst, 
    int compressedSize, 
    int dstCapacity);

/* Loads a dictionary into the LZ4 stream for better compression. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_LoadDict(
    SZ_Lz4_v1_10_0_Stream* stream, 
    const char* dictionary, 
    int dictSize);

/* Saves the dictionary for reuse in compression. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_SaveDict(
    SZ_Lz4_v1_10_0_Stream* stream, 
    char* safeBuffer, 
    int maxDictSize);

/* Attaches a dictionary stream to an LZ4 stream for improved compression. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_AttachDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    SZ_Lz4_v1_10_0_Stream* dictStream);

/* Flushes the LZ4 stream to ensure complete output is written. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_Flush(SZ_Lz4_v1_10_0_Stream* stream);

/* Decompresses using a dictionary for better accuracy. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressUsingDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int srcSize,
    int dstCapacity,
    const char* dictStart,
    int dictSize);

/* Partial decompression using a dictionary reference. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_DecompressPartialUsingDict(
    SZ_Lz4_v1_10_0_Stream* stream,
    const char* src,
    char* dst,
    int compressedSize,
    int targetOutputSize,
    int maxOutputSize,
    const char* dictStart,
    int dictSize);

/////////////////////////////////////////////////////////////////////
// High Compression Methods

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressHC(
    const char* src,
    char* dst,
    int srcSize,
    int dstCapacity,
    int compressionLevel);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressHC_ExtState(
    void* stateHC,
    const char* src,
    char* dst,
    int srcSize,
    int maxDstSize,
    int compressionLevel);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4_v1_10_0_CompressHC_DestSize(
    void* stateHC,
    const char* src,
    char* dst,
    int* srcSizePtr,
    int targetDstSize,
    int compressionLevel);

/////////////////////////////////////////////////////////////////////
// Frame Methods

// LZ4 Frame Compression Structure
typedef struct {
    void* internalState;
} SZ_Lz4F_v1_10_0_CompressionContext;

// LZ4 Frame Decompression Structure
typedef struct {
    void* internalState;
} SZ_Lz4F_v1_10_0_DecompressionContext;

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressHC_Stream(
    SZ_Lz4F_v1_10_0_CompressionContext* ctx,
    void* dstBuffer, size_t dstCapacity,
    const void* srcBuffer, int srcSize,
    int compressionLevel,
    const LZ4F_compressOptions_t* cOptPtr);

// Computes the maximum compressed size for a given input size using LZ4 Frame
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressFrameBound(size_t srcSize, const LZ4F_preferences_t* prefsPtr);

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressBound(size_t srcSize, const LZ4F_preferences_t* prefsPtr);

// Compression Context Management
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CreateCompressionContext(SZ_Lz4F_v1_10_0_CompressionContext* ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_FreeCompressionContext(SZ_Lz4F_v1_10_0_CompressionContext* ctx);

// Frame-Based Compression Functions
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressBegin(SZ_Lz4F_v1_10_0_CompressionContext* ctx, void* dstBuffer, size_t dstCapacity, const LZ4F_preferences_t* prefsPtr);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressUpdate(SZ_Lz4F_v1_10_0_CompressionContext* ctx, void* dstBuffer, size_t dstCapacity, const void* srcBuffer, size_t srcSize, const LZ4F_compressOptions_t* cOptPtr);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_Flush(SZ_Lz4F_v1_10_0_CompressionContext* ctx, void* dstBuffer, size_t dstCapacity, const LZ4F_compressOptions_t* cOptPtr);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CompressEnd(SZ_Lz4F_v1_10_0_CompressionContext* ctx, void* dstBuffer, size_t dstCapacity, const LZ4F_compressOptions_t* cOptPtr);

// Decompression Context Management
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_CreateDecompressionContext(SZ_Lz4F_v1_10_0_DecompressionContext* ctx);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_FreeDecompressionContext(SZ_Lz4F_v1_10_0_DecompressionContext* ctx);

// Frame-Based Decompression Functions
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_GetFrameInfo(SZ_Lz4F_v1_10_0_DecompressionContext* ctx, LZ4F_frameInfo_t* frameInfoPtr, const void* srcBuffer, size_t* srcSizePtr);
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_Decompress(SZ_Lz4F_v1_10_0_DecompressionContext* ctx, void* dstBuffer, size_t* dstSizePtr, const void* srcBuffer, size_t* srcSizePtr, const LZ4F_decompressOptions_t* dOptPtr);
FUNCTIONEXPORT void FUNCTIONCALLINGCONVENCTION SZ_Lz4F_v1_10_0_ResetDecompressionContext(SZ_Lz4F_v1_10_0_DecompressionContext* ctx);

#endif // SZ_Lz4_v1_10_0_H