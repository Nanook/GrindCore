#include "pal_facebook_zstd_v1_5_2_seekable.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//
// ===== Seekable Compression Context Management =====
//
// The creates refuse a struct that already holds a stream, which they would replace and leak (audit/memory.md M10).
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_CreateCStream(SZ_ZStd_v1_5_2_SeekableCStream* ctx) {
    if (!ctx || ctx->zcs) return -1;
    ctx->zcs = ZSTD_v1_5_2_seekable_createCStream();
    return ctx->zcs ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_FreeCStream(SZ_ZStd_v1_5_2_SeekableCStream* ctx) {
    if (ctx && ctx->zcs) {
        ZSTD_v1_5_2_seekable_freeCStream(ctx->zcs);
        ctx->zcs = NULL;
        return 0;
    }
    return -1;
}

//
// ===== Seekable Compression Initialization =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_InitCStream(
    SZ_ZStd_v1_5_2_SeekableCStream* ctx,
    int32_t compressionLevel,
    int32_t checksumFlag,
    uint32_t maxFrameSize)
{
    if (!ctx || !ctx->zcs) return (size_t)-1;
    return ZSTD_v1_5_2_seekable_initCStream(ctx->zcs, compressionLevel, checksumFlag, maxFrameSize);
}

//
// ===== Seekable Streaming Compression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_CompressStream(
    SZ_ZStd_v1_5_2_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    const void* src,
    size_t srcCapacity,
    int64_t* inSize,
    int64_t* outSize)
{
    if (!ctx || !ctx->zcs || !dst || !src || !inSize || !outSize) return (size_t)-1;

    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, 0 };
    ZSTD_v1_5_2_inBuffer input = { src, srcCapacity, 0 };

    size_t ret = ZSTD_v1_5_2_seekable_compressStream(ctx->zcs, &output, &input);

    *inSize = (int64_t)input.pos;
    *outSize = (int64_t)output.pos;

    return ret;
}

//
// ===== Seekable Frame Control =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_EndFrame(
    SZ_ZStd_v1_5_2_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    int64_t* outSize)
{
    if (!ctx || !ctx->zcs || !dst || !outSize) return (size_t)-1;

    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, 0 };

    size_t ret = ZSTD_v1_5_2_seekable_endFrame(ctx->zcs, &output);

    *outSize = (int64_t)output.pos;

    return ret;
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_EndStream(
    SZ_ZStd_v1_5_2_SeekableCStream* ctx,
    void* dst,
    size_t dstCapacity,
    int64_t* outSize)
{
    if (!ctx || !ctx->zcs || !dst || !outSize) return (size_t)-1;

    ZSTD_v1_5_2_outBuffer output = { dst, dstCapacity, 0 };

    size_t ret = ZSTD_v1_5_2_seekable_endStream(ctx->zcs, &output);

    *outSize = (int64_t)output.pos;

    return ret;
}

//
// ===== Seekable Decompression Context Management =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_Create(SZ_ZStd_v1_5_2_Seekable* ctx) {
    if (!ctx || ctx->zs) return -1;   // M10, as CreateCStream
    ctx->zs = ZSTD_v1_5_2_seekable_create();
    return ctx->zs ? 0 : -1;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_Free(SZ_ZStd_v1_5_2_Seekable* ctx) {
    if (ctx && ctx->zs) {
        ZSTD_v1_5_2_seekable_free(ctx->zs);
        ctx->zs = NULL;
        return 0;
    }
    return -1;
}

//
// ===== Seek table checks (audit/other-codecs.md 4.8.2) =====
//
// zstd's loader takes each seek-table entry's 32-bit sizes as they are. A corrupt or crafted table can then:
//   - hold a size within zstd's error-code range of 4 GiB, which GetFrameCompressedSize/GetFrameDecompressedSize
//     return as an error code where size_t is 32-bit (and as the size on 64-bit);
//   - claim more decompressed data than its frame's compressed bytes can hold, which shows up as a false
//     GetDecompressedSize;
//   - place frames past the start of the seek table (checked for InitBuff, where the source's size is known).
// No file zstd writes does any of these: its frames are at most ZSTD_v1_5_2_seekable_MAX_FRAME_DECOMPRESSED_SIZE. A
// table that does is rejected with corruption_detected, the same on every RID, and the decoder is freed so nothing can
// read through it: every call on the context then fails, and Free does nothing. The same checks as the 1.5.7 PAL.

