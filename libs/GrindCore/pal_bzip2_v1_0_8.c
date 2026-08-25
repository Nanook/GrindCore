#include "pal_bzip2_v1_0_8.h"
#include <string.h>
#include <stdlib.h>

/* Required by bzip2 itself when built with BZ_NO_STDIO (as bzip2_v1_0_8's CMake target does):
 * bzlib_private.h's AssertH macro - used pervasively through blocksort.c/compress.c/decompress.c/
 * huffman.c for internal consistency checks - normally calls BZ2_bz__AssertH__fail(), which
 * fprintf()s to stderr and exit()s, but that function is itself compiled out under BZ_NO_STDIO.
 * In its place, AssertH calls this extern function, which the embedder is documented to supply
 * (see bzlib_private.h:88-90). This should only ever fire on a genuine internal bug/corruption in
 * bzip2's own state machine, never on ordinary bad input (which surfaces as a normal BZ_*_ERROR
 * return code instead) - abort() matches the original's "this is fatal" contract without stdio. */
void bz_internal_error(int errcode) {
    (void)errcode;
    abort();
}

/* Clamp a size_t byte count down to what bz_stream's 32-bit avail_in/avail_out can represent in
 * one call. Streaming callers already loop on partial consumption (inSize/outSize can be less
 * than requested), so this just caps a single call's chunk rather than being an error case. */
static unsigned int clampToUInt(size_t value) {
    return (value > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (unsigned int)value;
}

//
// ===== Streaming Compression =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CreateCompressionContext(SZ_BZip2_v1_0_8_CompressionContext* ctx, int32_t blockSize100k, int32_t workFactor) {
    if (!ctx) return BZ_PARAM_ERROR;
    memset(&ctx->strm, 0, sizeof(bz_stream));
    /* bzalloc/bzfree/opaque left NULL - bzip2 falls back to malloc/free internally. verbosity
     * hardcoded to 0 - see the header comment on this function. */
    return BZ2_bzCompressInit(&ctx->strm, blockSize100k, 0, workFactor);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CompressStream(SZ_BZip2_v1_0_8_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t action, int64_t* inSize, int64_t* outSize) {
    /* src may legitimately be NULL when srcSize is 0 - BZ_FLUSH/BZ_FINISH are called once all
     * real input has already been fed, with nothing left to pass (the same idiom as zlib's
     * Z_FINISH with avail_in == 0). Only reject a NULL src if the caller claims bytes are there. */
    if (!ctx || !dst || (!src && srcSize > 0) || !inSize || !outSize) return BZ_PARAM_ERROR;

    unsigned int availIn = clampToUInt(srcSize);
    unsigned int availOut = clampToUInt(dstCapacity);

    ctx->strm.next_in = (char*)src;
    ctx->strm.avail_in = availIn;
    ctx->strm.next_out = (char*)dst;
    ctx->strm.avail_out = availOut;

    int result = BZ2_bzCompress(&ctx->strm, action);

    *inSize = (int64_t)(availIn - ctx->strm.avail_in);
    *outSize = (int64_t)(availOut - ctx->strm.avail_out);

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_FreeCompressionContext(SZ_BZip2_v1_0_8_CompressionContext* ctx) {
    if (!ctx) return BZ_PARAM_ERROR;
    return BZ2_bzCompressEnd(&ctx->strm);
}

//
// ===== Streaming Decompression =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CreateDecompressionContext(SZ_BZip2_v1_0_8_DecompressionContext* ctx, int32_t small) {
    if (!ctx) return BZ_PARAM_ERROR;
    memset(&ctx->strm, 0, sizeof(bz_stream));
    /* verbosity hardcoded to 0 - see SZ_BZip2_v1_0_8_CreateCompressionContext's header comment. */
    return BZ2_bzDecompressInit(&ctx->strm, 0, small);
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_DecompressStream(SZ_BZip2_v1_0_8_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int64_t* inSize, int64_t* outSize) {
    /* src may legitimately be NULL when srcSize is 0 - see SZ_BZip2_v1_0_8_CompressStream's
     * comment on the same relaxation; callers may want to drive decompression forward on
     * previously-buffered internal state without supplying new input. */
    if (!ctx || !dst || (!src && srcSize > 0) || !inSize || !outSize) return BZ_PARAM_ERROR;

    unsigned int availIn = clampToUInt(srcSize);
    unsigned int availOut = clampToUInt(dstCapacity);

    ctx->strm.next_in = (char*)src;
    ctx->strm.avail_in = availIn;
    ctx->strm.next_out = (char*)dst;
    ctx->strm.avail_out = availOut;

    int result = BZ2_bzDecompress(&ctx->strm);

    *inSize = (int64_t)(availIn - ctx->strm.avail_in);
    *outSize = (int64_t)(availOut - ctx->strm.avail_out);

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_FreeDecompressionContext(SZ_BZip2_v1_0_8_DecompressionContext* ctx) {
    if (!ctx) return BZ_PARAM_ERROR;
    return BZ2_bzDecompressEnd(&ctx->strm);
}

//
// ===== Block Compression & Decompression =====
//
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CompressBlock(void* dst, size_t* dstCapacity, const void* src, size_t srcSize, int32_t blockSize100k, int32_t workFactor) {
    if (!dst || !dstCapacity || !src) return BZ_PARAM_ERROR;
    if (srcSize > 0xFFFFFFFFu || *dstCapacity > 0xFFFFFFFFu) return BZ_PARAM_ERROR;

    unsigned int destLen = (unsigned int)*dstCapacity;
    /* verbosity hardcoded to 0 - see SZ_BZip2_v1_0_8_CreateCompressionContext's header comment. */
    int result = BZ2_bzBuffToBuffCompress((char*)dst, &destLen, (char*)src, (unsigned int)srcSize, blockSize100k, 0, workFactor);

    if (result == BZ_OK)
        *dstCapacity = (size_t)destLen;

    return result;
}

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_DecompressBlock(void* dst, size_t* dstCapacity, const void* src, size_t srcSize, int32_t small) {
    if (!dst || !dstCapacity || !src) return BZ_PARAM_ERROR;
    if (srcSize > 0xFFFFFFFFu || *dstCapacity > 0xFFFFFFFFu) return BZ_PARAM_ERROR;

    unsigned int destLen = (unsigned int)*dstCapacity;
    int result = BZ2_bzBuffToBuffDecompress((char*)dst, &destLen, (char*)src, (unsigned int)srcSize, small, 0);

    if (result == BZ_OK)
        *dstCapacity = (size_t)destLen;

    return result;
}
