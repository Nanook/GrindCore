/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

 #ifndef ZSTDMT_v1_5_2_COMPRESS_H
 #define ZSTDMT_v1_5_2_COMPRESS_H

 #if defined (__cplusplus)
 extern "C" {
 #endif


/* Note : This is an internal API.
 *        These APIs used to be exposed with ZSTDLIB_API,
 *        because it used to be the only way to invoke MT compression.
 *        Now, you must use ZSTD_v1_5_2_compress2 and ZSTD_v1_5_2_compressStream2() instead.
 *
 *        This API requires ZSTD_v1_5_2_MULTITHREAD to be defined during compilation,
 *        otherwise ZSTDMT_createCCtx*() will fail.
 */

/* ===   Dependencies   === */
#include "../common/zstd_deps.h"   /* size_t */
#define ZSTD_v1_5_2_STATIC_LINKING_ONLY   /* ZSTD_v1_5_2_parameters */
#include "../zstd.h"            /* ZSTD_v1_5_2_inBuffer, ZSTD_v1_5_2_outBuffer, ZSTDLIB_API */


/* ===   Constants   === */
#ifndef ZSTDMT_v1_5_2_NBWORKERS_MAX /* a different value can be selected at compile time */
#  define ZSTDMT_v1_5_2_NBWORKERS_MAX ((sizeof(void*)==4) /*32-bit*/ ? 64 : 256)
#endif
#ifndef ZSTDMT_v1_5_2_JOBSIZE_MIN   /* a different value can be selected at compile time */
#  define ZSTDMT_v1_5_2_JOBSIZE_MIN (512 KB)
#endif
#define ZSTDMT_v1_5_2_JOBLOG_MAX   (MEM_32bits() ? 29 : 30)
#define ZSTDMT_v1_5_2_JOBSIZE_MAX  (MEM_32bits() ? (512 MB) : (1024 MB))


/* ========================================================
 * ===  Private interface, for use by ZSTD_compress.c   ===
 * ===  Not exposed in libzstd. Never invoke directly   ===
 * ======================================================== */

/* ===   Memory management   === */
typedef struct ZSTDMT_v1_5_2_CCtx_s ZSTDMT_v1_5_2_CCtx;
/* Requires ZSTD_v1_5_2_MULTITHREAD to be defined during compilation, otherwise it will return NULL. */
ZSTDMT_v1_5_2_CCtx* ZSTDMT_v1_5_2_createCCtx_advanced(unsigned nbWorkers,
                                        ZSTD_v1_5_2_customMem cMem,
					ZSTD_v1_5_2_threadPool *pool);
size_t ZSTDMT_v1_5_2_freeCCtx(ZSTDMT_v1_5_2_CCtx* mtctx);

size_t ZSTDMT_v1_5_2_sizeof_CCtx(ZSTDMT_v1_5_2_CCtx* mtctx);

/* ===   Streaming functions   === */

size_t ZSTDMT_v1_5_2_nextInputSizeHint(const ZSTDMT_v1_5_2_CCtx* mtctx);

/*! ZSTDMT_v1_5_2_initCStream_internal() :
 *  Private use only. Init streaming operation.
 *  expects params to be valid.
 *  must receive dict, or cdict, or none, but not both.
 *  mtctx can be freshly constructed or reused from a prior compression.
 *  If mtctx is reused, memory allocations from the prior compression may not be freed,
 *  even if they are not needed for the current compression.
 *  @return : 0, or an error code */
size_t ZSTDMT_v1_5_2_initCStream_internal(ZSTDMT_v1_5_2_CCtx* mtctx,
                    const void* dict, size_t dictSize, ZSTD_v1_5_2_dictContentType_e dictContentType,
                    const ZSTD_v1_5_2_CDict* cdict,
                    ZSTD_v1_5_2_CCtx_params params, unsigned long long pledgedSrcSize);

/*! ZSTDMT_v1_5_2_compressStream_generic() :
 *  Combines ZSTDMT_compressStream() with optional ZSTDMT_flushStream() or ZSTDMT_endStream()
 *  depending on flush directive.
 * @return : minimum amount of data still to be flushed
 *           0 if fully flushed
 *           or an error code
 *  note : needs to be init using any ZSTD_v1_5_2_initCStream*() variant */
size_t ZSTDMT_v1_5_2_compressStream_generic(ZSTDMT_v1_5_2_CCtx* mtctx,
                                     ZSTD_v1_5_2_outBuffer* output,
                                     ZSTD_v1_5_2_inBuffer* input,
                                     ZSTD_v1_5_2_EndDirective endOp);

 /*! ZSTDMT_v1_5_2_toFlushNow()
  *  Tell how many bytes are ready to be flushed immediately.
  *  Probe the oldest active job (not yet entirely flushed) and check its output buffer.
  *  If return 0, it means there is no active job,
  *  or, it means oldest job is still active, but everything produced has been flushed so far,
  *  therefore flushing is limited by speed of oldest job. */
size_t ZSTDMT_v1_5_2_toFlushNow(ZSTDMT_v1_5_2_CCtx* mtctx);

/*! ZSTDMT_v1_5_2_updateCParams_whileCompressing() :
 *  Updates only a selected set of compression parameters, to remain compatible with current frame.
 *  New parameters will be applied to next compression job. */
void ZSTDMT_v1_5_2_updateCParams_whileCompressing(ZSTDMT_v1_5_2_CCtx* mtctx, const ZSTD_v1_5_2_CCtx_params* cctxParams);

/*! ZSTDMT_v1_5_2_getFrameProgression():
 *  tells how much data has been consumed (input) and produced (output) for current frame.
 *  able to count progression inside worker threads.
 */
ZSTD_v1_5_2_frameProgression ZSTDMT_v1_5_2_getFrameProgression(ZSTDMT_v1_5_2_CCtx* mtctx);


#if defined (__cplusplus)
}
#endif

#endif   /* ZSTDMT_v1_5_2_COMPRESS_H */