// The most one zstd block decodes to (Block_Maximum_Size). The densest block, RLE, takes 4 bytes for it (a 3-byte
// header and the byte), so a frame of c compressed bytes decodes to at most c / 4 of these.
#define SZ_ZSTD_SEEK_BLOCK_MAX ((uint64_t)128 * 1024)

// ZSTD_v1_5_2_error_corruption_detected (zstd_v1_5_2/zstd_errors.h:59). That header can't be included here: the
// seekable header's bare #include "zstd.h" finds 1.5.7's first on GrindCore's include path, which brings 1.5.7's
// zstd_errors.h, and the two define the same macros. The tests check the result by its name.
#define SZ_ZSTD_V1_5_2_CORRUPTION_DETECTED ((size_t)-20)

// Where size_t is 32-bit, an entry size s reads as -(2^32 - s). Testing that with zstd's own isError gives the
// 32-bit answer on every RID.
static int SZ_ZStd_v1_5_2_SeekSizeReadsAsError(uint64_t size)
{
    return size != 0 && ZSTD_v1_5_2_isError((size_t)0 - (size_t)(((uint64_t)1 << 32) - size));
}

static size_t SZ_ZStd_v1_5_2_Seekable_CheckTable(SZ_ZStd_v1_5_2_Seekable* ctx, const void* src, size_t srcSize)
{
    unsigned const n = ZSTD_v1_5_2_seekable_getNumFrames(ctx->zs);
    uint64_t end = 0;
    unsigned i;
    for (i = 0; i < n; i++) {
        // each is one 32-bit entry, which size_t holds exactly on every RID
        uint64_t const c = ZSTD_v1_5_2_seekable_getFrameCompressedSize(ctx->zs, i);
        uint64_t const d = ZSTD_v1_5_2_seekable_getFrameDecompressedSize(ctx->zs, i);
        if (SZ_ZStd_v1_5_2_SeekSizeReadsAsError(c) || SZ_ZStd_v1_5_2_SeekSizeReadsAsError(d) ||
            d > c / 4 * SZ_ZSTD_SEEK_BLOCK_MAX)
            goto bad;
        end += c;
    }
    if (src) {
        // the seek table is the last frame: an 8-byte skippable header, the entries, then the footer, whose
        // descriptor's top bit says whether entries carry a checksum (the loader has checked all of it)
        const unsigned char* footer = (const unsigned char*)src + srcSize - ZSTD_seekTableFooterSize;
        uint64_t const entry = (footer[4] & 0x80) ? 12 : 8;
        if (end + 8 + n * entry + ZSTD_seekTableFooterSize > srcSize)
            goto bad;
    }
    return 0;
bad:
    ZSTD_v1_5_2_seekable_free(ctx->zs);
    ctx->zs = NULL;
    return SZ_ZSTD_V1_5_2_CORRUPTION_DETECTED;
}

//
// ===== Seekable Decompression Initialization - In-Memory =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_InitBuff(
    SZ_ZStd_v1_5_2_Seekable* ctx,
    const void* src,
    size_t srcSize)
{
    if (!ctx || !ctx->zs || !src) return (size_t)-1;
    size_t const r = ZSTD_v1_5_2_seekable_initBuff(ctx->zs, src, srcSize);
    return ZSTD_v1_5_2_isError(r) ? r : SZ_ZStd_v1_5_2_Seekable_CheckTable(ctx, src, srcSize);
}

