#ifndef PAL_BZIP2_V1_0_8_H
#define PAL_BZIP2_V1_0_8_H

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

/* We only need the core/utility API - strip bzip2's optional FILE*-based high-level API
 * (also defined via BZ_NO_STDIO on the bzip2_v1_0_8 CMake target itself, kept here too so
 * this header is self-consistent even if included outside that target's build). */
#ifndef BZ_NO_STDIO
#define BZ_NO_STDIO
#endif
#include "external/bzip2/bzip2/bzlib.h"
#include <stddef.h>

/* Streaming Compression/Decompression Context Management.
 * bz_stream is deliberately shaped like zlib's z_stream (next_in/avail_in/next_out/avail_out),
 * so - unlike zstd's opaque ZSTD_CCtx / ZSTD_DCtx pointer handles - it's a real value type we
 * own and embed directly, following the same "PAL owns the native struct as a value" ownership style
 * as pal_zlib_v1_3_1's PAL_ZStream, but exposing bzip2's own bz_stream fields directly rather
 * than mirroring them into a second parallel struct, since bzip2's own struct is already a
 * stable public ABI (bzlib.h), not an internal implementation detail that needs to be hidden. */
typedef struct {
    bz_stream strm;
} SZ_BZip2_v1_0_8_CompressionContext;

typedef struct {
    bz_stream strm;
} SZ_BZip2_v1_0_8_DecompressionContext;

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Streaming Compression ===== */

/* Initializes ctx and calls BZ2_bzCompressInit. blockSize100k: 1-9 (900k*n block size, larger =
 * better ratio/more memory). workFactor: 0-250 (0 = bzip2's default 30; higher trades speed for
 * resilience against pathological/repetitive input). No verbosity parameter - bzip2's verbosity
 * output goes entirely through VPrintf0-5, which are no-ops under BZ_NO_STDIO (see below), so
 * it's hardcoded to 0 internally rather than exposing a parameter that would silently do
 * nothing. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CreateCompressionContext(SZ_BZip2_v1_0_8_CompressionContext* ctx, int32_t blockSize100k, int32_t workFactor);

/* Feeds up to srcSize bytes from src and writes up to dstCapacity bytes to dst, driving one
 * BZ2_bzCompress(action) call. action: BZ_RUN (0) while more input may follow, BZ_FINISH (2) to
 * flush and terminate the stream (bzip2 has no separate "flush without finishing" concept the
 * way zstd does - BZ_FLUSH exists but is for the rarely-needed case of finishing a block boundary
 * without ending the stream). inSize and outSize report bytes actually consumed/produced this call
 * (bzip2 may not consume/produce everything requested in one call - callers must loop). Returns
 * bzip2's own result code (BZ_RUN_OK/BZ_FINISH_OK/BZ_STREAM_END, or a negative BZ_*_ERROR code). */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CompressStream(SZ_BZip2_v1_0_8_CompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int32_t action, int64_t* inSize, int64_t* outSize);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_FreeCompressionContext(SZ_BZip2_v1_0_8_CompressionContext* ctx);

/* ===== Streaming Decompression ===== */

/* small: nonzero selects bzip2's reduced-memory decompression algorithm (slower, ~2.5x less
 * memory) - matches BZ2_bzDecompressInit's own "small" parameter name/meaning exactly. No
 * verbosity parameter - see SZ_BZip2_v1_0_8_CreateCompressionContext. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CreateDecompressionContext(SZ_BZip2_v1_0_8_DecompressionContext* ctx, int32_t small);

/* Same in/out-size-reporting shape as SZ_BZip2_v1_0_8_CompressStream. No action parameter -
 * BZ2_bzDecompress takes none; it returns BZ_STREAM_END once the compressed stream is fully
 * consumed. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_DecompressStream(SZ_BZip2_v1_0_8_DecompressionContext* ctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, int64_t* inSize, int64_t* outSize);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_FreeDecompressionContext(SZ_BZip2_v1_0_8_DecompressionContext* ctx);

/* ===== Block Compression & Decompression (one-shot, no persistent context) ===== */

/* *dstCapacity is the destination buffer's size on entry, the actual compressed size on success.
 * Guards srcSize and dstCapacity against bzip2's own unsigned-int (32-bit) length limit explicitly -
 * BZ2_bzBuffToBuffCompress silently truncates rather than erroring on inputs that large. No
 * verbosity parameter - see SZ_BZip2_v1_0_8_CreateCompressionContext. */
FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_CompressBlock(void* dst, size_t* dstCapacity, const void* src, size_t srcSize, int32_t blockSize100k, int32_t workFactor);

FUNCTIONEXPORT int32_t FUNCTIONCALLINGCONVENCTION SZ_BZip2_v1_0_8_DecompressBlock(void* dst, size_t* dstCapacity, const void* src, size_t srcSize, int32_t small);

#ifdef __cplusplus
}
#endif

#endif /* PAL_BZIP2_V1_0_8_H */
