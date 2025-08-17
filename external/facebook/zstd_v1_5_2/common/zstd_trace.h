/*
 * Copyright (c) Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_v1_5_2_TRACE_H
#define ZSTD_v1_5_2_TRACE_H

#if defined (__cplusplus)
extern "C" {
#endif

#include <stddef.h>

/* weak symbol support
 * For now, enable conservatively:
 * - Only GNUC
 * - Only ELF
 * - Only x86-64 and i386
 * Also, explicitly disable on platforms known not to work so they aren't
 * forgotten in the future.
 */
#if !defined(ZSTD_v1_5_2_HAVE_WEAK_SYMBOLS) && \
    defined(__GNUC__) && defined(__ELF__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && \
    !defined(__APPLE__) && !defined(_WIN32) && !defined(__MINGW32__) && \
    !defined(__CYGWIN__) && !defined(_AIX)
#  define ZSTD_v1_5_2_HAVE_WEAK_SYMBOLS 1
#else
#  define ZSTD_v1_5_2_HAVE_WEAK_SYMBOLS 0
#endif
#if ZSTD_v1_5_2_HAVE_WEAK_SYMBOLS
#  define ZSTD_v1_5_2_WEAK_ATTR __attribute__((__weak__))
#else
#  define ZSTD_v1_5_2_WEAK_ATTR
#endif

/* Only enable tracing when weak symbols are available. */
#ifndef ZSTD_v1_5_2_TRACE
#  define ZSTD_v1_5_2_TRACE ZSTD_v1_5_2_HAVE_WEAK_SYMBOLS
#endif

#if ZSTD_v1_5_2_TRACE

struct ZSTD_v1_5_2_CCtx_s;
struct ZSTD_v1_5_2_DCtx_s;
struct ZSTD_v1_5_2_CCtx_params_s;

typedef struct {
    /**
     * ZSTD_v1_5_2_VERSION_NUMBER
     *
     * This is guaranteed to be the first member of ZSTD_v1_5_2_trace.
     * Otherwise, this struct is not stable between versions. If
     * the version number does not match your expectation, you
     * should not interpret the rest of the struct.
     */
    unsigned version;
    /**
     * Non-zero if streaming (de)compression is used.
     */
    unsigned streaming;
    /**
     * The dictionary ID.
     */
    unsigned dictionaryID;
    /**
     * Is the dictionary cold?
     * Only set on decompression.
     */
    unsigned dictionaryIsCold;
    /**
     * The dictionary size or zero if no dictionary.
     */
    size_t dictionarySize;
    /**
     * The uncompressed size of the data.
     */
    size_t uncompressedSize;
    /**
     * The compressed size of the data.
     */
    size_t compressedSize;
    /**
     * The fully resolved CCtx parameters (NULL on decompression).
     */
    struct ZSTD_v1_5_2_CCtx_params_s const* params;
    /**
     * The ZSTD_v1_5_2_CCtx pointer (NULL on decompression).
     */
    struct ZSTD_v1_5_2_CCtx_s const* cctx;
    /**
     * The ZSTD_v1_5_2_DCtx pointer (NULL on compression).
     */
    struct ZSTD_v1_5_2_DCtx_s const* dctx;
} ZSTD_v1_5_2_Trace;

/**
 * A tracing context. It must be 0 when tracing is disabled.
 * Otherwise, any non-zero value returned by a tracing begin()
 * function is presented to any subsequent calls to end().
 *
 * Any non-zero value is treated as tracing is enabled and not
 * interpreted by the library.
 *
 * Two possible uses are:
 * * A timestamp for when the begin() function was called.
 * * A unique key identifying the (de)compression, like the
 *   address of the [dc]ctx pointer if you need to track
 *   more information than just a timestamp.
 */
typedef unsigned long long ZSTD_v1_5_2_TraceCtx;

/**
 * Trace the beginning of a compression call.
 * @param cctx The dctx pointer for the compression.
 *             It can be used as a key to map begin() to end().
 * @returns Non-zero if tracing is enabled. The return value is
 *          passed to ZSTD_v1_5_2_trace_compress_end().
 */
ZSTD_v1_5_2_WEAK_ATTR ZSTD_v1_5_2_TraceCtx ZSTD_v1_5_2_trace_compress_begin(
    struct ZSTD_v1_5_2_CCtx_s const* cctx);

/**
 * Trace the end of a compression call.
 * @param ctx The return value of ZSTD_v1_5_2_trace_compress_begin().
 * @param trace The zstd tracing info.
 */
ZSTD_v1_5_2_WEAK_ATTR void ZSTD_v1_5_2_trace_compress_end(
    ZSTD_v1_5_2_TraceCtx ctx,
    ZSTD_v1_5_2_Trace const* trace);

/**
 * Trace the beginning of a decompression call.
 * @param dctx The dctx pointer for the decompression.
 *             It can be used as a key to map begin() to end().
 * @returns Non-zero if tracing is enabled. The return value is
 *          passed to ZSTD_v1_5_2_trace_compress_end().
 */
ZSTD_v1_5_2_WEAK_ATTR ZSTD_v1_5_2_TraceCtx ZSTD_v1_5_2_trace_decompress_begin(
    struct ZSTD_v1_5_2_DCtx_s const* dctx);

/**
 * Trace the end of a decompression call.
 * @param ctx The return value of ZSTD_v1_5_2_trace_decompress_begin().
 * @param trace The zstd tracing info.
 */
ZSTD_v1_5_2_WEAK_ATTR void ZSTD_v1_5_2_trace_decompress_end(
    ZSTD_v1_5_2_TraceCtx ctx,
    ZSTD_v1_5_2_Trace const* trace);

#endif /* ZSTD_v1_5_2_TRACE */

#if defined (__cplusplus)
}
#endif

#endif /* ZSTD_v1_5_2_TRACE_H */