//
// ===== Seekable Decompression Initialization - Streaming =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_InitAdvanced(
    SZ_ZStd_v1_5_2_Seekable* ctx,
    void* opaque,
    ZSTD_v1_5_2_seekable_read* readFn,
    ZSTD_v1_5_2_seekable_seek* seekFn)
{
    if (!ctx || !ctx->zs || !readFn || !seekFn) return (size_t)-1;

    ZSTD_v1_5_2_seekable_customFile customFile;
    customFile.opaque = opaque;
    customFile.read = readFn;
    customFile.seek = seekFn;

    size_t const r = ZSTD_v1_5_2_seekable_initAdvanced(ctx->zs, customFile);
    return ZSTD_v1_5_2_isError(r) ? r : SZ_ZStd_v1_5_2_Seekable_CheckTable(ctx, NULL, 0);
}

//
// ===== Seekable Decompression =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_Decompress(
    SZ_ZStd_v1_5_2_Seekable* ctx,
    void* dst,
    size_t dstSize,
    uint64_t offset)
{
    if (!ctx || !ctx->zs || !dst) return (size_t)-1;
    return ZSTD_v1_5_2_seekable_decompress(ctx->zs, dst, dstSize, (unsigned long long)offset);
}

//
// ===== Seekable Decompression by Frame =====
//
FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_DecompressFrame(
    SZ_ZStd_v1_5_2_Seekable* ctx,
    void* dst,
    size_t dstSize,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs || !dst) return (size_t)-1;
    return ZSTD_v1_5_2_seekable_decompressFrame(ctx->zs, dst, dstSize, frameIndex);
}

//
// ===== Seekable Seek Table Inspection =====
//
FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_GetNumFrames(
    const SZ_ZStd_v1_5_2_Seekable* ctx)
{
    if (!ctx || !ctx->zs) return 0;
    return ZSTD_v1_5_2_seekable_getNumFrames(ctx->zs);
}

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_GetFrameCompressedOffset(
    const SZ_ZStd_v1_5_2_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (uint64_t)-1;
    return ZSTD_v1_5_2_seekable_getFrameCompressedOffset(ctx->zs, frameIndex);
}

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_GetFrameDecompressedOffset(
    const SZ_ZStd_v1_5_2_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (uint64_t)-1;
    return ZSTD_v1_5_2_seekable_getFrameDecompressedOffset(ctx->zs, frameIndex);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_GetFrameCompressedSize(
    const SZ_ZStd_v1_5_2_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (size_t)-1;
    return ZSTD_v1_5_2_seekable_getFrameCompressedSize(ctx->zs, frameIndex);
}

FUNCTIONEXPORT size_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_GetFrameDecompressedSize(
    const SZ_ZStd_v1_5_2_Seekable* ctx,
    uint32_t frameIndex)
{
    if (!ctx || !ctx->zs) return (size_t)-1;
    return ZSTD_v1_5_2_seekable_getFrameDecompressedSize(ctx->zs, frameIndex);
}

FUNCTIONEXPORT uint64_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_GetDecompressedSize(
    const SZ_ZStd_v1_5_2_Seekable* ctx)
{
    if (!ctx || !ctx->zs) return 0;

    // Get total decompressed size by summing all frame sizes
    uint32_t numFrames = ZSTD_v1_5_2_seekable_getNumFrames(ctx->zs);
    if (numFrames == 0) return 0;

    // The last frame's decompressed offset plus its size gives us the total
    uint64_t lastFrameOffset = ZSTD_v1_5_2_seekable_getFrameDecompressedOffset(ctx->zs, numFrames - 1);
    size_t lastFrameSize = ZSTD_v1_5_2_seekable_getFrameDecompressedSize(ctx->zs, numFrames - 1);

    return lastFrameOffset + lastFrameSize;
}

FUNCTIONEXPORT uint32_t FUNCTIONCALLINGCONVENCTION SZ_ZStd_v1_5_2_Seekable_OffsetToFrameIndex(
    const SZ_ZStd_v1_5_2_Seekable* ctx,
    uint64_t offset)
{
    if (!ctx || !ctx->zs) return (uint32_t)-1;
    return ZSTD_v1_5_2_seekable_offsetToFrameIndex(ctx->zs, (unsigned long long)offset);
}
